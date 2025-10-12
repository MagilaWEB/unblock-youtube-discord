#pragma once
#include "ui_api.hpp"
#include "../engine/engine_api.hpp"
#include "../unblock/unblock_api.hpp"

#include <AppCore/AppCore.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

#include "ui_button.h"

class UI_API Ui : public IUiAPI,
				  public WindowListener,
				  public LoadListener,
				  public ViewListener
{
	IEngineAPI*			_engine;
	RefPtr<Overlay>		_overlay;

	std::unique_ptr<IUnblockAPI> _unblock{ nullptr };

	BUTTON(_start_testing);
	BUTTON(_start_testing_2);

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
	void _testing();

public:
	void RunTask(const JSObject& obj, const JSArgs& args);
};
