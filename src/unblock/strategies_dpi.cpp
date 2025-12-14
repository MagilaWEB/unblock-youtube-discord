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
	changeFakeKey(strategy_file.key);
}

void StrategiesDPI::changeFakeKey(std::string key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	InputConsole::textInfo("Выбран FakeBin [%s].", key.c_str());

	auto it = std::find_if(_fake_bin_params.begin(), _fake_bin_params.end(), [&key](const FakeBinParam& _it) { return _it.key.contains(key); });

	ASSERT_ARGS(it != _fake_bin_params.end(), "a key is missing for fake_bin %s", key.c_str());

	_fake_bind_key = key;
}

void StrategiesDPI::changeFilteringTopLevelDomains(bool state)
{
	_filtering_top_level_domains = state;
}

void StrategiesDPI::addOptionalStrategies(std::string name)
{
	auto it = std::find(_section_opt_service_names.begin(), _section_opt_service_names.end(), name);
	if (it != _section_opt_service_names.end())
		return;

	_section_opt_service_names.emplace_back(name);
}

void StrategiesDPI::removeOptionalStrategies(std::string name)
{
	std::erase(_section_opt_service_names, name);
}

void StrategiesDPI::clearOptionalStrategies()
{
	_section_opt_service_names.clear();
}

bool StrategiesDPI::isFaked() const
{
	return _faked;
}

void StrategiesDPI::_readFileStrategies(std::string section, u32 index_service)
{
	_file_strategy_dpi->forLineSection(
		section.c_str(),
		[this, index_service](std::string _str)
		{
#if __clang__
			[[clang::no_destroy]]
#endif
			static std::string include_section{ "include_section>>" };
			if (_str.starts_with(include_section))
			{
				std::string result = _str.substr(include_section.length(), _str.length());

				_readFileStrategies(result, index_service);
				return false;
			}
			_saveStrategies(_strategy_dpi[index_service], _str);
			return false;
		}
	);
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
						_service_blocklist_file = "all.list";

						_readFileStrategies(service_name, index);

						// Optional strategy sections for individual services.
						for (auto& name : _section_opt_service_names)
						{
							std::string service_name_type{ service_name };
							service_name_type.append("_");
							service_name_type.append(name);

							_service_blocklist_file = name;
							_service_blocklist_file.append(".list");

							_readFileStrategies(service_name_type, index);
						}

						_service_blocklist_file = "all.list";
						_readFileStrategies("END", index);

						for (auto& line : _strategy_dpi[index])
							line.append(" ");

						while ((_strategy_dpi[index].back().contains("--new")) || (_strategy_dpi[index].empty()))
							_strategy_dpi[index].pop_back();
					}
#ifdef DEBUG
					for (auto& line : _strategy_dpi[index])
						Debug::ok("%s", line.c_str());
#endif
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
		strategy_dpi.emplace_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%FAKE_QUIC%", str))
	{
		strategy_dpi.emplace_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%FAKE_UNKNOWN%", str))
	{
		strategy_dpi.emplace_back(new_str.value());
		return;
	}

	if (auto new_str = _getFake("%SEQOVL_PATTERN%", str))
	{
		strategy_dpi.emplace_back(new_str.value());
		return;
	}

	StrategiesDPIBase::_saveStrategies(strategy_dpi, str);
}

std::optional<std::string> StrategiesDPI::_getBlockList(std::string str) const
{
	auto path_file					 = Core::get().configsPath() / "blacklist" / _service_blocklist_file;
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

	if (str.contains("%IP-SETLIST%"))
	{
		auto path_ip_set = Core::get().configsPath() / "ip-set-all.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_set), "The [%s] file does not exist!", path_ip_set.string().c_str());
		return "--ipset=" + (path_ip_set.string());
	}

	if (str.contains("%IP-EXCLUDE%"))
	{
		auto path_ip_exclude = Core::get().configsPath() / "ip-exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_exclude), "The [%s] file does not exist!", path_ip_exclude.string().c_str());
		return "--ipset-exclude=" + (path_ip_exclude.string());
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

		auto path_ip_blacklist = Core::get().configsPath() / "ip-blacklist.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_blacklist), "The [%s] file does not exist!", path_ip_blacklist.string().c_str());
		return "--hostlist=" + (path_ip_blacklist.string());
	}

	return std::nullopt;
}

std::optional<std::string> StrategiesDPI::_getFake(std::string key, std::string str) const
{
	if (str.contains(key))
	{
		if (_fake_bind_key.empty())
			return std::nullopt;

		auto it = std::find_if(
			_fake_bin_params.begin(),
			_fake_bin_params.end(),
			[this](const FakeBinParam& _it) { return _it.key.contains(_fake_bind_key); }
		);

		if (it != _fake_bin_params.end())
		{
			if (key == "%FAKE_TLS%")
				return "--dpi-desync-fake-tls=" + (*it).file_clienthello;

			if (key == "%FAKE_QUIC%")
				return "--dpi-desync-fake-quic=" + (*it).file_initial;

			if (key == "%SEQOVL_PATTERN%")
				return "--dpi-desync-split-seqovl-pattern=" + (*it).file_clienthello;

			if (key == "%FAKE_UNKNOWN%")
				return "--dpi-desync-fake-unknown-udp=" + (*it).file_initial;

			return std::nullopt;
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
