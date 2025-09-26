#pragma once
#include "engine_api.hpp"
#include "../ui/ui_api.hpp"
#include "../unblock/unblock_api.hpp"

#include <AppCore/AppCore.h>
#include <AppCore/App.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
#include <Ultralight/Ultralight.h>

using namespace ultralight;

class ENGINE_API Engine final : public IEngineAPI
{
	RefPtr<App>					 _app;
	RefPtr<Window>				 _window;

	std::unique_ptr<IUiAPI>		 _ui{ nullptr };
	std::unique_ptr<IUnblockAPI> _unblock{ nullptr };

	InputConsole				 _input_console;
	bool						 _quit{ false };

public:
	static Engine& get();

	void initialize();
	void run();
	App* app() override;
	Window* window() override;

private:
	void _sendDpiApplicationType();
	void _finish();
};
