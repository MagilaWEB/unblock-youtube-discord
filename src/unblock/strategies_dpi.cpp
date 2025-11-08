#include "strategies_dpi.h"

StrategiesDPI::StrategiesDPI()
{
	patch_file = Core::get().configsPath() / "strategy";
	for (auto& entry : std::filesystem::directory_iterator(patch_file))
		_strategy_files_list.push_back(entry.path().filename().string());

	_file_fake_bin_config->open(Core::get().configsPath() / "fake_bin", ".config", true);

	_file_fake_bin_config->forLineParametersSection(
		"FAKE_TLS",
		[this](std::string key, std::string value)
		{
			const auto path_file = Core::get().binariesPath() / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());

			_fake_bin_params.push_back({ key, path_file.string() });
			return false;
		}
	);

	_file_fake_bin_config->forLineParametersSection(
		"FAKE_QUIC",
		[this](std::string key, std::string value)
		{
			const auto path_file = Core::get().binariesPath() / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());

			auto it =
				std::find_if(_fake_bin_params.begin(), _fake_bin_params.end(), [&key](const FakeBinParam& _it) { return _it.key.contains(key); });
			if (it != _fake_bin_params.end())
				(*it).file_initial = path_file.string();
			else
				_fake_bin_params.push_back({ key, {}, path_file.string() });

			return false;
		}
	);
}

std::vector<std::string> StrategiesDPI::getStrategy(u32 service) const
{
	ASSERT_ARGS(
		service < STRATEGY_DPI_MAX,
		"getStrategy is an attempt to get a strategy vector for the service [%s] that does not match STRATEGY_DPI_MAX [%s]!",
		service,
		STRATEGY_DPI_MAX
	);

	return _strategy_dpi.at(service);
}

std::string StrategiesDPI::getKeyFakeBin() const
{
	return _fake_bind_key;
}

const std::vector<StrategiesDPI::FakeBinParam>& StrategiesDPI::getFakeBinList() const
{
	return _fake_bin_params;
}

void StrategiesDPI::changeFakeKey(u32 index)
{
	const auto& strategy_file = _fake_bin_params[index];
	InputConsole::textInfo("Выбран FakeBin [%s].", strategy_file.key.c_str());

	changeFakeKey(strategy_file.key);
}

void StrategiesDPI::changeFakeKey(std::string key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	auto it = std::find_if(_fake_bin_params.begin(), _fake_bin_params.end(), [&key](const FakeBinParam& _it) { return _it.key.contains(key); });

	ASSERT_ARGS(it != _fake_bin_params.end(), "a key is missing for fake_bin %s", key.c_str());

	_fake_bind_key = key;
}

void StrategiesDPI::changeFilteringTopLevelDomains(bool state)
{
	_filtering_top_level_domains = state;
}

bool StrategiesDPI::isFaked() const
{
	return _faked;
}

void StrategiesDPI::_uploadStrategies()
{
	if (_file_strategy_dpi->isOpen())
	{
		auto faked = _file_strategy_dpi->parameterSection<bool>("SETTING", "faked");
		_faked	   = faked ? faked.value() : false;

		for (auto& service : _strategy_dpi)
			service.clear();

		_file_strategy_dpi->forLineSection(
			"START_SERVICE",
			[this](std::string str)
			{
				for (auto& [index, service_name] : indexStrategies)
				{
					if (str.contains(service_name))
					{
						_file_strategy_dpi->forLineSection(
							service_name,
							[this, &index](std::string _str)
							{
								_saveStrategies(_strategy_dpi[index], _str);
								return false;
							}
						);
					}
				}

				return false;
			}
		);
	}
}

