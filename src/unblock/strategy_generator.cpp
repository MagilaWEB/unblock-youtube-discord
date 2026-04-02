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
		Debug::warning("file strategy not file [{}]", _file_strategy->getPath().string());
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
				for (auto& line : list)
				{
					if (line.empty() || line.starts_with("//") || std::regex_match(line, std::regex{ "\n" }))
						continue;

					file.writeText(line);
				}
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

void StrategyGenerator::_readFileFilters(std::string_view section)
{
	const bool start_end = section == "START" || section == "END";
	auto&		section_lines = _map_filters[section.data()];

	if (!section_lines.empty())
		return;

	_file_strategy->forLineSection(
		section,
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

bool StrategyGenerator::_useIn(std::string str, std::string_view section)
{
	constexpr static std::string_view use_in{ "use_in>>" };

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

			for (auto& line : to_list)
				to_list.writeText(line);
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

std::optional<std::string> StrategyGenerator::_getDataFile(std::string str, std::string_view section, bool all)
{
	if (str.contains("%BLOCKLIST%"))
	{
		std::filesystem::path path = _user_blacklist / (all ? "all" : section) += ".list";
		if (!std::filesystem::exists(path))
			return "";

		return "--hostlist=\"" + path.string() + "\"";
	}

	if (str.contains("%IP-SETLIST%"))
	{
		std::filesystem::path path = _user_ip_set / (all ? "all" : section) += ".list";
		if (!std::filesystem::exists(path))
			return "";
		return "--ipset=\"" + path.string() + "\"";
	}

	if (str.contains("%DOMAINS-EXCLUDE%"))
	{
		static const auto path_file_domains_exclude = Core::get().configsPath() / "domains_exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_file_domains_exclude), "The [{}] file does not exist!", path_file_domains_exclude.string());
		return "--hostlist-exclude=\"" + (path_file_domains_exclude.string()) + "\"";
	}

	if (str.contains("%IP-EXCLUDE%"))
	{
		static const auto path_ip_exclude = Core::get().configsPath() / "ip-exclude.list";
		ASSERT_ARGS(std::filesystem::exists(path_ip_exclude), "The [{}] file does not exist!", path_ip_exclude.string());
		return "--ipset-exclude=\"" + (path_ip_exclude.string()) + "\"";
	}

	return std::nullopt;
}
