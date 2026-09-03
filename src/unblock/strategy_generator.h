#pragma once
class StrategyGenerator final
{
	std::shared_ptr<File>  _file_strategy{};
	std::list<std::string> _section_opt_service_names{};

	std::vector<std::string> _custom_hosts{};
	std::vector<std::string> _custom_ip_set{};
	std::vector<std::string> _custom_domains_exclude{};
	std::vector<std::string> _custom_ip_exclude{};

	static const std::filesystem::path& _base_blacklist();
	static const std::filesystem::path& _base_ip_set();
	static const std::filesystem::path& _user_blacklist();
	static const std::filesystem::path& _user_ip_set();
	static const std::filesystem::path& _user_domains_exclude();
	static const std::filesystem::path& _user_ip_exclude();

public:
	using map_filters = std::map<std::string, std::vector<std::string>>;

private:
	map_filters _map_filters{};

public:
	explicit StrategyGenerator();
	~StrategyGenerator() = default;

	void inFile(std::shared_ptr<File>& strategy);
	void changeServiceList(std::list<std::string>);

	void changeCustomLists(
		std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
	);

	const map_filters& mapFilters();

private:
	void					   _convertDataFiles();
	void					   _readFileFilters(std::string_view section);
	bool					   _useIn(std::string str, std::string_view section);
	std::optional<std::string> _getDataFile(std::string str, std::string_view section, bool all = false);

	static void
		_buildUserExclude(const std::filesystem::path& base_path, const std::filesystem::path& user_path, const std::vector<std::string>& custom);
	static void _appendLines(const std::filesystem::path& file_path, const std::vector<std::string>& items);
};
