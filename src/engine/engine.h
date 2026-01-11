#pragma once
#include "engine_api.hpp"
#include "../ui/ui_base.h"

using namespace ultralight;

class ENGINE_API Engine final : public IEngineAPI
{
	RefPtr<App>	   _app;
	RefPtr<Window> _window;

	std::unique_ptr<UiBase> _ui{ nullptr };
	std::shared_ptr<File>	_file_user_setting;

public:
	static Engine& get();

	void initialize();
	void run();

	void	console() override;
	App*	app() override;
	Window* window() override;
	std::shared_ptr<File>& userConfig() override;

private:
	bool _checkRunApp();
	void _finish();
};
