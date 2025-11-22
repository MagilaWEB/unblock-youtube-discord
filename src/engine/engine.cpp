#include "engine.h"
#include "version.hpp"

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
#ifdef DEBUG
	console();
#endif

	Localization::get().set("RU");

	// assign a base ui folder to ultralight.
	Platform::instance().set_file_system(GetPlatformFileSystem("./../ui/"));

	Config config{};
	config.effect_quality = EffectQuality::High;

#if __clang__
	[[clang::no_destroy]]
#endif
	static std::string title{ "Unblock " };
	title.append("Version:").append(VERSION_STR);

	_app = App::Create({}, config);

	_window = Window::Create(_app->main_monitor(), 1'100, 600, false, kWindowFlags_Titled | kWindowFlags_Borderless | kWindowFlags_Resizable);
	_window->SetTitle(title.c_str());

	_ui = std::make_unique<Ui>(this);


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

	// Enable flags so we can color the output
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD  dwMode		 = 0;
	GetConsoleMode(consoleHandle, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(consoleHandle, dwMode);
	SetConsoleTitle("Unblock Console");

	CONSOLE_FONT_INFOEX fontInfo{};
	fontInfo.cbSize = sizeof(fontInfo);

	GetCurrentConsoleFontEx(consoleHandle, TRUE, &fontInfo);

	wcscpy_s(fontInfo.FaceName, L"Lucida Console");
	fontInfo.dwFontSize.Y = 15;
	fontInfo.dwFontSize.X = 38;

	SetCurrentConsoleFontEx(consoleHandle, TRUE, &fontInfo);

	HWND  hwnd	= GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(hwnd, TRUE);
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

void Engine::_finish()
{
	_window->set_listener(nullptr);
}
