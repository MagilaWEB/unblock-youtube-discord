#pragma once

#include "../core/pch.h"
#include "../core/file_system.h"

#include "engine_api.hpp"

#include "../ui/ui_base.h"

#include <atomic>
#include <fstream>
#include <optional>
#include <thread>

class ENGINE_API Engine final : public IEngineAPI
{
	std::optional<saucer::application>	 _app;
	std::shared_ptr<saucer::window>		 _window;
	std::optional<saucer::smartview>	 _view;

	std::shared_ptr<UiBase> _ui{ nullptr };
	std::shared_ptr<File>	_file_user_setting;

	HWND			_hwnd_console;
	std::streambuf* _cinBuffer, *_coutBuffer, *_cerrBuffer;
	std::fstream	_consoleInput, _consoleOutput, _consoleError;
	FILE*			_fp_console;

	// UI update ticker: posts update() to the main thread where JS and WebView2 live.
	std::jthread _update_ticker;
	std::atomic<bool> _update_ticker_run{ false };

public:
	Engine();
	~Engine() override;

	static Engine& get();

	void initialize();
	void run();

	saucer::smartview*			   webview() override;
	std::shared_ptr<saucer::window> window() override;
	void						   showConsole() override;
	void						   hideConsole() override;
	void						   quit() override;

	std::shared_ptr<File>& userConfig() override;

	bool hasCyrillicOrSpaceInBinaryPath() override;

private:
	bool _checkRunApp();
	void _finish();
	std::string _getSystemLocale();
	void _forceSetWindowIcon(HWND hwnd, const wchar_t* iconPath);
	void _applyDarkTitleBar(HWND hwnd);
	void _setupScheme(saucer::smartview& view);
	void _startUpdateTicker(saucer::application* app);
	void _stopUpdateTicker();
	coco::stray _start(saucer::application* app);
};