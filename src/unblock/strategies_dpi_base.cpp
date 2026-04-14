#include "strategies_dpi_base.h"

StrategiesDPIBase::StrategiesDPIBase()
{
	_file_strategy_dpi = std::make_shared<File>();
}

void StrategiesDPIBase::changeStrategy(u32 index)
{
	const auto& strategy_file = _strategy_files_list[index];
	InputConsole::textInfo(Localization::Str{ "str_console_selected_config" }(), strategy_file);

	_file_strategy_dpi->open(_patch_file / strategy_file, "", true);

	_uploadStrategies();

	_file_strategy_dpi->close();
}

void StrategiesDPIBase::changeStrategy(std::string_view file)
{
	auto it_file = std::ranges::find(_strategy_files_list, file);
	ASSERT_ARGS(it_file != _strategy_files_list.end(), "the file was not found, file{}!", file);

	std::string strategy_file{ *it_file };

	InputConsole::textInfo(Localization::Str{ "str_console_selected_config" }(), strategy_file);

	_file_strategy_dpi->open(_patch_file / strategy_file, "", true);

	_uploadStrategies();

	_file_strategy_dpi->close();
}

void StrategiesDPIBase::changeDirVersion(std::string_view dir_version)
{
	_patch_dir_version = dir_version;
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

void StrategiesDPIBase::_saveStrategies(std::string_view str)
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

	_strategy_dpi.emplace_back(str);
}

std::optional<std::string> StrategiesDPIBase::_getPath(std::string_view str, std::string_view prefix, std::filesystem::path path) const
{
	if (str.contains(prefix))
		return std::regex_replace(str.data(), std::regex{ prefix.data() }, path.string() + "\\");

	return std::nullopt;
}

void StrategiesDPIBase::_sortFiles()
{
	std::ranges::sort(
		_strategy_files_list,
		[](const std::string& left, const std::string& right)
		{
			std::smatch		  left_res;
			std::smatch		  right_res;
			static const std::regex reg("\\d+");
			if (std::regex_search(left, left_res, reg) && std::regex_search(right, right_res, reg))
				return std::stoul(left_res.str()) < std::stoul(right_res.str());

			return left.length() < right.length();
		}
	);
}
