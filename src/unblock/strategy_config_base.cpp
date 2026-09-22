#include "strategy_config_base.h"

StrategyConfigBase::StrategyConfigBase()
{
	_file_strategy_dpi = std::make_shared<File>();
}

void StrategyConfigBase::changeStrategy(u32 index)
{
	const auto& strategy_file = _strategy_files_list[index];
	InputConsole::textInfo(Localization::Str{ "str_console_selected_config" }(), strategy_file);

	_file_strategy_dpi->open(_patch_file / strategy_file, "", true);

	_uploadStrategies();

	_file_strategy_dpi->close();
}

void StrategyConfigBase::changeStrategy(std::string_view file)
{
	auto it_file = std::ranges::find(_strategy_files_list, file);
	ASSERT_ARGS(it_file != _strategy_files_list.end(), "the file was not found, file{}!", file);

	std::string strategy_file{ *it_file };

	InputConsole::textInfo(Localization::Str{ "str_console_selected_config" }(), strategy_file);

	_file_strategy_dpi->open(_patch_file / strategy_file, "", true);

	_uploadStrategies();

	_file_strategy_dpi->close();
}

void StrategyConfigBase::changeDirVersion(std::string_view dir_version)
{
	_patch_dir_version = dir_version;

	_strategy_files_list.clear();

	if (_patch_dir_version.empty())
		_patch_file = Core::get().configsPath() / _strategyRootDir();
	else
		_patch_file = Core::get().configsPath() / _strategyRootDir() / _patch_dir_version;

	for (auto& entry : std::filesystem::directory_iterator(_patch_file))
	{
		auto& path = entry.path();

		if (std::filesystem::is_regular_file(path) && path.has_extension())
			if (path.extension() == ".config")
				_strategy_files_list.push_back(path.filename().string());
	}

	_sortFiles();
}

void StrategyConfigBase::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_file_service_list = config;
}

void StrategyConfigBase::changeOptionalServices(std::list<std::string> list_service)
{
	_section_opt_service_names = list_service;
}

void StrategyConfigBase::changeCustomLists(
	std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
)
{
	_generator.changeCustomLists(std::move(hosts), std::move(ip_set), std::move(domains_exclude), std::move(ip_exclude));
}

std::string StrategyConfigBase::getStrategyFileName() const
{
	return _file_strategy_dpi->name();
}

const std::vector<std::string>& StrategyConfigBase::getStrategyList() const
{
	return _strategy_files_list;
}

size_t StrategyConfigBase::getStrategySize() const
{
	return _strategy_files_list.size();
}

const std::vector<std::string>& StrategyConfigBase::getStrategy()
{
	return _strategy_dpi;
}

std::vector<std::string> StrategyConfigBase::listVersionDirs(const std::filesystem::path& root)
{
	std::vector<std::string> strategy_dirs{};

	for (auto& entry : std::filesystem::directory_iterator(root))
		strategy_dirs.push_back(entry.path().filename().string());

	std::ranges::sort(strategy_dirs, [](const std::string& left, const std::string& right) { return Core::get().isVersionNewer(left, right); });

	return strategy_dirs;
}

void StrategyConfigBase::_uploadFromGenerator()
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
}

void StrategyConfigBase::_saveStrategies(std::string_view str)
{
	if (auto new_str = _getPath(str, "%ROOT%", Core::get().currentPath()))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getPath(str, "%CONFIGS%", Core::get().configsPath()))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getPath(str, "%BIN%", Core::get().binPath()))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	if (auto new_str = _getPath(str, "%BINARIES%", Core::get().binariesPath()))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	// Zapret1 configs reference fake blobs that live in binaries/fake/.
	if (auto new_str = _getPath(str, "%FAKE%", Core::get().binariesPath() / "fake"))
	{
		_strategy_dpi.push_back(new_str.value());
		return;
	}

	_strategy_dpi.emplace_back(str);
}

void StrategyConfigBase::_normalizeStrategyString(std::string& str) const
{
	size_t pos;
	while ((pos = str.find("=,")) != std::string::npos)
		str.replace(pos, 2, "=");

	while ((pos = str.find(",,")) != std::string::npos)
		str.replace(pos, 2, ",");

	if (!str.empty() && str.back() == ',')
		str.pop_back();
}

bool StrategyConfigBase::_ignoringLineStrategy(std::string_view str) const
{
	static const std::regex r_n{ "\n" };
	return str.empty() || str.starts_with("//") || std::regex_match(std::string{ str }, r_n);
}

void StrategyConfigBase::_getAllPorts(std::string& str) const
{
	if (!_file_service_list)
		return;

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

			const std::string& setting_service_string = result.value();
			size_t			   position				  = 0;
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

std::optional<std::string> StrategyConfigBase::_getPath(std::string_view str, std::string_view prefix, std::filesystem::path path) const
{
	if (str.contains(prefix))
		return std::regex_replace(std::string{ str }, std::regex{ std::string{ prefix } }, path.string() + "\\");

	return std::nullopt;
}

void StrategyConfigBase::_sortFiles()
{
	std::ranges::sort(
		_strategy_files_list,
		[](const std::string& left, const std::string& right)
		{
			std::smatch				left_res;
			std::smatch				right_res;
			static const std::regex reg{ "\\d+" };
			if (std::regex_search(left, left_res, reg) && std::regex_search(right, right_res, reg))
				return std::stoul(left_res.str()) < std::stoul(right_res.str());

			return left.length() < right.length();
		}
	);
}
