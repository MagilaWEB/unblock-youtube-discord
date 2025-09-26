#pragma once
#include "ui_api.hpp"
#include "../engine/engine_api.hpp"

#include <AppCore/AppCore.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

using namespace ultralight;

class UI_API Ui : public IUiAPI,
				  WindowListener,
				  ViewListener
{
	std::shared_ptr<IEngineAPI> _engine;
	RefPtr<Overlay>				_overlay;

public:
	Ui() = delete;
	Ui(std::shared_ptr<IEngineAPI> engine);
	~Ui();

	void Run() override;

	virtual void OnClose(Window* window) override;

	virtual void OnResize(Window* window, uint32_t width, uint32_t height) override {}

	virtual void OnChangeCursor(View* caller, Cursor cursor) override { _engine->window()->SetCursor(cursor); }

private:
};
