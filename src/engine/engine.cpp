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
		_input_console.textInfo("Нажмите 0, для выхода из приложения.");
		_input_console.textInfo("Нажмите 1, для остановки служб обхода блокировок.");
		_input_console.textInfo("Нажмите 2, для запуска подбора конфигурации.");

		const auto result = _input_console.sendNum({
			0,
			1,
			2,
		});

		if (result == 1)
		{
			_unblock->allRemoveService();
			_finish();
			continue;
		}
		else if (result == 0)
		{
			quit = true;
			continue;
		}

		_sendDpiApplicationType();

		if (_unblock->checkSavedConfiguration())
		{
			_unblock->baseTestDomain();
			_unblock->startAuto();
		}

		_finish();
	}
}

void Engine::_sendDpiApplicationType()
{
	_input_console.textInfo("Нажмите 1, для применения обхода блокировки на весь сетевой трафик ОС.");
	_input_console.textInfo("Нажмите 2, для применения обхода только для Discord.com, YouTube.com, x.com.");
	_input_console.textInfo(
		"Обход блокировки на весь сетевой трафик ОС может негативно повлиять на доступ к сайтам которые не находятся в блокировке!"
	);

	if (_input_console.sendNum({ 1, 2 }) == 1)
		_unblock->changeDpiApplicationType(DpiApplicationType::ALL);
	else
		_unblock->changeDpiApplicationType(DpiApplicationType::BASE);
}

void Engine::_finish()
{
	_input_console.textAsk("Закрыть приложение");

	quit = _input_console.getBool();
}
