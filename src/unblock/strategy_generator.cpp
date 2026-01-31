#include "strategy_generator.h"

StrategyGenerator::StrategyGenerator()
{
	if (!std::filesystem::is_directory(_user_blacklist))
		std::filesystem::create_directory(_user_blacklist);

	if (!std::filesystem::is_directory(_user_ip_set))
		std::filesystem::create_directory(_user_ip_set);
}

void StrategyGenerator::inFile(std::shared_ptr<File>& strategy)
{
	_file_strategy = strategy;

	if (!_file_strategy->isOpen())
	{
		Debug::warning("file strategy not file [%s]", _file_strategy->getPath().string().c_str());
		return;
	}

	_map_filters.clear();

	_convertDataFiles();

	_readFileFilters("START");

	for (auto& service : _section_opt_service_names)
		_readFileFilters(service);

	_readFileFilters("END");
}

void StrategyGenerator::changeServiceList(std::list<std::string> list)
{
	_section_opt_service_names = list;
}

void StrategyGenerator::filteringTopDomain(bool state)
{
	_filtering_top_level_domains = state;
}

const StrategyGenerator::map_filters& StrategyGenerator::mapFilters()
{
	return _map_filters;
}

void StrategyGenerator::_convertDataFiles()
{
	File black_list_all{ false };
	black_list_all.open(_user_blacklist / "all", ".list", true);

	if (black_list_all.isOpen())
		black_list_all.clear();

	File ip_set_all{ false };
	ip_set_all.open(_user_ip_set / "all", ".list", true);

	if (ip_set_all.isOpen())
		ip_set_all.clear();

	for (auto& service : _section_opt_service_names)
	{
		auto all_files_to_filters = [&service](File& file, std::filesystem::path base, bool clear)
		{
			if (clear && file.isOpen())
				file.clear();

			File list{ false };
			list.open(base / service, ".list", true);

			if (list.isOpen())
			{
				list.forLine(
					[&file](std::string line)
					{
						if (line.empty() || line.starts_with("//") || std::regex_match(line, std::regex{ "\n" }))
							return false;

						file.writeText(line);
						return false;
					}
				);
			}
		};

		all_files_to_filters(black_list_all, _base_blacklist, false);
		all_files_to_filters(ip_set_all, _base_ip_set, false);

		File new_blacklist{ false };
		new_blacklist.open(_user_blacklist / service, ".list", true);

		File new_ip_set{ false };
		new_ip_set.open(_user_ip_set / service, ".list", true);

		all_files_to_filters(new_blacklist, _base_blacklist, true);
		all_files_to_filters(new_ip_set, _base_ip_set, true);
	}
}

void StrategyGenerator::_readFileFilters(std::string section)
{
	const bool start_end = section == "START" || section == "END";

	auto& section_lines = _map_filters[section];

	if (!section_lines.empty())
		return;

	_file_strategy->forLineSection(
		section.c_str(),
		[&](std::string str)
		{
			if (_useIn(str, section))
				return false;

			if (auto new_data = _getDataFile(str, section, start_end))
				str = new_data.value();

			section_lines.push_back(str);
			return false;
		}
	);
}

bool StrategyGenerator::_useIn(std::string str, std::string section)
{
#if __clang__
	[[clang::no_destroy]]
#endif
	static std::string use_in{ "use_in>>" };

	if (str.starts_with(use_in))
	{
		std::string result = str.substr(use_in.length(), str.length());
		if (result == section)
			return true;

		auto copy_line_from_file_to_file = [&section, result](std::filesystem::path path)
		{
			File from_list{ false };
			from_list.open(path / section, ".list", true);

			File to_list{ false };
			to_list.open(path / result, ".list", true);

			from_list.forLine(
				[&to_list](std::string line)
				{
					to_list.writeText(line);
					return false;
				}
			);
		};

		if (_map_filters[result].empty())
		{
			if (std::ranges::find(_section_opt_service_names, result) == _section_opt_service_names.end())
			{
				File blacklist{ false };
				blacklist.open(_user_blacklist / result, ".list", true);

				if (blacklist.isOpen())
					blacklist.clear();

				File ip_set{ false };
				ip_set.open(_user_ip_set / result, ".list", true);

				if (ip_set.isOpen())
					ip_set.clear();
			}

			copy_line_from_file_to_file(_user_blacklist);
			copy_line_from_file_to_file(_user_ip_set);

			_readFileFilters(result);
		}
		else
		{
			copy_line_from_file_to_file(_user_blacklist);
			copy_line_from_file_to_file(_user_ip_set);
		}

		return true;
	}

	return false;
}

std::optional<std::string> StrategyGenerator::_getDataFile(std::string str, std::string section, bool all)
{
	if (str.contains("%BLOCKLIST%"))
	{
		if (_filtering_top_level_domains && all)
		{
#if __clang__
			[[clang::no_destroy]]
#endif
			static const auto path_file_top_level_domains = Core::get().configsPath() / "top_level_domains.list";
			ASSERT_ARGS(
				std::filesystem::exists(path_file_top_level_domains),
				"The [%s] file does not exist!",
				path_file_top_level_domains.string().c_str()
			);
			return "--hostlist \"" + (path_file_top_level_domains.string()) + "\"";
		}

		std::filesystem::path path = _user_blacklist / (all ? "all" : section) += ".list";
		if (!std::filesystem::exists(path))
			return "";

		return "--hostlist \"" + path.string() + "\"";
	}

	if (str.contains("%IP-SETLIST%"))
	{
		std::filesystem::path path = _user_ip_set / (all ? "all" : section) += ".list";
		if (!std::filesystem::exists(path))
			return "";
		return "--ipset \"" + path.string() + "\"";
	}

	if (str.contains("%DOMAINS-EXCLUDE%"))
	{
#if __clang__
		[[clang::no_destroy]]
#endif
		static const auto path_file_domains_exclude = Core::get().configsPath() / "domains_exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_file_domains_exclude), "The [%s] file does not exist!", path_file_domains_exclude.string().c_str());
		return "--hostlist-exclude \"" + (path_file_domains_exclude.string()) + "\"";
	}

	if (str.contains("%IP-EXCLUDE%"))
	{
#if __clang__
		[[clang::no_destroy]]
#endif
		static const auto path_ip_exclude = Core::get().configsPath() / "ip-exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_exclude), "The [%s] file does not exist!", path_ip_exclude.string().c_str());
		return "--ipset-exclude \"" + (path_ip_exclude.string()) + "\"";
	}

	return std::nullopt;
}
