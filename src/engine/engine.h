#pragma once
#include <AppCore/App.h>

#include "engine_api.hpp"
#include "../ui/ui_api.hpp"
#include "../unblock/unblock_api.hpp"

using namespace ultralight;

class ENGINE_API Engine final : public IEngineAPI
{
	std::unique_ptr<IUiAPI>		 _ui{ nullptr };
	std::unique_ptr<IUnblockAPI> _unblock{ nullptr };
	InputConsole				 _input_console;
	bool						 _quit{ false };

public:
	static Engine& get();

	void initialize();
	void run();

private:
	void _sendDpiApplicationType();
	void _finish();
};
