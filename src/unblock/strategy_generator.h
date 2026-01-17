#pragma once
class StrategyGenerator final
{
	std::shared_ptr<File>  _file_strategy{};
	std::list<std::string> _section_opt_service_names{};
	bool				   _filtering_top_level_domains{ false };

	inline static const std::filesystem::path _base_blacklist{ Core::get().configsPath() / "blacklist" };
	inline static const std::filesystem::path _base_ip_set{ Core::get().configsPath() / "ip-set" };
	inline static const std::filesystem::path _user_blacklist{ Core::get().userPath() / "blacklist" };
	inline static const std::filesystem::path _user_ip_set{ Core::get().userPath() / "ip-set" };

public:
	using map_filters = std::map<std::string, std::vector<std::string>>;

private:
	map_filters _map_filters{};

public:
	explicit StrategyGenerator();
	~StrategyGenerator()		 = default;

	void inFile(std::shared_ptr<File>& strategy);
	void changeServiceList(std::list<std::string>);
	void filteringTopDomain(bool state);

	const map_filters& mapFilters();

private:
	void					   _convertDataFiles();
	void					   _readFileFilters(std::string section);
	bool					   _useIn(std::string str, std::string section);
	std::optional<std::string> _getDataFile(std::string str, std::string section, bool all = false);
};
