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
	_file_user_setting = std::make_shared<File>();
	_file_user_setting->open({ Core::get().userPath() / "setting" }, ".config", true);

#ifdef DEBUG
	console();
#else
	auto result = _file_user_setting->parameterSection<bool>("SUSTEM", "show_console");
	if (result && result.value())
		console();
	else
		Debug::initLogFile();
#endif

	Localization::get().set("RU");

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
	_app->Run();
	_finish();
}

void Engine::console()
{
	static bool show{ false };
	if (show)
		return;

	show = true;
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);

	FILE* stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	freopen_s(&stream, "CONOUT$", "w+", stderr);

	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	int	   hCrt		  = _open_osfhandle(u64(handle_out), _O_TEXT);
	FILE*  hf_out	  = _fdopen(hCrt, "w");
	setvbuf(hf_out, NULL, _IONBF, 1);
	*stdout = *hf_out;

	HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
	hCrt			 = _open_osfhandle(u64(handle_in), _O_TEXT);
	FILE* hf_in		 = _fdopen(hCrt, "r");
	setvbuf(hf_in, NULL, _IONBF, 128);
	*stdin = *hf_in;

	// Enable flags so we can color the output
	DWORD dwMode = 0;
	GetConsoleMode(handle_out, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(handle_out, dwMode);
	SetConsoleTitle("Unblock Console");

	HWND  hwnd	= GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(hwnd, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);

	// Set UTF-8
	SetConsoleCP(65'001);
	SetConsoleOutputCP(65'001);

	Debug::initLogFile();
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

void Engine::_finish()
{
	_window->set_listener(nullptr);
}
