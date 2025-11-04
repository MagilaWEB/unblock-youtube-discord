#pragma once
#include "../engine/engine_api.hpp"

#include <AppCore/AppCore.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

#include "ui_elements.hpp"

class Unblock;

class UI_API Ui : public WindowListener,
				  public LoadListener,
				  public ViewListener,
				  public UiElements
{
	Ptr<Unblock> _unblock;

	IEngineAPI*		_engine;
	RefPtr<Overlay> _overlay;

public:
	Ui() = delete;
	Ui(IEngineAPI* engine);
	~Ui();

	virtual void OnAddConsoleMessage(View* caller, const ConsoleMessage& msg) override;

	virtual void OnWindowObjectReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;
	virtual void OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;

	virtual void OnClose(Window* window) override;

	virtual void OnResize(Window* window, uint32_t width, uint32_t height) override {}

	virtual void OnChangeCursor(View* caller, Cursor cursor) override { _engine->window()->SetCursor(cursor); }

private:
	void _setting();

	void _testing();
	void _testingWindow();

	void _startService();
	void _startServiceWindow();

	void _stopService();

	void _updateTitleButton(bool proxy = false);

public:
	void	runTask(const JSObject& obj, const JSArgs& args);
	JSValue langText(const JSObject& obj, const JSArgs& args);
};
