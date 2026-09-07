#pragma once

#include "engine_api.hpp"
#include "window_geometry.h"

#include "../core/file_system.h"

#include <saucer/smartview.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

class Ui;

class Engine final : public IEngineAPI
{
	std::optional<saucer::application> _app;
	std::shared_ptr<saucer::window>	   _window;
	std::optional<saucer::smartview>   _view;

	std::shared_ptr<Ui>	  _ui{ nullptr };
	std::shared_ptr<File> _file_user_setting;

	HWND			_hwnd_console;
	std::streambuf *_cinBuffer, *_coutBuffer, *_cerrBuffer;
	std::fstream	_consoleInput, _consoleOutput, _consoleError;
	FILE*			_fp_console;

	// UI update ticker: posts update() to the main thread where JS and WebView2 live.
	std::jthread	  _update_ticker;
	std::atomic<bool> _update_ticker_run{ false };

	// Debounced WINDOW geometry persistence (see markWindowGeometryDirty).
	std::atomic<bool>							   _geom_dirty{ false };
	std::chrono::steady_clock::time_point		   _geom_dirty_since{};
	std::mutex									   _geom_mutex{};
	static constexpr std::chrono::milliseconds kGeomFlushDelay{ 500 };

	// Move/resize hook (WM_EXITSIZEMOVE): saucer has no move event, so the
	// final position/size after a drag is settled and persisted here.
	WNDPROC _prev_wndproc{ nullptr };

	// Geometry decided by _restoreWindowGeometry, re-applied after show():
	// saucer caches the DPI at window creation (primary monitor), so the
	// pre-show set_size computes the physical size with a stale DPI when
	// restoring onto another monitor. After show() the cache is synced via
	// WM_DPICHANGED and the same logical values land correctly.
	window_geometry::Geometry _restored_geo{};
	unsigned				 _restored_dpi{ 96 };
	bool					 _have_restored{ false };
	static LRESULT CALLBACK _windowHookProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

public:
	Engine();
	~Engine() noexcept override;

	static Engine& get();

	void initialize();
	void run();

	saucer::smartview*				webview() override;
	std::shared_ptr<saucer::window> window() override;
	void							showConsole() override;
	void							hideConsole() override;
	void							quit() override;
	void							markWindowGeometryDirty() override;
	void							flushWindowGeometry() override;

	std::shared_ptr<File>& userConfig() override;

	bool hasCyrillicOrSpaceInBinaryPath() override;

private:
	bool		_checkRunApp();
	void		_finish();
	void		_restoreWindowGeometry();
	void		_reapplyWindowGeometry();
	void		_maybeFlushWindowGeometry();
	void		_flushWindowGeometry();
	void		_installMoveHook();
	void		_removeMoveHook();
	void		_onSizeMoveEnd();
	std::string _getSystemLocale();
	void		_forceSetWindowIcon(HWND hwnd, const wchar_t* iconPath);
	void		_applyDarkTitleBar(HWND hwnd);
	void		_setupScheme(saucer::smartview& view);
	void		_startUpdateTicker(saucer::application* app);
	void		_stopUpdateTicker();
	coco::stray _start(saucer::application* app);
};
