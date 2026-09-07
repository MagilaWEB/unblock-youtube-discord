#include "engine.h"
#include "version.hpp"

#include "../ui/ui.h"

#include <saucer/modules/stable/webview2.hpp>

#include <cwctype>
#include <algorithm>
#include <cmath>
#include <fstream>

#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>

#include <filesystem>

Engine::Engine()
{
	auto created = saucer::application::create(
		{
			.id							= "com.unblock",
			.argc						= std::nullopt,
			.argv						= std::nullopt,
			.quit_on_last_window_closed = true,
		}
	);
	ASSERT(created.has_value());
	if (created.has_value())
		_app.emplace(std::move(*created));
}

Engine::~Engine() noexcept
{
	try
	{
		_finish();
	}
	catch (...)
	{
		// Destructors must not throw; swallowing the exception keeps the
		// fstream close() (hideConsole) from escaping during shutdown.
		// Debug::error may throw (format_error), so guard it as well.
		try
		{
			Debug::error("Exception thrown during Engine shutdown");
		}
		catch (...)	   // NOLINT(bugprone-empty-catch) - swallowing on purpose: a noexcept destructor must not propagate anything.
		{
		}
	}
}

Engine& Engine::get()
{
	static Engine instance;
	return instance;
}

void Engine::initialize()
{
	Localization::get().set(_getSystemLocale());

	if (_checkRunApp())
	{
		Debug::winApiWindowShow("str_warning", "str_warning_copy_application_running");
		return;
	}

	_file_user_setting = std::make_shared<File>(false);
	_file_user_setting->open({ Core::get().userPath() / "setting" }, ".config", true);

#ifdef DEBUG
	showConsole();
#else
	auto result = _file_user_setting->parameterSection<bool>("SYSTEM", "show_console");
	if (result && result.value())
		showConsole();
#endif

	Debug::initLogFile();
	Debug::setVersion(VERSION_STR);
}

void Engine::run()
{
	if (_checkRunApp())
		return;

	// Scheme registration is mandatory before creating the webview (WebView2 stage).
	saucer::webview::register_scheme("ui");

	if (_app)
		_app->run([this](saucer::application* app) -> coco::stray { return _start(app); });

	_finish();
}

namespace
{
	// saucer's screens() returns raw rcMonitor with x/y swapped, so work areas
	// are enumerated directly via Win32 (rcWork excludes the taskbar).
	BOOL CALLBACK collectWorkArea(HMONITOR monitor, HDC, LPRECT, LPARAM user_data)
	{
		auto* out = reinterpret_cast<std::vector<window_geometry::Area>*>(user_data);

		MONITORINFO info{};
		info.cbSize = sizeof(info);
		if (!GetMonitorInfoW(monitor, &info))
			return TRUE;

		unsigned dpi{ 96 };
		if (UINT dx{ 0 }, dy{ 0 }; SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dx, &dy)) && dx != 0)
			dpi = dx;

		out->push_back(
			{
				.x	 = static_cast<int>(info.rcWork.left),
				.y	 = static_cast<int>(info.rcWork.top),
				.w	 = static_cast<int>(info.rcWork.right - info.rcWork.left),
				.h	 = static_cast<int>(info.rcWork.bottom - info.rcWork.top),
				.dpi = dpi,
			}
		);
		return TRUE;
	}

	std::vector<window_geometry::Area> listWorkAreas()
	{
		std::vector<window_geometry::Area> areas;
		EnumDisplayMonitors(nullptr, nullptr, collectWorkArea, reinterpret_cast<LPARAM>(&areas));
		return areas;
	}

	// Index of the work area containing the cursor; falls back to primary.
	size_t cursorAreaIndex(const std::vector<window_geometry::Area>& areas)
	{
		POINT cursor{};
		if (GetCursorPos(&cursor))
		{
			for (size_t i = 0; i < areas.size(); ++i)
			{
				const auto& a = areas[i];
				if (cursor.x >= a.x && cursor.x < a.x + a.w && cursor.y >= a.y && cursor.y < a.y + a.h)
					return i;
			}
		}
		return 0;
	}
}	 // namespace

