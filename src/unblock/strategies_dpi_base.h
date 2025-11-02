#pragma once
#include "../core/file_system.h"

inline constexpr std::pair<u32, pcstr> indexStrategies[]{
	{ 0,	 "unblock" },
	{ 1, "GoodbyeDPI" }
};

class StrategiesDPIBase
{
protected:
	constexpr static u32 _STRATEGY_DPI_MAX{ sizeof(indexStrategies) / sizeof(std::pair<u32, pcstr>) };
	Ptr<File>		_file_strategy_dpi;

	std::filesystem::path	 patch_file;
	std::vector<std::string> _strategy_files_list{};
	std::array<std::vector<std::string>, _STRATEGY_DPI_MAX> _strategy_dpi{};

public:
	StrategiesDPIBase()			 = default;
	virtual ~StrategiesDPIBase() = default;

	virtual void changeStrategy(u32 index = 1);
	virtual void changeStrategy(pcstr file);

	virtual std::string						getStrategyFileName() const;
	virtual const std::vector<std::string>& getStrategyList() const;
	virtual u32								getStrategySize() const;

protected:
	virtual void _uploadStrategies();
	virtual void _saveStrategies(std::vector<std::string>& strategy_dpi, std::string str);

	virtual std::optional<std::string> _getPath(std::string str, std::string prefix, std::filesystem::path path) const;
};
