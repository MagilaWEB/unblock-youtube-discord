#include "engine.h"
#include "version.hpp"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

Engine& Engine::get()
{
	static Engine instance;
	return instance;
}

static std::string get_system_locale()
{
	std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> buffer{};
	int											chars = GetUserDefaultLocaleName(buffer.data(), static_cast<int>(buffer.size()));
	if (chars == 0)
		return "US";

	int			size_needed = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), chars - 1, nullptr, 0, nullptr, nullptr);
	std::string result(size_needed, 0);

	WideCharToMultiByte(CP_UTF8, 0, buffer.data(), chars - 1, result.data(), size_needed, nullptr, nullptr);
	return result.substr(result.find_first_of("-") + 1, result.length());
}

static void ForceSetWindowIcon(HWND hwnd, const wchar_t* iconPath)
{
	HICON hIconBig = static_cast<HICON>(LoadImageW(NULL, iconPath, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE));
	HICON hIconSmall = static_cast<HICON>(LoadImageW(NULL, iconPath, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));

	if (!hIconBig || !hIconSmall)
	{
		hIconBig   = static_cast<HICON>(LoadImageW(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
		hIconSmall = hIconBig;
	}

	SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconBig));
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));

	SetClassLongPtr(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hIconBig));
	SetClassLongPtr(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(hIconSmall));
}

static void ApplyDarkTitleBar(HWND hwnd)
{
	BOOL useDarkMode = TRUE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

void Engine::initialize()
{
	Localization::get().set(get_system_locale());

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
	auto result = _file_user_setting->parameterSection<bool>("SUSTEM", "show_console");
	if (result && result.value())
		showConsole();
#endif

	Debug::initLogFile();

	// assign a base ui folder to ultralight.
	Platform::instance().set_file_system(GetPlatformFileSystem("./../ui/"));

	Config config{};
	config.effect_quality		 = EffectQuality::Low;
	config.force_repaint		 = true;
	config.memory_cache_size	 = 128 * 1'024 * 1'024;
	config.animation_timer_delay = 1.0 / 30.0;
	config.max_update_time		 = 1.0 / 60.0;

	Settings setting{};

	_app = App::Create(setting, config);

	u32 width{ 0 };
	u32 height{ 0 };

	if (auto config_width = _file_user_setting->parameterSection<u32>("WINDOW", "width"))
	{
		width = config_width.value();

		if (auto config_height = _file_user_setting->parameterSection<u32>("WINDOW", "height"))
			height = config_height.value();
	}
	else
	{
		const int screenScale = GetSystemMetrics(SM_CYSCREEN) / 520;

		width = 520 * screenScale;
		height = 510 * screenScale;
	}

	_window = Window::Create(_app->main_monitor(), width, height, false, kWindowFlags_Resizable | kWindowFlags_Maximizable);

	auto hwnd = static_cast<HWND>(_window->native_handle());

	ApplyDarkTitleBar(hwnd);
	ForceSetWindowIcon(hwnd, L"./unblock.ico");

	static std::string title{ "Unblock " + std::format("Version: {}", VERSION_STR) };
	_window->SetTitle(title.c_str());

	_ui = std::make_unique<UiBase>(this);
	_window->set_listener(_ui.get());
}

void Engine::run()
{
	if (!_checkRunApp())
		_app->Run();

	_finish();
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

	// style color text cmd
	if (auto handle_out = GetStdHandle(STD_OUTPUT_HANDLE))
	{
		DWORD dwMode{ 0 };
		GetConsoleMode(handle_out, &dwMode);
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(handle_out, dwMode);
	}

	// Blocking close
	EnableMenuItem(GetSystemMenu(_hwnd_console, FALSE), SC_CLOSE, MF_GRAYED);

	// Set UTF-8
	SetConsoleTitle("Unblock Console");
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

App* Engine::app()
{
	return _app.get();
}

Window* Engine::window()
{
	return _window.get();
}

std::shared_ptr<File>& Engine::userConfig()
{
	return _file_user_setting;
}

bool Engine::_checkRunApp()
{
	static HANDLE mutex{ CreateMutex(nullptr, true, "MutexOfTheUnblockApplication") };
	static bool	  app_run{ WaitForSingleObject(mutex, 0) != WAIT_OBJECT_0 };
	return app_run;
}

void Engine::_finish()
{
	hideConsole();
	if (_window)
		_window->set_listener(nullptr);
}