// clang-tidy resolves coco::stray's promise_type through the coroutine instance below;
// that's an artifact of the coroutine machinery, not a real instance-access bug.
// NOLINTNEXTLINE(readability-static-accessed-through-instance)
coco::stray Engine::_start(saucer::application* app)
{
	auto window = saucer::window::create(app);
	if (!window)
	{
		Debug::error("Failed to create window: {}", window.error());
		app->quit();
		// NOLINTNEXTLINE(readability-static-accessed-through-instance) - coroutine promise_type artifact, see _start().
		co_return;
	}
	_window = std::move(window).value();

	// Saucer works in logical pixels (96 DPI base) and scales to physical
	// itself per-monitor, so no manual GetSystemMetrics scaling here.
	_window->set_min_size({ window_geometry::kMinWidth, window_geometry::kMinHeight });
	_window->set_resizable(true);
	_window->set_background({ .r = 13, .g = 14, .b = 20, .a = 255 });
	_window->set_title(("Unblock " + std::format("Version: {}", VERSION_STR)).c_str());

	// Icon via saucer (big icon). One Win32 line complements the small
	// title-bar icon — saucer's Win32 backend only sends ICON_BIG upstream.
	const auto hwnd = _window->native().hwnd;
	{
		// NB: unblock.ico lives in bin/, not in currentPath() (project root).
		const auto icon_path = Core::get().binPath() / "unblock.ico";
		if (auto icon = saucer::icon::from(icon_path); icon)
		{
			_window->set_icon(*icon);
			if (HICON small_icon = static_cast<HICON>(
					LoadImageW(nullptr, icon_path.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE)
				))
				SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
		}
		else
			_forceSetWindowIcon(hwnd, L"./unblock.ico");
	}

	_restoreWindowGeometry();
	_installMoveHook();

	// No saucer API for the frame chrome; applied once after show() below
	// so DWM does not fight the compositor during initial placement.
	_applyDarkTitleBar(hwnd);

	// Smartview over the window (move-only -> optional).
	auto view = saucer::smartview::create(
		{
			.window				   = _window,
			.attributes			   = true,
			.persistent_cookies	   = true,
			.hardware_acceleration = true,
			.storage_path		   = std::nullopt,
			.user_agent			   = std::nullopt,
			.browser_flags		   = {},
		}
	);
	if (!view)
	{
		Debug::error("Failed to create webview: {}", view.error());
		app->quit();
		// NOLINTNEXTLINE(readability-static-accessed-through-instance) - coroutine promise_type artifact, see _start().
		co_return;
	}
	_view.emplace(std::move(*view));

	saucer::smartview* const webview = &*_view;
	_setupScheme(*webview);

	// Dev tools only in Debug builds; disabled in Release.
#ifdef _DEBUG
	webview->set_dev_tools(true);
#endif

	// Context menu does not affect visuals — disabled in both builds.
	webview->set_context_menu(false);

	_ui = std::make_shared<Ui>(this);
	_ui->postConstruct();

	// Bridge + window subscriptions (before navigation).
	_ui->setup(webview);

	webview->set_url("ui://root/main.html");
	_window->show();
	// Second geometry pass: by now WM_DPICHANGED has synced saucer's DPI
	// cache to the actual monitor, so the same logical values produce the
	// correct physical size. Runs before the loop pumps paint — no flicker.
	_reapplyWindowGeometry();

	_startUpdateTicker(app);

	co_await app->finish();
}

