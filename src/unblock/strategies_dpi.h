#pragma once
#include "strategies_dpi_base.h"
#include "strategy_generator.h"

class StrategiesDPI final : public StrategiesDPIBase
{
	File			  _file_fake_bin_config;
	StrategyGenerator _generator;

public:
	struct FakeBinParam
	{
		bool		init{ false };
		std::string file_clienthello{};
		std::string file_initial{};
		std::string domain;
	};

private:
	std::string							_fake_bind_key{};
	std::map<std::string, FakeBinParam> _fake_bin_params{};
	std::list<std::string>				_section_opt_service_names{};

public:
	StrategiesDPI();
	~StrategiesDPI() override = default;

	void changeFakeKey(u32 index = 1);
	void changeFakeKey(std::string key = "");
	void changeFilteringTopLevelDomains(bool state);
	void changeDirVersion(std::string dir_version) override;

	void addOptionalStrategies(std::string name);
	void removeOptionalStrategies(std::string name);
	void clearOptionalStrategies();

	std::string								   getKeyFakeBin() const;
	const std::map<std::string, FakeBinParam>& getFakeBinList() const;

private:
	void _uploadStrategies() override;
	void _saveStrategies(std::string str) override;

	bool					   _ignoringLineStrategy(std::string str);
	std::optional<std::string> _getAllPorts(std::string str) const;
	std::optional<std::string> _getFake(std::string str);
};
