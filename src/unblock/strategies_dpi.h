#pragma once
#include "strategies_dpi_base.h"
#include "strategy_generator.h"

class StrategiesDPI final : public StrategiesDPIBase
{
	File				  _file_fake_bin_config;
	File				  _file_lua_init;
	std::shared_ptr<File> _file_service_list;

	StrategyGenerator _generator;

public:
	struct FakeBinParam
	{
		std::string file{};
		bool		init{ false };
	};

private:
	std::map<std::string, FakeBinParam> _fake_bin_params{};
	std::list<std::string>				_section_opt_service_names{};
	u32									_max_strategy_count{ 0 };

public:
	StrategiesDPI();
	~StrategiesDPI() override = default;

	void serviceConfigFile(const std::shared_ptr<File>& config);

	void changeDirVersion(std::string_view dir_version) override;
	void changeOptionalServices(std::list<std::string> list_service);
	u32	 getMaxStrategyCount() const;

private:
	void _uploadStrategies() override;
	void _saveStrategies(std::string_view str) override;

	void					   _init_lua_to_zapret();
	void					   _blob_init_to_zapret();
	void					   _normalizeStrategyString(std::string& str) const;
	void					   _normalizeStrategyFinal();
	bool					   _ignoringLineStrategy(std::string_view str) const;
	void					   _getAllPorts(std::string& str) const;
	void					   _luaDesyncNumberStrategy(std::string& str);
};
