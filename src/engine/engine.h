#pragma once
#include "engine_api.hpp"
#include "../unblock/unblock_api.hpp"

class ENGINE_API Engine final : public IEngineAPI
{
	std::unique_ptr<IUnblockAPI> _unblock{ nullptr };
	InputConsole				 _input_console;
	bool						 quit{ false };


public:
	static Engine& get();

	void initialize();
	void run();

private:
	void _sendDpiApplicationType();
	void _finish();
};
