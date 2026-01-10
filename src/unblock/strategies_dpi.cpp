#include "strategies_dpi.h"

StrategiesDPI::StrategiesDPI()
{
	_file_fake_bin_config->open(Core::get().configsPath() / "fake_bin", ".config", true);

	_file_fake_bin_config->forLineParametersSection(
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

	_file_fake_bin_config->forLineParametersSection(
		"FAKE_TLS_DOMAIN",
		[this](std::string key, std::string value)
		{
			auto& fake	= _fake_bin_params[key];
			fake.domain = value;
			fake.init	= true;
			return false;
		}
	);

	_file_fake_bin_config->forLineParametersSection(
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

	_file_blacklist_all->open(Core::get().userPath() / "all_blacklist", ".list", true);
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
	_filtering_top_level_domains = state;
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

void StrategiesDPI::_readFileStrategies(std::string section)
{
	_file_strategy_dpi->forLineSection(
		section.c_str(),
		[this](std::string _str)
		{
#if __clang__
			[[clang::no_destroy]]
#endif
			static std::string include_section{ "include_section>>" };
			if (_str.starts_with(include_section))
			{
				std::string result = _str.substr(include_section.length(), _str.length());

				_readFileStrategies(result);
				return false;
			}

			_saveStrategies(_str);
			return false;
		}
	);
}

void StrategiesDPI::_uploadStrategies()
{
	if (_file_strategy_dpi->isOpen())
	{
		_strategy_dpi.clear();
		_file_blacklist_all->clear();

		const std::string base_path_blacklist = (Core::get().configsPath() / "blacklist").string();

		for (auto& name : _section_opt_service_names)
		{
			auto service_blocklist_file = base_path_blacklist + "\\" + name + ".list";

			if (!std::filesystem::exists(service_blocklist_file))
				continue;

			File blacklist{};
			blacklist.open(service_blocklist_file, "", true);

			blacklist.forLine(
				[this](std::string str)
				{
					_file_blacklist_all->writeText(str);
					return false;
				}
			);
		}

		_file_blacklist_all->close();

		_service_blocklist_file = _file_blacklist_all->getPath().string();
		_readFileStrategies("START");

		// Optional strategy sections for individual services.
		for (auto& name : _section_opt_service_names)
		{
			_service_blocklist_file = base_path_blacklist + "\\" + name + ".list";
			_readFileStrategies(name);
		}

		_service_blocklist_file = _file_blacklist_all->getPath().string();
		_readFileStrategies("END");

		for (auto& line : _strategy_dpi)
			if (line.contains("=\""))
				line = std::regex_replace(line, std::regex{ "\\=" }, " ");

		std::erase_if(_strategy_dpi, [](std::string line) { return line.empty(); });

		if (_strategy_dpi.empty())
		{
			Debug::warning("strategy dpi empty");
			return;
		}

		while (_strategy_dpi.back().contains("--new"))
			_strategy_dpi.pop_back();

		for (auto& line : _strategy_dpi)
			Debug::ok("%s", line.c_str());
	}
}

void StrategiesDPI::_saveStrategies(std::string str)
{
	if (auto new_str = _getBlockList(str))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getGameFilter(str))
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

std::optional<std::string> StrategiesDPI::_getBlockList(std::string str) const
{
	if (str.contains("%BLOCKLIST%"))
	{
		if (_filtering_top_level_domains)
		{
			auto path_file_top_level_domains = Core::get().configsPath() / "top_level_domains.list";
			ASSERT_ARGS(
				std::filesystem::exists(path_file_top_level_domains),
				"The [%s] file does not exist!",
				path_file_top_level_domains.string().c_str()
			);
			return "--hostlist \"" + (path_file_top_level_domains.string()) + "\"";
		}

		ASSERT_ARGS(std::filesystem::exists(_service_blocklist_file), "The [%s] file does not exist!", _service_blocklist_file.c_str());
		return "--hostlist \"" + _service_blocklist_file + "\"";
	}

	if (str.contains("%DOMAINS-EXCLUDE%"))
	{
		auto path_file_domains_exclude = Core::get().configsPath() / "domains_exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_file_domains_exclude), "The [%s] file does not exist!", path_file_domains_exclude.string().c_str());
		return "--hostlist-exclude \"" + (path_file_domains_exclude.string()) + "\"";
	}

	if (str.contains("%IP-SETLIST%"))
	{
		if (std::find(_section_opt_service_names.begin(), _section_opt_service_names.end(), "game_mod") == _section_opt_service_names.end())
			return "";

		auto path_ip_set = Core::get().configsPath() / "ip-set-all.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_set), "The [%s] file does not exist!", path_ip_set.string().c_str());
		return "--ipset \"" + (path_ip_set.string()) + "\"";
	}

	if (str.contains("%IP-EXCLUDE%"))
	{
		auto path_ip_exclude = Core::get().configsPath() / "ip-exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_exclude), "The [%s] file does not exist!", path_ip_exclude.string().c_str());
		return "--ipset-exclude \"" + (path_ip_exclude.string()) + "\"";
	}

	return std::nullopt;
}

std::optional<std::string> StrategiesDPI::_getGameFilter(std::string str) const
{
	if (str.contains("%GameFilter%"))
	{
		std::string ports{ "0" };
		auto		it = std::find(_section_opt_service_names.begin(), _section_opt_service_names.end(), "game_mod");
		if (it != _section_opt_service_names.end())
			ports = "1024-65535";

		return std::regex_replace(str, std::regex{ "%GameFilter%" }, ports);
	}

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
