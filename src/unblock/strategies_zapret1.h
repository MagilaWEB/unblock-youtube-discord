#pragma once
#include "strategy_config_base.h"

// Classic zapret (winws.exe) strategy builder: static --dpi-desync configs
// from configs/strategy_zapret1/<version>/ with selectable fake profiles
// ([FAKE_TLS]/[FAKE_QUIC]/[FAKE_TLS_DOMAIN] in fake_bin_zapret1.config).
// Ported from the 1.4.19 tag and adapted to the current conventions.
class StrategiesZapret1 final : public StrategyConfigBase
{
	File _file_fake_bin_config;

public:
	struct FakeBinParam
	{
		std::string file_clienthello{};
		std::string file_initial{};
		std::string domain{};
		bool		init{ false };
	};

private:
	std::string							_fake_bind_key{};
	std::map<std::string, FakeBinParam> _fake_bin_params{};

public:
	StrategiesZapret1();
	~StrategiesZapret1() override = default;

	void changeFakeKey(u32 index = 1);
	void changeFakeKey(std::string_view key = "");

	std::string								   getKeyFakeBin() const;
	const std::map<std::string, FakeBinParam>& getFakeBinList() const;

private:
	void _uploadStrategies() override;
	void _saveStrategies(std::string_view str) override;

	std::filesystem::path _strategyRootDir() const override { return "strategy_zapret1"; }

	std::optional<std::string> _getFake(std::string_view str);

	// Zapret1 strips leftover placeholders per line (unlike Zapret2, which
	// does it once at the end so %INIT_*% markers survive until expansion).
	void _getAllPorts(std::string& str) const;

#ifdef UNBLOCK_TESTS
	friend class StrategiesZapret1Test;
#endif
};
