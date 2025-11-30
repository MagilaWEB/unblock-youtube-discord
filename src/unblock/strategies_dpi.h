#pragma once
#include "strategies_dpi_base.h"

class StrategiesDPI final : public StrategiesDPIBase
{
	Ptr<File> _file_fake_bin_config;

	std::atomic_bool _filtering_top_level_domains{ false };
	std::atomic_bool _faked{ true };

public:
	struct FakeBinParam
	{
		std::string key{};
		std::string file_clienthello{};
		std::string file_initial{};
	};

private:
	std::string				  _fake_bind_key{};
	std::vector<FakeBinParam> _fake_bin_params{};
	std::list<std::string>	  _section_opt_service_names{};
	std::string				  _service_blocklist_file{};

public:
	StrategiesDPI();
	~StrategiesDPI() override = default;

	void changeFakeKey(u32 index = 1);
	void changeFakeKey(std::string key = "");
	void changeFilteringTopLevelDomains(bool state);

	void addOptionalStrategies(std::string name);
	void removeOptionalStrategies(std::string name);
	void clearOptionalStrategies();

	bool isFaked() const;

	std::vector<std::string>		 getStrategy(u32 service = 0) const;
	std::string						 getKeyFakeBin() const;
	const std::vector<FakeBinParam>& getFakeBinList() const;

private:
	void _readFileStrategies(std::string section, u32 index_service);
	void _uploadStrategies() override;
	void _saveStrategies(std::vector<std::string>& strategy_dpi, std::string str) override;

	std::optional<std::string> _getBlockList(std::string str) const;
	std::optional<std::string> _getFake(std::string key, std::string str) const;
};
