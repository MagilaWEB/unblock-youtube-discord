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

using namespace ultralight;

class Ui;
class UI_API UiBase final : public WindowListener,
							private LoadListener,
							private ViewListener

{
	std::unique_ptr<Ui> _ui;

	Ptr<File> _file_user_setting;

	IEngineAPI*		_engine;
	RefPtr<Overlay> _overlay;

public:
	UiBase() = delete;
	UiBase(IEngineAPI* engine);
	~UiBase() override;

	void OnAddConsoleMessage(View* caller, const ConsoleMessage& msg) override;

	void OnWindowObjectReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;
	void OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;

	void OnResize(ultralight::Window* window, uint32_t width, uint32_t height) override;

	void OnClose(Window* window) override;

	void OnChangeCursor(View*, Cursor cursor) override { _engine->window()->SetCursor(cursor); }

	const Ptr<File>& userSetting();

public:
	void	runTask(const JSObject& obj, const JSArgs& args);
	JSValue langText(const JSObject& obj, const JSArgs& args);
};
