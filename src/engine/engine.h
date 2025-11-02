#pragma once
#include "engine_api.hpp"
#include "../ui/ui.h"

using namespace ultralight;

class ENGINE_API Engine final : public IEngineAPI
{
	RefPtr<App>	   _app;
	RefPtr<Window> _window;

	std::unique_ptr<Ui> _ui{ nullptr };

public:
	static Engine& get();

	void initialize();
	void run();

	void	console() override;
	App*	app() override;
	Window* window() override;

private:
	void _sendDpiApplicationType();
	void _finish();
};
