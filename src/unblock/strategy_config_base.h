#pragma once
#include "../core/file_system.h"
#include "strategy_generator.h"

class StrategyConfigBase
{
protected:
	std::shared_ptr<File> _file_strategy_dpi;
	std::shared_ptr<File> _file_service_list;

	std::filesystem::path	 _patch_file;
	std::filesystem::path	 _patch_dir_version{ "" };
	std::vector<std::string> _strategy_files_list{};
	std::vector<std::string> _strategy_dpi{};

	std::list<std::string> _section_opt_service_names{};
	StrategyGenerator	   _generator;

public:
	StrategyConfigBase();
	virtual ~StrategyConfigBase() = default;

	virtual void changeStrategy(u32 index = 1);
	virtual void changeStrategy(std::string_view file);

	virtual void changeDirVersion(std::string_view dir_version);

	void serviceConfigFile(const std::shared_ptr<File>& config);
	void changeOptionalServices(std::list<std::string> list_service);
	void changeCustomLists(
		std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
	);

	virtual std::string						getStrategyFileName() const;
	virtual const std::vector<std::string>& getStrategyList() const;
	virtual size_t							getStrategySize() const;

	const std::vector<std::string>& getStrategy();

	// Sorted version directory names under the given strategy root.
	static std::vector<std::string> listVersionDirs(const std::filesystem::path& root);

protected:
	virtual void _uploadStrategies() = 0;
	virtual void _saveStrategies(std::string_view str);

	// Strategy config root under configsPath() ("strategy" / "strategy_zapret1").
	virtual std::filesystem::path _strategyRootDir() const = 0;

	// Shared pipeline: expands the generator sections into _strategy_dpi
	// through the virtual _saveStrategies(). Subclasses append their own
	// finalization (lua numbering, fake expansion is inside _saveStrategies).
	void _uploadFromGenerator();

	void _normalizeStrategyString(std::string& str) const;
	bool _ignoringLineStrategy(std::string_view str) const;
	void _getAllPorts(std::string& str) const;

	virtual std::optional<std::string> _getPath(std::string_view str, std::string_view prefix, std::filesystem::path path) const;

	void _sortFiles();
};
