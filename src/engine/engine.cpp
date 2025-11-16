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

	static std::string title{ "Unblock " };
	title.append("Version:").append(VERSION_STR);

	_app = App::Create({}, config);

	_window = Window::Create(_app->main_monitor(), 1'100, 600, false, kWindowFlags_Titled | kWindowFlags_Borderless);
	_window->SetTitle(title.c_str());

	_ui = std::make_unique<Ui>(this);


	_window->set_listener(_ui.get());
}

void Engine::run()
{
	std::atomic_bool quit{ false };
	std::jthread	 _{ [&quit]
					{
						while (!quit)
						{
							using namespace std::chrono;
							std::this_thread::sleep_for(30ms);
							{
								FAST_LOCK(Core::getTaskLock());
								auto& task = Core::getTask();
								while (!task.empty())
								{
									task.front()();
									task.pop_front();
								}
							}
							std::this_thread::yield();

							FAST_LOCK_SHARED(Core::getTaskLockJS());
						}
					} };

	_app->Run();
	quit = true;

	// ASSERT(_unblock.get());

	//// Without this crutch, changing the color of the text in the console after launching the application does not work.
	//_input_console.clear();

	// while (!_quit)
	//{
	//	const u32 select = _input_console.selectFromList({ "Запустить unblock.",
	//													   "Запустить proxy Unblock.",
	//													   "Остановить службы Unblock.",
	//													   "Остановить службу proxy Unblock.",
	//													   "Остановить все службы Unblock.",
	//													   "Закрыть приложение." });

	//	switch (select)
	//	{
	//	case 0:
	//	{
	//		_sendDpiApplicationType();

	//		if (_unblock->checkSavedConfiguration())
	//		{
	//			const u32 select = _input_console.selectFromList({ "Автоматический подбор конфигурации.", "Выбрать в ручную." });

	//			if (select == 0)
	//				_unblock->startAuto();
	//			else
	//				_unblock->startManual();
	//		}
	//		break;
	//	}
	//	case 1:
	//	{
	//		_unblock->startProxyManual();
	//		break;
	//	}
	//	case 2:
	//	{
	//		_unblock->allRemoveService();
	//		break;
	//	}
	//	case 3:
	//	{
	//		_unblock->proxyRemoveService();
	//		break;
	//	}
	//	case 4:
	//	{
	//		_unblock->allRemoveService();
	//		_unblock->proxyRemoveService();
	//		break;
	//	}
	//	default:
	//	{
	//		_quit = true;
	//		return;
	//	}
	//	}

	//	_finish();
	//}

	_finish();
}

void Engine::console()
{
	static bool show{ false };
	if (!show)
	{
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

void Engine::_sendDpiApplicationType()
{
	/*const u32 select = _input_console.selectFromList(
		{ "Применить обход только для Discord.com, YouTube.com, x.com.", "Применить обход на весь сетевой трафик ОС." },
		[this](u32)
		{
			_input_console.textInfo(
				"Обход блокировки на весь сетевой трафик ОС может негативно повлиять на доступ к сайтам которые не находятся в блокировке!"
			);
		}
	);
	if (select == 1)
		_unblock->changeDpiApplicationType(DpiApplicationType::ALL);
	else
		_unblock->changeDpiApplicationType(DpiApplicationType::BASE);*/
}

void Engine::_finish()
{
	_window->set_listener(nullptr);
	/*const u32 select = _input_console.selectFromList({ "Далее.", "Закрыть приложение." });

	_quit = select == 1;*/
}
