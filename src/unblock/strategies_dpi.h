#pragma once
#include "strategies_dpi_base.h"

class StrategiesDPI final : public StrategiesDPIBase
{
	Ptr<File> _file_fake_bin_config;
	Ptr<File> _file_blacklist_all;

	std::atomic_bool _filtering_top_level_domains{ false };

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
	void changeDirVersion(std::string dir_version) override;

	void addOptionalStrategies(std::string name);
	void removeOptionalStrategies(std::string name);
	void clearOptionalStrategies();

	std::string						 getKeyFakeBin() const;
	const std::vector<FakeBinParam>& getFakeBinList() const;

private:
	void _readFileStrategies(std::string section);
	void _uploadStrategies() override;
	void _saveStrategies(std::string str) override;

	std::optional<std::string> _getBlockList(std::string str) const;
	std::optional<std::string> _getGameFilter(std::string str) const;
	std::optional<std::string> _getFake(std::string str) const;
};
