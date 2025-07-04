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
		CONSOLE_FONT_INFOEX fontInfo;
		fontInfo.cbSize = sizeof(fontInfo);

		GetCurrentConsoleFontEx(hConsole, TRUE, &fontInfo);

		wcscpy(fontInfo.FaceName, L"Lucida Console");
		fontInfo.dwFontSize.Y = 15;
		fontInfo.dwFontSize.X = 38;

		SetCurrentConsoleFontEx(hConsole, TRUE, &fontInfo);
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
		_input_console.textInfo("Нажмите 1, для остановки служб обхода блокировок.");
		_input_console.textInfo("Нажмите 2, для запуска подбора конфигурации.");

		if (_input_console.sendNum({ 1, 2 }) == 1)
		{
			_unblock->allRemoveService();
			_finish();
			continue;
		}

		_sendDpiApplicationType();

		_unblock->baseTestDomain();

		_unblock->start();

		_finish();
	}
}

void Engine::_sendDpiApplicationType()
{
	_input_console.textInfo(
		"Обход блокировки на весь сетевой трафик ОС может негативно повлиять на доступ к сайтам которые не находятся в блокировке!"
	);
	_input_console.textAsk("Применить обход блокировки на весь сетевой трафик ОС");

	if (_input_console.getBool())
		_unblock->changeDpiApplicationType(DpiApplicationType::ALL);
	else
		_unblock->changeDpiApplicationType(DpiApplicationType::BASE);
}

void Engine::_finish()
{
	_input_console.textAsk("Закрыть приложение");

	quit = _input_console.getBool();
}