void Engine::_setupScheme(saucer::smartview& view)
{
	// Serve interface files from the "ui" folder via the ui://root/... scheme.
	const std::filesystem::path ui_root = Core::get().currentPath() / "ui";

	view.handle_scheme(
		"ui",
		// saucer wraps handlers in noexcept-expected callable/transformer templates, so any
		// allocation inside the lambda looks like an escaping exception to bugprone-exception-escape.
		// NOLINTNEXTLINE(bugprone-exception-escape)
		[ui_root](const saucer::scheme::request& request, saucer::scheme::executor exec)
		{
			saucer::fs::path file_path = ui_root / request.url().path();

			// path() returns the path with a leading '/' (root-directory) which drops the base.
			if (file_path.has_root_directory() && file_path.has_relative_path())
			{
				saucer::fs::path rel{ file_path.relative_path() };
				file_path = ui_root / rel;
			}

			if (file_path.extension().empty())
				file_path += ".html";

			std::ifstream stream(file_path, std::ios::binary);
			if (!stream)
			{
				exec.reject(saucer::scheme::error::not_found);
				return;
			}

			std::vector<std::uint8_t> data{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

			exec.resolve(
				saucer::scheme::response{
					.data	 = saucer::stash::from(std::move(data)),
					.mime	 = "text/html",
					.headers = {},
					.status	 = 200,
				}
			);
		}
	);
}

saucer::smartview* Engine::webview()
{
	return _view ? &*_view : nullptr;
}

std::shared_ptr<saucer::window> Engine::window()
{
	return _window;
}

void Engine::showConsole()
{
	if (_consoleInput.is_open())
		return;

	ASSERT(AllocConsole());
	AttachConsole(ATTACH_PARENT_PROCESS);

	freopen_s(&_fp_console, "CONIN$", "r", stdin);
	freopen_s(&_fp_console, "CONOUT$", "w", stdout);
	freopen_s(&_fp_console, "CONOUT$", "w", stderr);

	_cinBuffer	= std::cin.rdbuf();
	_coutBuffer = std::cout.rdbuf();
	_cerrBuffer = std::cerr.rdbuf();

	_consoleInput.open("CONIN$", std::ios::in);
	_consoleOutput.open("CONOUT$", std::ios::out);
	_consoleError.open("CONOUT$", std::ios::out);

	std::cin.rdbuf(std::cin.rdbuf());
	std::cout.rdbuf(std::cout.rdbuf());
	std::cerr.rdbuf(std::cerr.rdbuf());

	std::ios::sync_with_stdio(true);

	_hwnd_console = GetConsoleWindow();

	if (auto handle_out = GetStdHandle(STD_OUTPUT_HANDLE))
	{
		DWORD dwMode{ 0 };
		GetConsoleMode(handle_out, &dwMode);
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(handle_out, dwMode);
	}

	EnableMenuItem(GetSystemMenu(_hwnd_console, FALSE), SC_CLOSE, MF_GRAYED);

	// Set UTF-8
	SetConsoleTitleW(L"Unblock Console");
	SetConsoleCP(65'001);
	SetConsoleOutputCP(65'001);
}

void Engine::hideConsole()
{
	if (FreeConsole())
	{
		_consoleInput.close();
		_consoleOutput.close();
		_consoleError.close();

		std::cin.rdbuf(_cinBuffer);
		std::cout.rdbuf(_coutBuffer);
		std::cerr.rdbuf(_cerrBuffer);

		_cinBuffer	= nullptr;
		_coutBuffer = nullptr;
		_cerrBuffer = nullptr;

		std::ios::sync_with_stdio(false);

		PostMessage(_hwnd_console, WM_CLOSE, 0, 0);
	}
}

std::shared_ptr<File>& Engine::userConfig()
{
	return _file_user_setting;
}

void Engine::quit()
{
	// Loop is still alive here (quit() is what ends it) — safe to persist.
	_flushWindowGeometry();
	if (_app)
		_app->quit();
}

bool Engine::hasCyrillicOrSpaceInBinaryPath()
{
	const auto is_cyrillic_or_space = [](wchar_t ch)
	{
		if (std::iswspace(ch))
			return true;

		return (ch >= 0x04'00 && ch <= 0x04'FF) || (ch >= 0x05'00 && ch <= 0x05'2F);
	};

	return std::ranges::any_of(Core::get().binariesPath().wstring(), is_cyrillic_or_space);
}

void Engine::_installMoveHook()
{
	if (!_window || _prev_wndproc)
		return;

	// Manual subclass (no comctl32 dependency): saucer already hooked the
	// window at creation, so chain to whatever proc is current.
	_prev_wndproc =
		reinterpret_cast<WNDPROC>(SetWindowLongPtrW(_window->native().hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Engine::_windowHookProc)));
}

// NOLINTNEXTLINE(bugprone-exception-escape) - Win32 proc; exceptions must not cross it, and the body below does not throw.
LRESULT CALLBACK Engine::_windowHookProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	Engine& self = Engine::get();

	if (msg == WM_EXITSIZEMOVE)
		self._onSizeMoveEnd();

	if (self._prev_wndproc)
		return CallWindowProcW(self._prev_wndproc, hwnd, msg, w_param, l_param);
	return DefWindowProcW(hwnd, msg, w_param, l_param);
}

void Engine::_removeMoveHook()
{
	// Restore saucer's pristine window-proc chain first thing at teardown:
	// our proc must not sit in the path of DestroyWindow and friends.
	if (_window && _prev_wndproc)
	{
		SetWindowLongPtrW(_window->native().hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_prev_wndproc));
		_prev_wndproc = nullptr;
	}
}

void Engine::flushWindowGeometry()
{
	_flushWindowGeometry();
}

void Engine::_onSizeMoveEnd()
{
	// Persist the final position. Pure moves are reported by nothing else
	// (saucer has resize but no move event). No size roundtrip here: saucer
	// owns sizing across the transition, and a logical roundtrip would only
	// lose pixels to float truncation on fractional DPIs.
	markWindowGeometryDirty();
}

