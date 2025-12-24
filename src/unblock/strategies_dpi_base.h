#pragma once
#include "../core/file_system.h"

class StrategiesDPIBase
{
protected:
	Ptr<File> _file_strategy_dpi;

	std::filesystem::path	 patch_file;
	std::vector<std::string> _strategy_files_list{};
	std::vector<std::string> _strategy_dpi{};

public:
	StrategiesDPIBase()			 = default;
	virtual ~StrategiesDPIBase() = default;

	virtual void changeStrategy(u32 index = 1);
	virtual void changeStrategy(pcstr file);

	virtual std::string						getStrategyFileName() const;
	virtual const std::vector<std::string>& getStrategyList() const;
	virtual size_t							getStrategySize() const;

	const std::vector<std::string>& getStrategy();

protected:
	virtual void _uploadStrategies();
	virtual void _saveStrategies(std::string str);

	virtual std::optional<std::string> _getPath(std::string str, std::string prefix, std::filesystem::path path) const;
};
