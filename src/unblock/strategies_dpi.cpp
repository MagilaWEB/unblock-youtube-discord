#include "strategies_dpi.h"

StrategiesDPI::StrategiesDPI()
{
	for (auto& entry : std::filesystem::directory_iterator(Core::get().configsPath()))
	{
		const auto& name = entry.path().filename().string();
		if (name.contains("strategy_"))
			_strategy_files_list.push_back(name);
	}

	_file_feke_bin_config->open(Core::get().configsPath() / "feke_bin", ".config", true);

	_file_feke_bin_config->forLineParametrsSection(
		"FAKE_TLS",
		[this](std::string key, std::string value)
		{
			_feke_key_list.push_back(key);
			_feke_bin_params_tls.insert({ key, value });
			return false;
		}
	);

	_file_feke_bin_config->forLineParametrsSection(
		"FAKE_QUIC",
		[this](std::string key, std::string value)
		{
			_feke_bin_params_quic.insert({ key, value });
			return false;
		}
	);
}

void StrategiesDPI::changeStrategy(u32 index)
{
	const auto & strategy_file = _strategy_files_list[index];
	InputConsole::textInfo("Выбрана конфигурация [%s].", strategy_file.c_str());

	_file_strategy_dpi->open(Core::get().configsPath() / strategy_file, "", true);

	for (auto& v : _strategy_dpi)
		v.clear();

	_uploadStrategies();
}

void StrategiesDPI::changeStrategy(pcstr file)
{
	std::string strategy_file{};

	for (const auto& _file : _strategy_files_list)
		if (_file.contains(file))
			strategy_file = _file;

	InputConsole::textInfo("Выбрана конфигурация [%s].", strategy_file.c_str());

	_file_strategy_dpi->open(Core::get().configsPath() / strategy_file, "", true);

	for (auto& v : _strategy_dpi)
		v.clear();

	_uploadStrategies();
}

std::string StrategiesDPI::getStrategyFileName() const
{
	return _file_strategy_dpi->name();
}

u32 StrategiesDPI::getStrategySize() const
{
	return _strategy_files_list.size();
}

std::vector<std::string> StrategiesDPI::getStrategy(u32 service) const
{
	if (service >= _STRATEGY_DPI_MAX)
	{
		Debug::fatal(
			"getStrategy is an attempt to get a strategy vector for the service [%s] that does not match STRATEGY_DPI_MAX [%s]!",
			service,
			_STRATEGY_DPI_MAX
		);
		return {};
	}

	return _strategy_dpi[service];
}

std::string StrategiesDPI::getKeyFakeBin() const
{
	return _fake_bind_key;
}

std::vector<std::string> StrategiesDPI::getFekeBinList() const
{
	return _feke_key_list;
}

void StrategiesDPI::changeFakeKey(std::string key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	if (!_feke_bin_params_tls.contains(key))
		Debug::warning("для feke_bin_tls отсуствует тип %s", key.c_str());

	if (!_feke_bin_params_quic.contains(key))
		Debug::warning("для feke_bin_quic отсуствует тип %s", key.c_str());

	_fake_bind_key = key;
}

void StrategiesDPI::changeIgnoringHostlist(bool state)
{
	_ignoring_hostlist = state;
}

void StrategiesDPI::_uploadStrategies()
{
	if (_file_strategy_dpi->isOpen())
	{
		_file_strategy_dpi->forLineSection(
			"START_SERVICE",
			[this](std::string str)
			{
				for (auto& [index, service_name] : indexStrategies)
				{
					if (str.contains(service_name))
					{
						_file_strategy_dpi->forLineSection(
							service_name.c_str(),
							[this, &index](std::string str)
							{
								_saveStrategies(_strategy_dpi[index], str);
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
	if (auto new_str = _getPath(str, "%ROOT%", Core::get().currentPath()))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getPath(str, "%CONFIGS%", Core::get().configsPath()))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getPath(str, "%BIN%", Core::get().binPath()))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getPath(str, "%BINARIES%", Core::get().binariesPath()))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getBlockList(str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFakeTls(str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getFakeQuic(str))
	{
		strategy_dpi.push_back(new_str.value());
		return;
	}

	strategy_dpi.push_back(str);
}

std::optional<std::string> StrategiesDPI::_getPath(std::string str, std::string prefix, std::filesystem::path path) const
{
	if (str.contains(prefix))
	{
		utils::str_replace(str, prefix, path.string() + "\\");
		return str;
	}

	return std::nullopt;
}

std::optional<std::string> StrategiesDPI::_getBlockList(std::string str) const
{
	auto path_file = Core::get().configsPath() / "blacklist.list";

	if (str.contains("%BLOCKLIST%"))
	{
		if (_ignoring_hostlist)
			return "";

		if (std::filesystem::exists(path_file))
			return "--hostlist=" + (path_file.string());
		else
			Debug::error("Файл [%s] не существует!", path_file.string().c_str());
	}

	if (str.contains("%BLOCKLIST-GD-DPI%"))
	{
		if (_ignoring_hostlist)
			return "";

		if (std::filesystem::exists(path_file))
			return "--blacklist " + (path_file.string());
		else
			Debug::error("Файл [%s] не существует!", path_file.string().c_str());
	}

	path_file = Core::get().configsPath() / "ip-blacklist.list";

	if (str.contains("%IP-SETLIST%"))
	{
		if (_ignoring_hostlist)
			return "";

		if (std::filesystem::exists(path_file))
			return "--ipset=" + (path_file.string());
		else
			Debug::error("Файл [%s] не существует!", path_file.string().c_str());
	}

	if (str.contains("%IP-BLOCKLIST%"))
	{
		if (_ignoring_hostlist)
			return "";

		if (std::filesystem::exists(path_file))
			return "--hostlist=" + (path_file.string());
		else
			Debug::error("Файл [%s] не существует!", path_file.string().c_str());
	}

	return std::nullopt;
}

std::optional<std::string> StrategiesDPI::_getFakeTls(std::string str) const
{
	return _getFake("%FAKE_TLS%", _feke_bin_params_tls, str, "--dpi-desync-fake-tls=");
}

std::optional<std::string> StrategiesDPI::_getFakeQuic(std::string str) const
{
	return _getFake("%FAKE_QUIC%", _feke_bin_params_quic, str, "--dpi-desync-fake-quic=");
}

std::optional<std::string>
	StrategiesDPI::_getFake(pcstr key, const std::unordered_map<std::string, std::string>& feke_bin_params, std::string str, pcstr argument) const
{
	if (str.contains(key))
	{
		if (!_fake_bind_key.empty())
		{
			if (feke_bin_params.contains(_fake_bind_key))
			{
				const std::string& file		 = feke_bin_params.at(_fake_bind_key);
				auto			   path_file = Core::get().binariesPath() / file;
				if (std::filesystem::exists(path_file))
					return argument + path_file.string();
				else
					Debug::error("Файл [%s] не существует!", path_file.string().c_str());
			}
			else
			{
				std::string list_key{};
				for (auto& [key, _] : feke_bin_params)
					if (list_key.empty())
						list_key = key;
					else
						list_key.append("," + key);

				Debug::error("fake ключь [%s] не найден! Доступные ключи [%s].", _fake_bind_key.c_str(), list_key.c_str());
			}
		}
		else
			return "";
	}

	return std::nullopt;
}
