#pragma once
#include "../core/file_system.h"

const static std::unordered_map<u32, std::string> indexStrategies{
	{ 0,	 "unblock1" },
	{ 1,	 "unblock2" },
	{ 2, "GoodbyeDPI" }
};

class StrategiesDPI
{
	constexpr static u8 _STRATEGY_DPI_MAX{ 3 };

	Ptr<FileSystem> _file_strategy_dpi;
	Ptr<FileSystem> _file_fake_bin_config;

public:
	struct FakeBinParam
	{
		std::string key{};
		std::string value_tls{};
		std::string value_quic{};
	};

private:
	std::string				  _fake_bind_key{};
	std::vector<FakeBinParam> _fake_bin_params{};
	std::vector<std::string>  _strategy_files_list{};
	std::vector<std::string>  _strategy_dpi[_STRATEGY_DPI_MAX]{};
	bool					  _ignoring_hostlist{ false };

public:
	StrategiesDPI();
	~StrategiesDPI() = default;

	void changeStrategy(u32 index = 1);
	void changeStrategy(pcstr file);
	void changeFakeKey(std::string key = "");
	void changeIgnoringHostlist(bool state);

	std::string						 getStrategyFileName() const;
	u32								 getStrategySize() const;
	std::vector<std::string>		 getStrategy(u32 service = 0) const;
	std::string						 getKeyFakeBin() const;
	const std::vector<FakeBinParam>& getFakeBinList() const;

private:
	void _uploadStrategies();
	void _saveStrategies(std::vector<std::string>& strategy_dpi, std::string str);

	std::optional<std::string> _getPath(std::string str, std::string prefix, std::filesystem::path path) const;
	std::optional<std::string> _getBlockList(std::string str) const;
	std::optional<std::string> _getFake(std::string key, std::string str) const;
};
