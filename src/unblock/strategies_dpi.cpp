#include "strategies_dpi.h"

StrategiesDPI::StrategiesDPI()
{
	_file_fake_bin_config.open(Core::get().configsPath() / "fake_bin", ".config", true);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS",
		[this](std::string key, std::string value)
		{
			const auto path_file = Core::get().binariesPath() / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());

			auto& fake			  = _fake_bin_params[key];
			fake.file_clienthello = path_file.string();
			fake.init			  = true;
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS_DOMAIN",
		[this](std::string key, std::string value)
		{
			auto& fake	= _fake_bin_params[key];
			fake.domain = value;
			fake.init	= true;
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_QUIC",
		[this](std::string key, std::string value)
		{
			const auto path_file = Core::get().binariesPath() / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());

			auto& fake		  = _fake_bin_params[key];
			fake.file_initial = path_file.string();
			fake.init		  = true;
			return false;
		}
	);

	_file_fake_bin_config.close();
}

std::string StrategiesDPI::getKeyFakeBin() const
{
	return _fake_bind_key;
}

const std::map<std::string, StrategiesDPI::FakeBinParam>& StrategiesDPI::getFakeBinList() const
{
	return _fake_bin_params;
}

void StrategiesDPI::changeFakeKey(u32 index)
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

void StrategiesDPI::changeFakeKey(std::string key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	InputConsole::textInfo("Выбран FakeBin [%s].", key.c_str());

	auto& fake_bin = _fake_bin_params[key];

	ASSERT_ARGS(fake_bin.init, "a key is missing for fake_bin %s", key.c_str());

	_fake_bind_key = key;
}

void StrategiesDPI::changeFilteringTopLevelDomains(bool state)
{
	_generator.filteringTopDomain(state);
}

void StrategiesDPI::changeDirVersion(std::string dir_version)
{
	StrategiesDPIBase::changeDirVersion(dir_version);

	_strategy_files_list.clear();

	if (_patch_dir_version.empty())
		_patch_file = Core::get().configsPath() / "strategy";
	else
		_patch_file = Core::get().configsPath() / "strategy" / _patch_dir_version;

	for (auto& entry : std::filesystem::directory_iterator(_patch_file))
		_strategy_files_list.push_back(entry.path().filename().string());

	_sortFiles();
}

void StrategiesDPI::changeOptionalServices(std::list<std::string> list_service)
{
	_section_opt_service_names = list_service;
}

void StrategiesDPI::_uploadStrategies()
{
	_strategy_dpi.clear();

	_generator.changeServiceList(_section_opt_service_names);
	_generator.inFile(_file_strategy_dpi);

	auto& map = _generator.mapFilters();

	for (auto& line : map.at("START"))
		_saveStrategies(line);

	for (auto& [key, list] : map)
		if (key != "START" && key != "END")
			for (auto& line : list)
				_saveStrategies(line);

	for (auto& line : map.at("END"))
		_saveStrategies(line);

	for (auto& line : _strategy_dpi)
		if (line.contains("=\""))
			line = std::regex_replace(line, std::regex{ "\\=" }, " ");

	std::erase_if(_strategy_dpi, [](std::string line) { return line.empty(); });

	if (_strategy_dpi.empty())
	{
		Debug::warning("strategy dpi empty");
		return;
	}

	while (_strategy_dpi.back().starts_with("--new"))
		_strategy_dpi.pop_back();

	for (auto& line : _strategy_dpi)
		Debug::ok("%s", line.c_str());
}

void StrategiesDPI::_saveStrategies(std::string str)
{
	if (_ignoringLineStrategy(str))
		return;

	if (auto new_str = _getAllPorts(str))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake(str))
	{
		_strategy_dpi.emplace_back(new_str.value());
		return;
	}

	StrategiesDPIBase::_saveStrategies(str);
}

bool StrategiesDPI::_ignoringLineStrategy(std::string str)
{
	return str.empty() || str.starts_with("//") || std::regex_match(str, std::regex{ "\n" });
}

std::optional<std::string> StrategiesDPI::_getAllPorts(std::string str) const
{
	if (str.contains("%AllPorts%"))
		return std::regex_replace(str, std::regex{ "%AllPorts%" }, "1024-65535");

	return std::nullopt;
}

std::optional<std::string> StrategiesDPI::_getFake(std::string str)
{
	if (_fake_bind_key.empty())
		return std::nullopt;

	auto& fake = _fake_bin_params[_fake_bind_key];

	if (fake.init)
	{
		if (str.contains("%FAKE_TLS_DOMAIN%"))
		{
			if (fake.domain.empty())
				return "--dpi-desync-fake-tls-mod=rnd,dupsid,sni=www.google.com";

			return "--dpi-desync-fake-tls-mod=rnd,dupsid,sni=" + fake.domain;
		}

		if (str.contains("%FAKE_HOST_DOMAIN%"))
		{
			if (fake.domain.empty())
				return std::regex_replace(str, std::regex{ "%FAKE_HOST_DOMAIN%" }, "--dpi-desync-hostfakesplit-mod=host=www.google.com");

			return std::regex_replace(str, std::regex{ "%FAKE_HOST_DOMAIN%" }, "--dpi-desync-hostfakesplit-mod=host=" + fake.domain);
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

			return std::regex_replace(str, std::regex{ "%FAKE_CLIENT_HELLO%" }, fake.file_clienthello);
		}

		if (str.contains("%FAKE_INITIAL%"))
		{
			if (fake.file_initial.empty())
				return "";

			return std::regex_replace(str, std::regex{ "%FAKE_INITIAL%" }, fake.file_initial);
		}

		return std::nullopt;
	}
	else
	{
		std::string list_key{};
		for (auto& [key, _] : _fake_bin_params)
			if (list_key.empty())
				list_key = key;
			else
				list_key.append("," + key);

		Debug::error("fake ключ [%s] не найден! Доступные ключи [%s].", _fake_bind_key.c_str(), list_key.c_str());
	}

	return std::nullopt;
}
