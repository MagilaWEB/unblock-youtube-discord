#pragma once
#include "strategies_dpi_base.h"

inline constexpr std::pair<u32, pcstr> indexStrategies[]{
	{ 0,	 "unblock1" },
	{ 1,	 "unblock2" },
	{ 2, "GoodbyeDPI" }
};

class StrategiesDPI final : public StrategiesDPIBase
{
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
	bool					  _ignoring_hostlist{ false };

public:
	StrategiesDPI();
	~StrategiesDPI() = default;

	void changeFakeKey(std::string key = "");
	void changeIgnoringHostlist(bool state);

	std::vector<std::string>		 getStrategy(u32 service = 0) const;
	std::string						 getKeyFakeBin() const;
	const std::vector<FakeBinParam>& getFakeBinList() const;

private:
	void _uploadStrategies() override;
	void _saveStrategies(std::vector<std::string>& strategy_dpi, std::string str) override;

	std::optional<std::string> _getBlockList(std::string str) const;
	std::optional<std::string> _getFake(std::string key, std::string str) const;
};
