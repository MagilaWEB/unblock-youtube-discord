#pragma once
#include "../core/file_system.h"

class StrategiesDPIBase
{
protected:
	std::shared_ptr<File> _file_strategy_dpi;

	std::filesystem::path	 _patch_file;
	std::filesystem::path	 _patch_dir_version{ "" };
	std::vector<std::string> _strategy_files_list{};
	std::vector<std::string> _strategy_dpi{};

public:
	StrategiesDPIBase();
	virtual ~StrategiesDPIBase() = default;

	virtual void changeStrategy(u32 index = 1);
	virtual void changeStrategy(std::string_view file);

	virtual void changeDirVersion(std::string_view dir_version);

	virtual std::string						getStrategyFileName() const;
	virtual const std::vector<std::string>& getStrategyList() const;
	virtual size_t							getStrategySize() const;

	const std::vector<std::string>& getStrategy();

protected:
	virtual void _uploadStrategies();
	virtual void _saveStrategies(std::string_view str);

	virtual std::optional<std::string> _getPath(std::string_view str, std::string_view prefix, std::filesystem::path path) const;

	void _sortFiles();
};