void StrategiesDPI::_saveStrategies(std::vector<std::string>& strategy_dpi, std::string str)
{
	if (auto new_str = _getBlockList(str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%FAKE_TLS%", str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%FAKE_QUIC%", str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%FAKE_UNKNOWN%", str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%SEQOVL_PATTERN%", str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	__super::_saveStrategies(strategy_dpi, str);
}

std::optional<std::string> StrategiesDPI::_getBlockList(std::string str) const
{
	auto path_file					 = Core::get().configsPath() / "blacklist.list";
	auto path_file_top_level_domains = Core::get().configsPath() / "top_level_domains.list";

	if (str.contains("%BLOCKLIST%"))
	{
		if (_filtering_top_level_domains)
		{
			ASSERT_ARGS(
				std::filesystem::exists(path_file_top_level_domains),
				"The [%s] file does not exist!",
				path_file_top_level_domains.string().c_str()
			);
			return "--hostlist=" + (path_file_top_level_domains.string());
		}

		ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());
		return "--hostlist=" + (path_file.string());
	}

	if (str.contains("%DOMAINS-GOOGLE%"))
	{
		auto path_file_blacklist_google = Core::get().configsPath() / "blacklist_google.list";
		ASSERT_ARGS(
			std::filesystem::exists(path_file_blacklist_google),
			"The [%s] file does not exist!",
			path_file_blacklist_google.string().c_str()
		);
		return "--hostlist=" + (path_file_blacklist_google.string());
	}

	if (str.contains("%DOMAINS-EXCLUDE%"))
	{
		auto path_file_domains_exclude = Core::get().configsPath() / "domains_exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_file_domains_exclude), "The [%s] file does not exist!", path_file_domains_exclude.string().c_str());
		return "--hostlist-exclude=" + (path_file_domains_exclude.string());
	}

	if (str.contains("%BLOCKLIST-GD-DPI%"))
	{
		if (_filtering_top_level_domains)
		{
			ASSERT_ARGS(
				std::filesystem::exists(path_file_top_level_domains),
				"The [%s] file does not exist!",
				path_file_top_level_domains.string().c_str()
			);
			return "--blacklist " + (path_file_top_level_domains.string());
		}

		ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());
		return "--blacklist " + (path_file.string());
	}

	path_file = Core::get().configsPath() / "ip-blacklist.list";

	if (str.contains("%IP-SETLIST%"))
	{
		if (_filtering_top_level_domains)
		{
			ASSERT_ARGS(
				std::filesystem::exists(path_file_top_level_domains),
				"The [%s] file does not exist!",
				path_file_top_level_domains.string().c_str()
			);
			return "--hostlist=" + (path_file_top_level_domains.string());
		}

		ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());
		return "--ipset=" + (path_file.string());
	}

	if (str.contains("%IP-BLOCKLIST%"))
	{
		if (_filtering_top_level_domains)
		{
			ASSERT_ARGS(
				std::filesystem::exists(path_file_top_level_domains),
				"The [%s] file does not exist!",
				path_file_top_level_domains.string().c_str()
			);
			return "--hostlist=" + (path_file_top_level_domains.string());
		}

		ASSERT_ARGS(std::filesystem::exists(path_file), "The [%s] file does not exist!", path_file.string().c_str());
		return "--hostlist=" + (path_file.string());
	}

	return std::nullopt;
}

std::optional<std::string> StrategiesDPI::_getFake(std::string key, std::string str) const
{
	if (str.contains(key))
	{
		if (_fake_bind_key.empty())
			return "";

		auto it = std::find_if(
			_fake_bin_params.begin(),
			_fake_bin_params.end(),
			[this](const FakeBinParam& _it) { return _it.key.contains(_fake_bind_key); }
		);

		if (it != _fake_bin_params.end())
		{
			std::string argument;

			if (key.contains("TLS"))
				argument = "--dpi-desync-fake-tls=" + (*it).file_clienthello;
			else if (key.contains("SEQOVL"))
				argument = "--dpi-desync-split-seqovl-pattern=" + (*it).file_clienthello;
			else if (key.contains("UNKNOWN"))
				argument = "--dpi-desync-fake-unknown-udp=" + (*it).file_initial;
			else
				argument = "--dpi-desync-fake-quic=" + (*it).file_initial;

			return argument;
		}
		else
		{
			std::string list_key{};
			for (auto& fake : _fake_bin_params)
				if (list_key.empty())
					list_key = fake.key;
				else
					list_key.append("," + fake.key);

			Debug::error("fake ключ [%s] не найден! Доступные ключи [%s].", _fake_bind_key.c_str(), list_key.c_str());
		}
	}

	return std::nullopt;
}
