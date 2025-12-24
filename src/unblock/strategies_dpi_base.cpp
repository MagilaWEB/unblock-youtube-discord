#include "strategies_dpi_base.h"

void StrategiesDPIBase::changeStrategy(u32 index)
{
	const auto& strategy_file = _strategy_files_list[index];
	InputConsole::textInfo("Выбрана конфигурация [%s].", strategy_file.c_str());

	_file_strategy_dpi->open(patch_file / strategy_file, "", true);

	_strategy_dpi.clear();

	_uploadStrategies();
}

void StrategiesDPIBase::changeStrategy(pcstr file)
{
	std::string strategy_file{};

	for (const auto& _file : _strategy_files_list)
		if (_file.contains(file))
			strategy_file = _file;

	InputConsole::textInfo("Выбрана конфигурация [%s].", strategy_file.c_str());

	_file_strategy_dpi->open(patch_file / strategy_file, "", true);

	_strategy_dpi.clear();

	_uploadStrategies();
}

std::string StrategiesDPIBase::getStrategyFileName() const
{
	return _file_strategy_dpi->name();
}

const std::vector<std::string>& StrategiesDPIBase::getStrategyList() const
{
	return _strategy_files_list;
}

size_t StrategiesDPIBase::getStrategySize() const
{
	return _strategy_files_list.size();
}

const std::vector<std::string>& StrategiesDPIBase::getStrategy()
{
	return _strategy_dpi;
}

void StrategiesDPIBase::_uploadStrategies()
{
	if (_file_strategy_dpi->isOpen())
	{
		_strategy_dpi.clear();

		_file_strategy_dpi->forLine(
			[this](std::string str)
			{
				_saveStrategies(str);
				return false;
			}
		);
	}
}

void StrategiesDPIBase::_saveStrategies(std::string str)
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

	_strategy_dpi.push_back(str);
}

std::optional<std::string> StrategiesDPIBase::_getPath(std::string str, std::string prefix, std::filesystem::path path) const
{
	if (str.contains(prefix))
	{
		str = std::regex_replace(str, std::regex{ prefix }, path.string() + "\\");
		return str;
	}

	return std::nullopt;
}