void Engine::_restoreWindowGeometry()
{
	const auto areas = listWorkAreas();
	if (areas.empty())
	{
		_window->set_size({ window_geometry::kBaseWidth, window_geometry::kBaseHeight });
		return;
	}

	// Work in logical units throughout; saucer size() is logical while its
	// position() is physical, so areas are converted once up front.
	std::vector<window_geometry::Area> logical;
	logical.reserve(areas.size());
	for (const auto& area : areas)
		logical.push_back(window_geometry::logicalArea(area));

	const auto& cursor_area = logical[cursorAreaIndex(areas)];

	// Stored x/y/w/h are logical. v3 additionally stores the DPI they were
	// saved with, so reopening on a different-DPI monitor keeps the physical
	// size instead of shrinking. Older entries lack the DPI — their size is
	// kept as-is (same-monitor assumption) or recentered when unusable.
	const auto saved_v	 = _file_user_setting->parameterSection<u32>("WINDOW", "v");
	const auto saved_w	 = _file_user_setting->parameterSection<u32>("WINDOW", "width");
	const auto saved_h	 = _file_user_setting->parameterSection<u32>("WINDOW", "height");
	const auto saved_x	 = _file_user_setting->parameterSection<s32>("WINDOW", "x");
	const auto saved_y	 = _file_user_setting->parameterSection<s32>("WINDOW", "y");
	const auto saved_dpi = _file_user_setting->parameterSection<u32>("WINDOW", "dpi");

	window_geometry::Geometry geo			 = window_geometry::defaultGeometry(cursor_area);
	bool					  use_saved_size = false;
	// Legacy entries carry physical x/y (wrong units) but their logical w/h
	// is still valid — keep the size, recenter once.
	if ((!saved_v || saved_v.value() < 2) && saved_w && saved_h)
	{
		geo.w = std::max(static_cast<int>(saved_w.value()), window_geometry::kMinWidth);
		geo.h = std::max(static_cast<int>(saved_h.value()), window_geometry::kMinHeight);
		geo.x = cursor_area.x + (cursor_area.w - geo.w) / 2;
		geo.y = cursor_area.y + (cursor_area.h - geo.h) / 2;
	}
	else if (saved_v && saved_v.value() >= 2 && saved_w && saved_h && saved_x && saved_y)
	{
		geo = {
			.x = saved_x.value(),
			.y = saved_y.value(),
			.w = static_cast<int>(saved_w.value()),
			.h = static_cast<int>(saved_h.value()),
		};
		use_saved_size = true;
		if (!window_geometry::isVisibleAnywhere(geo, logical))
			geo = window_geometry::defaultGeometry(cursor_area);
	}

	// Pull into view on the area holding the top-left corner (or the cursor
	// area when centered). The size is kept as closed — never shrunk to fit.
	const window_geometry::Area* target = &cursor_area;
	for (const auto& area : logical)
	{
		if (geo.x >= area.x && geo.x < area.x + area.w && geo.y >= area.y && geo.y < area.y + area.h)
		{
			target = &area;
			break;
		}
	}
	// v3: the saved size is logical for the saved monitor's DPI — scale it
	// to the target monitor so the window keeps its physical size there.
	if (use_saved_size && saved_dpi && saved_dpi.value() != 0 && target->dpi != 0 && saved_dpi.value() != target->dpi)
	{
		const double ratio = static_cast<double>(saved_dpi.value()) / target->dpi;
		geo.w			   = std::max(static_cast<int>(std::lround(geo.w * ratio)), window_geometry::kMinWidth);
		geo.h			   = std::max(static_cast<int>(std::lround(geo.h * ratio)), window_geometry::kMinHeight);
	}

	geo = window_geometry::clampPositionToArea(geo, *target);

	// Stash for the post-show re-apply (saucer DPI cache syncs on show).
	_restored_geo  = geo;
	_restored_dpi  = target->dpi;
	_have_restored = true;

	_window->set_size({ geo.w, geo.h });
	// set_position takes physical pixels: convert back with the target DPI.
	_window->set_position({ .x = window_geometry::toPhysical(geo.x, target->dpi), .y = window_geometry::toPhysical(geo.y, target->dpi) });
}

void Engine::_reapplyWindowGeometry()
{
	if (!_have_restored || !_window)
		return;

	_window->set_size({ _restored_geo.w, _restored_geo.h });
	_window->set_position(
		{ .x = window_geometry::toPhysical(_restored_geo.x, _restored_dpi), .y = window_geometry::toPhysical(_restored_geo.y, _restored_dpi) }
	);
}

void Engine::markWindowGeometryDirty()
{
	std::lock_guard lock{ _geom_mutex };
	_geom_dirty		  = true;
	_geom_dirty_since = std::chrono::steady_clock::now();
}

