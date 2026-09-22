#include "strategies_zapret1.h"

StrategiesZapret1::StrategiesZapret1()
{
	_file_fake_bin_config.open(Core::get().configsPath() / "fake_bin_zapret1", ".config", true);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / "fake" / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake			  = _fake_bin_params[std::string{ key }];
			fake.file_clienthello = path_file.string();
			fake.init			  = true;
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS_DOMAIN",
		[this](std::string_view key, std::string_view value)
		{
			auto& fake	= _fake_bin_params[std::string{ key }];
			fake.domain = value;
			fake.init	= true;
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_QUIC",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / "fake" / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake		  = _fake_bin_params[std::string{ key }];
			fake.file_initial = path_file.string();
			fake.init		  = true;
			return false;
		}
	);

	_file_fake_bin_config.close();
}

std::string StrategiesZapret1::getKeyFakeBin() const
{
	return _fake_bind_key;
}

const std::map<std::string, StrategiesZapret1::FakeBinParam>& StrategiesZapret1::getFakeBinList() const
{
	return _fake_bin_params;
}

void StrategiesZapret1::changeFakeKey(u32 index)
{
	u32 it{ 0 };
	for (auto& [key, data] : _fake_bin_params)
	{
		if (it == index)
		{
			changeFakeKey(key);
			return;
		}
		it++;
	}
}

void StrategiesZapret1::changeFakeKey(std::string_view key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	// No log here on purpose: the strategy upload that follows already logs
	// the effective config, otherwise every startup reports the selection twice.
	const auto it = _fake_bin_params.find(std::string{ key });
	ASSERT_ARGS(it != _fake_bin_params.end(), "a key is missing for fake_bin {}", key);
	ASSERT_ARGS(it->second.init, "a key is missing for fake_bin {}", key);

	_fake_bind_key = it->first;
}

void StrategiesZapret1::_uploadStrategies()
{
	_uploadFromGenerator();

	std::erase_if(_strategy_dpi, [](const std::string& line) { return line.empty(); });

	if (_strategy_dpi.empty())
	{
		Debug::warning("strategy dpi empty");
		return;
	}

	while (_strategy_dpi.back().starts_with("--new"))
		_strategy_dpi.pop_back();

	for (auto& line : _strategy_dpi)
		Debug::ok("{}", line);
}

void StrategiesZapret1::_saveStrategies(std::string_view str)
{
	if (_ignoringLineStrategy(str))
		return;

	if (auto new_str = _getFake(str))
	{
		_strategy_dpi.emplace_back(new_str.value());
		return;
	}

	StrategyConfigBase::_saveStrategies(str);

	auto& line_back = _strategy_dpi.back();

	// Classic zapret takes file arguments space-separated, not with '='.
	if (line_back.contains("=\""))
		line_back = std::regex_replace(line_back, std::regex{ "=" }, " ");

	_getAllPorts(line_back);
}

void StrategiesZapret1::_getAllPorts(std::string& str) const
{
	StrategyConfigBase::_getAllPorts(str);

	static const std::regex port_vars{ R"(%[^%]+%)" };
	str = std::regex_replace(str, port_vars, "");

	_normalizeStrategyString(str);
}

std::optional<std::string> StrategiesZapret1::_getFake(std::string_view str)
{
	// Without a selected profile fake lines degrade to empty ones (dropped
	// later) instead of leaking raw placeholders to winws; plain lines pass
	// through to the base handler below.
	if (_fake_bind_key.empty())
	{
		static constexpr std::string_view fake_markers[]{ "%FAKE_", "%SEQOVL_PATTERN%" };
		for (auto marker : fake_markers)
			if (str.contains(marker))
				return "";

		return std::nullopt;
	}

	const auto it = _fake_bin_params.find(_fake_bind_key);
	if (it == _fake_bin_params.end())
	{
		std::string list_key{};
		for (auto& [key, _] : _fake_bin_params)
			if (list_key.empty())
				list_key = key;
			else
				list_key.append("," + key);

		Debug::error("fake key [{}] not found! Available keys [{}].", _fake_bind_key, list_key);
		return std::nullopt;
	}

	auto& fake = it->second;
	if (!fake.init)
	{
		Debug::error("fake key [{}] is not initialized.", _fake_bind_key);
		return std::nullopt;
	}

	if (str.contains("%FAKE_TLS_DOMAIN%"))
	{
		if (fake.domain.empty())
			return "--dpi-desync-fake-tls-mod=rnd,dupsid,sni=www.google.com";

		return "--dpi-desync-fake-tls-mod=rnd,dupsid,sni=" + fake.domain;
	}

	if (str.contains("%FAKE_HOST_DOMAIN%"))
	{
		if (fake.domain.empty())
			return std::regex_replace(std::string{ str }, std::regex{ "%FAKE_HOST_DOMAIN%" }, "--dpi-desync-hostfakesplit-mod=host=www.google.com");

		return std::regex_replace(std::string{ str }, std::regex{ "%FAKE_HOST_DOMAIN%" }, "--dpi-desync-hostfakesplit-mod=host=" + fake.domain);
	}

	if (str.contains("%FAKE_TLS%"))
	{
		if (fake.file_clienthello.empty())
			return "";

		return "--dpi-desync-fake-tls \"" + fake.file_clienthello + "\"";
	}

	if (str.contains("%FAKE_QUIC%"))
	{
		if (fake.file_initial.empty())
			return "";

		return "--dpi-desync-fake-quic \"" + fake.file_initial + "\"";
	}

	if (str.contains("%FAKE_HTTP%"))
	{
		if (fake.file_initial.empty())
			return "";

		return "--dpi-desync-fake-http \"" + fake.file_clienthello + "\"";
	}

	if (str.contains("%FAKE_DISCORD%"))
	{
		if (fake.file_initial.empty())
			return "";

		return "--dpi-desync-fake-discord \"" + fake.file_initial + "\"";
	}

	if (str.contains("%FAKE_STUN%"))
	{
		if (fake.file_initial.empty())
			return "";

		return "--dpi-desync-fake-stun \"" + fake.file_initial + "\"";
	}

	if (str.contains("%SEQOVL_PATTERN%"))
	{
		if (fake.file_clienthello.empty())
			return "";

		return "--dpi-desync-split-seqovl-pattern \"" + fake.file_clienthello + "\"";
	}

	if (str.contains("%FAKE_UNKNOWN%"))
	{
		if (fake.file_initial.empty())
			return "";

		return "--dpi-desync-fake-unknown-udp \"" + fake.file_initial + "\"";
	}

	if (str.contains("%FAKE_CLIENT_HELLO%"))
	{
		if (fake.file_clienthello.empty())
			return "";

		return std::regex_replace(std::string{ str }, std::regex{ "%FAKE_CLIENT_HELLO%" }, fake.file_clienthello);
	}

	if (str.contains("%FAKE_INITIAL%"))
	{
		if (fake.file_initial.empty())
			return "";

		return std::regex_replace(std::string{ str }, std::regex{ "%FAKE_INITIAL%" }, fake.file_initial);
	}

	return std::nullopt;
}
