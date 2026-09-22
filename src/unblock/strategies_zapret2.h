#pragma once
#include "strategy_config_base.h"

class StrategiesZapret2 final : public StrategyConfigBase
{
	File _file_fake_bin_config;
	File _file_lua_init;

public:
	struct FakeBinParam
	{
		std::string file{};
		bool		init{ false };
	};

private:
	std::map<std::string, FakeBinParam> _fake_bin_params{};
	bool								_numbering_active{ false };
	u32									_strategy_index{ 0 };

public:
	StrategiesZapret2();
	~StrategiesZapret2() override = default;

private:
	void _uploadStrategies() override;
	void _saveStrategies(std::string_view str) override;

	std::filesystem::path _strategyRootDir() const override { return "strategy"; }

	void _init_lua_to_zapret();
	void _blob_init_to_zapret();
	void _normalizeStrategyFinal();
	void _luaDesyncNumberStrategy(std::string& str);
};