void Engine::_maybeFlushWindowGeometry()
{
	{
		std::lock_guard lock{ _geom_mutex };
		if (!_geom_dirty || std::chrono::steady_clock::now() - _geom_dirty_since < kGeomFlushDelay)
			return;
		_geom_dirty = false;
	}
	_flushWindowGeometry();
}

void Engine::_flushWindowGeometry()
{
	try
	{
		if (!_window || !_file_user_setting)
			return;
		// Minimized windows report a degenerate size; never persist that.
		if (_window->minimized())
			return;

		const auto size = _window->size();
		if (size.w < window_geometry::kMinWidth || size.h < window_geometry::kMinHeight)
			return;

		// position() is physical; persist logical so restore math is DPI-clean.
		const HWND	   hwnd		 = _window->native().hwnd;
		const unsigned dpi		 = GetDpiForWindow(hwnd);
		const auto	   pos		 = _window->position();
		const int	   logical_x = window_geometry::toLogical(pos.x, dpi);
		const int	   logical_y = window_geometry::toLogical(pos.y, dpi);

		_file_user_setting->writeSectionParameter("WINDOW", "v", "3");
		_file_user_setting->writeSectionParameter("WINDOW", "dpi", std::to_string(dpi));
		_file_user_setting->writeSectionParameter("WINDOW", "width", std::to_string(size.w));
		_file_user_setting->writeSectionParameter("WINDOW", "height", std::to_string(size.h));
		_file_user_setting->writeSectionParameter("WINDOW", "x", std::to_string(logical_x));
		_file_user_setting->writeSectionParameter("WINDOW", "y", std::to_string(logical_y));
		_file_user_setting->save();
	}
	catch (...)
	{
		// Geometry persistence must never break the UI thread or shutdown.
	}
}

bool Engine::_checkRunApp()
{
	static HANDLE mutex{ CreateMutexW(nullptr, true, L"MutexOfTheUnblockApplication") };
	static bool	  app_run{ WaitForSingleObject(mutex, 0) != WAIT_OBJECT_0 };
	return app_run;
}

void Engine::_startUpdateTicker(saucer::application* app)
{
	if (_update_ticker_run)
		return;

	_update_ticker_run = true;
	_update_ticker	   = std::jthread(
		[this, app](std::stop_token token)
		{
			using namespace std::chrono;
			while (!token.stop_requested())
			{
				std::this_thread::sleep_for(30ms);
				if (token.stop_requested() || !_update_ticker_run)
					break;

				// Post to the main thread (msg loop) — the same thread that owns JS/WebView2.
				app->post(
					[this]()
					{
						if (!_update_ticker_run)
							return;
						_maybeFlushWindowGeometry();
						// No DOM sync for a minimized window: WebView2 has no
						// visible surface and every execute() is wasted work.
						if (_ui && !(_window && _window->minimized()))
							_ui->update();
					}
				);
			}
		}
	);
}

void Engine::_stopUpdateTicker()
{
	_update_ticker_run = false;

	if (_update_ticker.joinable())
		_update_ticker.request_stop();

	if (_update_ticker.joinable())
		_update_ticker.join();
}

void Engine::_finish()
{
	_stopUpdateTicker();
	// No saucer calls below this point: the message loop is dead and our
	// hook is uninstalled first, so teardown follows the pristine path.
	_removeMoveHook();
	hideConsole();
	_ui.reset();
	_view.reset();
	_window.reset();
	_app.reset();
}

std::string Engine::_getSystemLocale()
{
	std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> buffer{};
	int											chars = GetUserDefaultLocaleName(buffer.data(), static_cast<int>(buffer.size()));
	if (chars == 0)
		return "US";

	int			size_needed = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), chars - 1, nullptr, 0, nullptr, nullptr);
	std::string result(static_cast<size_t>(size_needed), 0);

	WideCharToMultiByte(CP_UTF8, 0, buffer.data(), chars - 1, result.data(), size_needed, nullptr, nullptr);
	return result.substr(result.find_first_of('-') + 1, result.length());
}

void Engine::_forceSetWindowIcon(HWND hwnd, const wchar_t* iconPath)
{
	HICON hIconBig =
		static_cast<HICON>(LoadImageW(nullptr, iconPath, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE));
	HICON hIconSmall =
		static_cast<HICON>(LoadImageW(nullptr, iconPath, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));

	if (!hIconBig || !hIconSmall)
	{
		hIconBig   = static_cast<HICON>(LoadImageW(nullptr, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
		hIconSmall = hIconBig;
	}

	SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconBig));
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));

	SetClassLongPtr(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hIconBig));
	SetClassLongPtr(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(hIconSmall));
}

void Engine::_applyDarkTitleBar(HWND hwnd)
{
	BOOL useDarkMode = TRUE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}
