#include "engine.h"
#include "version.hpp"

#include <saucer/modules/stable/webview2.hpp>

#include <cwctype>
#include <algorithm>
#include <fstream>

#include <dwmapi.h>
#include <shellapi.h>

#include <filesystem>

Engine::Engine()
{
	auto created = saucer::application::create(
		{
			.id = "com.unblock",
			.argc = std::nullopt,
			.argv = std::nullopt,
			.quit_on_last_window_closed = true,
		}
	);
	ASSERT(created.has_value());
	if (created.has_value())
		_app.emplace(std::move(*created));
}

Engine::~Engine()
{
	_finish();
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

	_app->run([this](saucer::application* app) -> coco::stray { return _start(app); });

	_finish();
}

coco::stray Engine::_start(saucer::application* app)
{
	auto window = saucer::window::create(app);
	if (!window)
	{
		Debug::error("Failed to create window: {}", window.error().message());
		app->quit();
		co_return;
	}
	_window = std::move(window).value();

	const u32 screen_scale = static_cast<u32>(GetSystemMetrics(SM_CYSCREEN)) / 520;

	int width{ 520 * static_cast<int>(screen_scale) };
	int height{ 510 * static_cast<int>(screen_scale) };

	if (auto config_width = _file_user_setting->parameterSection<u32>("WINDOW", "width"))
	{
		width = static_cast<int>(config_width.value());

		if (auto config_height = _file_user_setting->parameterSection<u32>("WINDOW", "height"))
			height = static_cast<int>(config_height.value());
	}

	_window->set_size({ width, height });

	// Center on the primary monitor.
	const auto screen = app->screens().front();
	_window->set_position(
		{ .x = screen.position.x + (screen.size.w - width) / 2,
		  .y = screen.position.y + (screen.size.h - height) / 2 }
	);

	const auto hwnd = _window->native().hwnd;

	_applyDarkTitleBar(hwnd);
	_forceSetWindowIcon(hwnd, L"./unblock.ico");

	_window->set_title(("Unblock " + std::format("Version: {}", VERSION_STR)).c_str());

	// Smartview over the window (move-only -> optional).
	auto view = saucer::smartview::create(
		{
			.window = _window,
			.attributes = true,
			.persistent_cookies = true,
			.hardware_acceleration = true,
			.storage_path = std::nullopt,
			.user_agent = std::nullopt,
			.browser_flags = {},
		}
	);
	if (!view)
	{
		Debug::error("Failed to create webview: {}", view.error().message());
		app->quit();
		co_return;
	}
	_view.emplace(std::move(*view));
	_setupScheme(*_view);

	// Dev tools only in Debug builds; disabled in Release.
#ifdef _DEBUG
	_view->set_dev_tools(true);
#endif

	// Context menu does not affect visuals — disabled in both builds.
	_view->set_context_menu(false);

	_ui = std::make_shared<UiBase>(this);
	_ui->postConstruct();

	// Bridge + window subscriptions (before navigation).
	_ui->setup(&*_view);

	_view->set_url("ui://root/main.html");
	_window->show();

	_startUpdateTicker(app);

	co_await app->finish();
}

void Engine::_setupScheme(saucer::smartview& view)
{
	// Serve interface files from the "ui" folder via the ui://root/... scheme.
	const std::filesystem::path ui_root = Core::get().currentPath() / "ui";

	view.handle_scheme(
		"ui",
		[ui_root = std::move(ui_root)](const saucer::scheme::request& request, saucer::scheme::executor exec)
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

			std::vector<std::uint8_t> data(
				(std::istreambuf_iterator<char>(stream)),
				std::istreambuf_iterator<char>()
			);

			exec.resolve(
				saucer::scheme::response{
					.data = saucer::stash::from(std::move(data)),
					.mime = "text/html",
					.headers = {},
					.status = 200,
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

	_cinBuffer  = std::cin.rdbuf();
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

		_cinBuffer  = nullptr;
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
	_update_ticker		= std::jthread(
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
						if (_update_ticker_run && _ui)
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
	hideConsole();
	_ui.reset();
	_view.reset();
	_window.reset();
	_app.reset();
}

std::string Engine::_getSystemLocale()
{
	std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> buffer{};
	int									 chars = GetUserDefaultLocaleName(buffer.data(), static_cast<int>(buffer.size()));
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
