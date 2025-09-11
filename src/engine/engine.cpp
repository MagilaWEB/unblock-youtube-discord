#include "pch.h"
#include "engine.h"
#include "../unblock/unblock.h"

Engine& Engine::get()
{
	[[clang::no_destroy]] static Engine instance;
	return instance;
}

void Engine::initialize()
{
	if (HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE))
	{
		CONSOLE_FONT_INFOEX fontInfo{};
		fontInfo.cbSize = sizeof(fontInfo);

		GetCurrentConsoleFontEx(hConsole, TRUE, &fontInfo);

		wcscpy(fontInfo.FaceName, L"Lucida Console");
		fontInfo.dwFontSize.Y = 15;
		fontInfo.dwFontSize.X = 38;

		SetCurrentConsoleFontEx(hConsole, TRUE, &fontInfo);

		HWND  hwnd	= GetConsoleWindow();
		HMENU hmenu = GetSystemMenu(hwnd, TRUE);
		EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);

		char consoleTitle[256];
		wsprintf(consoleTitle, "Unblock Console");
		SetConsoleTitle(static_cast<LPCTSTR>(consoleTitle));
	}

	_unblock = std::make_unique<Unblock>();
}

void Engine::run()
{
	ASSERT(_unblock.get());

	// Without this crutch, changing the color of the text in the console after launching the application does not work.
	_input_console.clear();

	while (!quit)
	{
		const u32 select = _input_console.selectFromList({ "Запустить unblock.",
														   "Запустить proxy Unblock.",
														   "Остановить службы Unblock.",
														   "Остановить службу proxy Unblock.",
														   "Остановить все службы Unblock.",
														   "Закрыть приложение." });

		switch (select)
		{
		case 0:
		{
			_sendDpiApplicationType();

			if (_unblock->checkSavedConfiguration())
			{
				const u32 select = _input_console.selectFromList({ "Автоматический подбор конфигурации.", "Выбрать в ручную." });

				if (select == 0)
					_unblock->startAuto();
				else
					_unblock->startManual();
			}
			break;
		}
		case 1:
		{
			_unblock->startProxyManual();
			break;
		}
		case 2:
		{
			_unblock->allRemoveService();
			break;
		}
		case 3:
		{
			_unblock->proxyRemoveService();
			break;
		}
		case 4:
		{
			_unblock->allRemoveService();
			_unblock->proxyRemoveService();
			break;
		}
		default:
		{
			quit = true;
			return;
		}
		}

		_finish();
	}
}

void Engine::_sendDpiApplicationType()
{
	const u32 select = _input_console.selectFromList(
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
		_unblock->changeDpiApplicationType(DpiApplicationType::BASE);
}

void Engine::_finish()
{
	const u32 select = _input_console.selectFromList({ "Далее.", "Закрыть приложение." });

	quit = select == 1;
}
