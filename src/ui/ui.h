#pragma once
#include "ui_api.hpp"

#include <AppCore/AppCore.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

using namespace ultralight;

class UI_API Ui : public IUiAPI,
				  WindowListener,
				  ViewListener
{
	RefPtr<ultralight::App> _app;
	RefPtr<Window>			_window;
	RefPtr<Overlay>			_overlay;

public:
	Ui();
	~Ui();

	void Run() override;

	virtual void OnClose(ultralight::Window* window) override;

	virtual void OnResize(ultralight::Window* window, uint32_t width, uint32_t height) override {}

	virtual void OnChangeCursor(ultralight::View* caller, ultralight::Cursor cursor) override { _window->SetCursor(cursor); }

private:
};
