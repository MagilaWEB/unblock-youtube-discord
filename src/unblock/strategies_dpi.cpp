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
			_fake_bin_params.push_back({ key, value });
			return false;
		}
	);

	_file_fake_bin_config->forLineParametersSection(
		"FAKE_QUIC",
		[this](std::string key, std::string value)
		{
			auto it = std::find_if(_fake_bin_params.begin(), _fake_bin_params.end(), [&key](const FakeBinParam& it) { return it.key.contains(key); });

			if (it != _fake_bin_params.end())
				(*it).value_quic = value;
			else
				_fake_bin_params.push_back({ key, {}, value });

			return false;
		}
	);
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

const std::vector<StrategiesDPI::FakeBinParam>& StrategiesDPI::getFakeBinList() const
{
	return _fake_bin_params;
}

void StrategiesDPI::changeFakeKey(std::string key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	auto it = std::find_if(_fake_bin_params.begin(), _fake_bin_params.end(), [&key](const FakeBinParam& it) { return it.key.contains(key); });

	if (it == _fake_bin_params.end())
		Debug::warning("для fake_bin отсутствует ключ %s", key.c_str());

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

	__super::_saveStrategies(strategy_dpi, str);
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

std::optional<std::string> StrategiesDPI::_getFake(std::string key, std::string str) const
{
	if (str.contains(key))
	{
		if (_fake_bind_key.empty())
			return "";

		auto it = std::find_if(
			_fake_bin_params.begin(),
			_fake_bin_params.end(),
			[this](const FakeBinParam& it) { return it.key.contains(_fake_bind_key); }
		);

		if (it != _fake_bin_params.end())
		{
			const bool is_tls = key.contains("TLS");

			const std::string& file		 = is_tls ? (*it).value_tls : (*it).value_quic;
			auto			   path_file = Core::get().binariesPath() / file;

			if (std::filesystem::exists(path_file))
				return (is_tls ? "--dpi-desync-fake-tls=" : "--dpi-desync-fake-quic=") + path_file.string();
			else
				Debug::error("Файл [%s] не существует!", path_file.string().c_str());
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
