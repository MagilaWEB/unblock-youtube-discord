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
	Ptr<FileSystem> _file_feke_bin_config;

	std::string									 _fake_bind_key;
	std::vector<std::string>					 _feke_key_list{};
	std::unordered_map<std::string, std::string> _feke_bin_params_tls;
	std::unordered_map<std::string, std::string> _feke_bin_params_quic;
	std::vector<std::string>					 _strategy_files_list;
	std::vector<std::string>					 _strategy_dpi[_STRATEGY_DPI_MAX];
	bool										 _ignoring_hostlist{ false };

public:
	StrategiesDPI();
	~StrategiesDPI() = default;

	void changeStrategy(u32 index = 1);
	void changeFakeKey(std::string key = "");
	void changeIgnoringHostlist(bool state);

	std::string				 getStrategyFileName() const;
	u32						 getStrategySize() const;
	std::vector<std::string> getStrategy(u32 service = 0) const;
	std::vector<std::string> getFekeBinList() const;

private:
	void _uploadFakeParams();
	void _uploadStrategies();
	void _saveStrategies(std::vector<std::string>& strategy_dpi, std::string str);

	std::optional<std::string> _getPath(std::string str, std::string prefix, std::filesystem::path path) const;
	std::optional<std::string> _getBlockList(std::string str) const;
	std::optional<std::string> _getFakeTls(std::string str) const;
	std::optional<std::string> _getFakeQuic(std::string str) const;
	std::optional<std::string>
		_getFake(pcstr key, const std::unordered_map<std::string, std::string>& feke_bin_params, std::string str, pcstr argument) const;
};
