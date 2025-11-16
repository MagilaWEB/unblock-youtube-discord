#pragma once
#include "../engine/engine_api.hpp"

#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Weverything"
#else
	#pragma warning(push)
	#pragma warning(disable : 4'100)
#endif

#include <AppCore/AppCore.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

#ifdef __clang__
	#pragma clang diagnostic pop
#else
	#pragma warning(pop)
#endif

#include "ui_elements.hpp"

class Unblock;

class UI_API Ui : public UiElements,
				  public WindowListener,
				  private LoadListener,
				  private ViewListener

{
	Ptr<Unblock> _unblock;

	IEngineAPI*		_engine;
	RefPtr<Overlay> _overlay;

public:
	Ui() = delete;
	Ui(IEngineAPI* engine);
	~Ui() override;

	virtual void OnAddConsoleMessage(View* caller, const ConsoleMessage& msg) override;

	virtual void OnWindowObjectReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;
	virtual void OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;

	virtual void OnClose(Window* window) override;

	virtual void OnChangeCursor(View*, Cursor cursor) override { _engine->window()->SetCursor(cursor); }

private:
	void _checkConflictService();

	void _setting();
	void _testing();
	void _testingWindow();

	void _activeService();
	void _startService();
	void _startServiceWindow();

	void _stopService();

	void _updateTitleButton(bool proxy = false);

public:
	void	runTask(const JSObject& obj, const JSArgs& args);
	JSValue langText(const JSObject& obj, const JSArgs& args);
};
