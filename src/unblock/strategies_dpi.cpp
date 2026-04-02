#include "strategies_dpi.h"

StrategiesDPI::StrategiesDPI()
{
	_file_lua_init.open(Core::get().configsPath() / "lua_init", ".config", true);
	_file_fake_bin_config.open(Core::get().configsPath() / "fake_bin", ".config", true);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_TLS",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / "fake" / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake = _fake_bin_params[key.data()];
			fake.file  = path_file.string();
			return false;
		}
	);

	_file_fake_bin_config.forLineParametersSection(
		"FAKE_QUIC",
		[this](std::string_view key, std::string_view value)
		{
			const auto path_file = Core::get().binariesPath() / "fake" / value;
			ASSERT_ARGS(std::filesystem::exists(path_file), "The [{}] file does not exist!", path_file.string());

			auto& fake = _fake_bin_params[key.data()];
			fake.file  = path_file.string();
			return false;
		}
	);

	_file_fake_bin_config.close();
}

void StrategiesDPI::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_file_service_list = config;
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

	// for (auto& line : _strategy_dpi)
	//	if (line.contains("=\""))
	//		line = std::regex_replace(line, std::regex{ "\\=" }, " ");

	_init_lua_to_zapret();
	_blob_init_to_zapret();

	_normalizeStrategyFinal();
}

void StrategiesDPI::_saveStrategies(std::string_view str)
{
	if (_ignoringLineStrategy(str))
		return;

	StrategiesDPIBase::_saveStrategies(str);

	auto& string_back = _strategy_dpi.back();
	_getAllPorts(string_back);
}

void StrategiesDPI::_init_lua_to_zapret()
{
	static const std::filesystem::path _lua_dir{ Core::get().binariesPath() / "lua" };

	auto iter = [this] { return std::ranges::find(_strategy_dpi, "%INIT_LUA%"); };
	if (iter() != _strategy_dpi.end())
	{
		for (auto& line : _file_lua_init)
			_strategy_dpi.insert(iter(), "--lua-init=@\"" + (_lua_dir / line).string() + "\"");

		_strategy_dpi.erase(iter());
	}
}

void StrategiesDPI::_blob_init_to_zapret()
{
	auto iter = [this] { return std::ranges::find(_strategy_dpi, "%INIT_BLOB%"); };
	if (iter() != _strategy_dpi.end())
	{
		for (auto& [key, data] : _fake_bin_params)
			if (data.init)
				data.init = false;

		static const std::regex pattern(R"(:blob=([^:]+))");

		auto& file_strategy = *_file_strategy_dpi;
		for (auto& line : file_strategy)
		{
			for (auto& [key, data] : _fake_bin_params)
			{
				if (!data.init)
				{
					std::smatch match;
					if (std::regex_search(line, match, pattern))
						if (match[1].str() == key)
							data.init = true;
				}
			}
		}

		for (auto& [key, data] : _fake_bin_params)
			if (data.init)
				_strategy_dpi.insert(iter(), "--blob=" + key + ":@\"" + data.file + "\"");

		for (auto& line : file_strategy)
		{
			std::smatch match;
			if (std::regex_search(line, match, pattern))
			{
				if (_fake_bin_params.find(match[1].str()) == _fake_bin_params.end())
				{
					Debug::warning(
						"The blob {} key does not exist in unblock, register it in the fake_bin.config config or use the existing ones."
						"The strategy string is [{}].",
						match[1].str(),
						line
					);
				}
			}
		}

		_strategy_dpi.erase(iter());
	}
}

void StrategiesDPI::_normalizeStrategyString(std::string& str) const
{
	size_t pos;
	while ((pos = str.find("=,")) != std::string::npos)
		str.replace(pos, 2, "=");

	while ((pos = str.find(",,")) != std::string::npos)
		str.replace(pos, 2, ",");

	if (!str.empty() && str.back() == ',')
		str.pop_back();
}

void StrategiesDPI::_normalizeStrategyFinal()
{
	for (auto& line : _strategy_dpi)
	{
		static std::regex port_vars(R"(%[^%]+%)");
		line = std::regex_replace(line, port_vars, "");

		_normalizeStrategyString(line);
	}

	std::erase_if(_strategy_dpi, [](std::string line) { return line.empty(); });

	if (_strategy_dpi.empty())
	{
		Debug::error("strategy dpi empty");
		return;
	}

	while (_strategy_dpi.back().starts_with("--new"))
		_strategy_dpi.pop_back();

	for (auto& line : _strategy_dpi)
		Debug::ok("{}", line);
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
				std::smatch				para;
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
