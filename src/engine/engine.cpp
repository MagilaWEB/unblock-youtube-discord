#include "engine.h"
#include "version.hpp"
#include <corecrt_io.h>
#include <fcntl.h>

Engine& Engine::get()
{
#if __clang__
	[[clang::no_destroy]]
#endif
	static Engine instance;
	return instance;
}

void Engine::initialize()
{
	Localization::get().set("RU");

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
	config.effect_quality = EffectQuality::Low;

#if __clang__
	[[clang::no_destroy]]
#endif
	static std::string title{ "Unblock " };
	title.append("Version:").append(VERSION_STR);

	_app = App::Create({}, config);

	_window = Window::Create(
		_app->main_monitor(),
		1'100,
		600,
		false,
		kWindowFlags_Titled | kWindowFlags_Borderless | kWindowFlags_Resizable | kWindowFlags_Maximizable
	);
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
