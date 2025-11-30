#pragma once
#include "engine_api.hpp"
#include "../ui/ui_base.h"

using namespace ultralight;

class ENGINE_API Engine final : public IEngineAPI
{
	RefPtr<App>	   _app;
	RefPtr<Window> _window;

	std::unique_ptr<UiBase> _ui{ nullptr };

public:
	static Engine& get();

	void initialize();
	void run();

	void	console() override;
	App*	app() override;
	Window* window() override;

private:
	void _finish();
};
