#include "strategies_dpi.h"

StrategiesDPI::StrategiesDPI()
{
	_file_fake_bin_config.open(Core::get().configsPath() / "fake_bin", ".config", true);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake			  = _fake_bin_params[key.data()];
			fake.file_clienthello = path_file.string();
			fake.init			  = true;
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS_DOMAIN",
		[this](std::string_view key, std::string_view value)
		{
			auto& fake	= _fake_bin_params[key.data()];
			fake.domain = value;
			fake.init	= true;
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_QUIC",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake		  = _fake_bin_params[key.data()];
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

void StrategiesDPI::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_file_service_list = config;
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

void StrategiesDPI::changeFakeKey(std::string_view key)
{
	if (key.empty())
	{
		_fake_bind_key = "";
		return;
	}

	InputConsole::textInfo("Выбран FakeBin [{}].", key);

	auto& fake_bin = _fake_bin_params[key.data()];

	ASSERT_ARGS(fake_bin.init, "a key is missing for fake_bin {}", key);

	_fake_bind_key = key;
}

void StrategiesDPI::changeDirVersion(std::string_view dir_version)
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

	std::vector<std::pair<u32, std::list<std::string>>> sort_service_filters{};

	auto& map = _generator.mapFilters();
	for (auto& [key, list] : map)
	{
		if (list.empty())
			continue;

		if (auto position = _file_strategy_dpi->positionSection(key))
		{
			auto& new_list = sort_service_filters.emplace_back(position.value(), std::list<std::string>{});
			for (auto& line : list)
				new_list.second.push_back(line);
		}
	}

	std::ranges::sort(sort_service_filters, [](const auto& left, const auto& right) { return left.first < right.first; });

	for (auto& pair : sort_service_filters)
		for (auto& line : pair.second)
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
		Debug::ok("{}", line);
}

void StrategiesDPI::_saveStrategies(std::string_view str)
{
	if (_ignoringLineStrategy(str))
		return;

	if (auto new_str = _getFake(str))
	{
		_strategy_dpi.emplace_back(new_str.value());
		return;
	}

	StrategiesDPIBase::_saveStrategies(str);

	auto& string_back = _strategy_dpi.back();
	_getAllPorts(string_back);
	_normalizeStrategyString(string_back);
}

void StrategiesDPI::_normalizeStrategyString(std::string& str) const
{
	static std::regex port_vars(R"(%[^%]+%)");

	str = std::regex_replace(str, port_vars, "");

	size_t pos;
	while ((pos = str.find("=,")) != std::string::npos)
		str.replace(pos, 2, "=");

	while ((pos = str.find(",,")) != std::string::npos)
		str.replace(pos, 2, ",");

	if (!str.empty() && str.back() == ',')
		str.pop_back();
}

bool StrategiesDPI::_ignoringLineStrategy(std::string_view str) const
{
	return str.empty() || str.starts_with("//") || std::regex_match(str.data(), std::regex{ "\n" });
}

void StrategiesDPI::_getAllPorts(std::string& str) const
{
	for (auto& name_service : _section_opt_service_names)
	{
		if (auto result = _file_service_list->parameterSection<std::string>("PORTS_LIST", name_service))
		{
			auto replace_target = [&str, &name_service](const std::string& _text)
			{
				static const std::regex reg_equally{ "\\:" };
				std::smatch para;
				if (std::regex_search(_text, para, reg_equally))
				{
					std::string target{ std::format("%{}%", para.prefix().str()) };

					if (str.contains(target))
						str = std::regex_replace(str, std::regex{ target }, para.suffix().str());
				}
				else
					Debug::warning("_getAllPorts Separator not found : for [{}]", name_service);
			};

			std::string setting_service_string{ result.value() };
			size_t		position = 0;
			while (position < setting_service_string.length())
			{
				size_t found = setting_service_string.find(">>", position);
				if (found == std::string::npos)
				{
					replace_target(setting_service_string.substr(position));
					break;
				}

				replace_target(setting_service_string.substr(position, found - position));
				position = found + 2;
			}
		}
	}
}

std::optional<std::string> StrategiesDPI::_getFake(std::string_view str)
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
			pcstr str_data = str.data();
			if (fake.domain.empty())
				return std::regex_replace(str_data, std::regex{ "%FAKE_HOST_DOMAIN%" }, "--dpi-desync-hostfakesplit-mod=host=www.google.com");

			return std::regex_replace(str_data, std::regex{ "%FAKE_HOST_DOMAIN%" }, "--dpi-desync-hostfakesplit-mod=host=" + fake.domain);
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

			return std::regex_replace(str.data(), std::regex{ "%FAKE_CLIENT_HELLO%" }, fake.file_clienthello);
		}

		if (str.contains("%FAKE_INITIAL%"))
		{
			if (fake.file_initial.empty())
				return "";

			return std::regex_replace(str.data(), std::regex{ "%FAKE_INITIAL%" }, fake.file_initial);
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

		Debug::error(Localization::Str{ "str_console_not_find_fake_key" }(), _fake_bind_key, list_key);
	}

	return std::nullopt;
}
