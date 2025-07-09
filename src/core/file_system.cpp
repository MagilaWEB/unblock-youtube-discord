#include "pch.h"
#include "file_system.h"

FileSystem::FileSystem()
{
	_stream.exceptions(std::ios::badbit);
}

FileSystem::~FileSystem()
{
	close();
}

void FileSystem::forLine(std::function<bool(std::string)>&& fn)
{
	if (_line_string.empty())
		return;

	for (auto& str : _line_string)
		if (fn(str))
			break;
}

FileSystem::OptionsVaildParamerts FileSystem::_validParametr(pcstr section, const std::string& is_sec)
{
	static bool entered_section{ false };

	if (is_sec.empty())
		return { entered_section, false, false };

	const static std::regex regex_section_name{ "\\[.*\\](?:.*|\\n)" };
	const bool presunably_section = std::regex_match(is_sec, regex_section_name);

	if (entered_section == false && presunably_section && is_sec.contains(section))
		entered_section = true;
	else if (entered_section && !presunably_section)
		return { true, false, true };
	else if (presunably_section)
	{
		entered_section = false;
		return { false, true, false };
	}

	return { entered_section, false, false };
}

void FileSystem::forLineSection(pcstr section, std::function<bool(std::string str)>&& fn)
{
	for (auto& str : _line_string)
	{
		const auto result = _validParametr(section, str);

		if (result.ran_paramert)
		{
			if (fn(str))
				break;
		}
		else if (result.section_end)
			break;
	}
}

void FileSystem::forLineParametrsSection(pcstr section, std::function<bool(std::string key, std::string value)>&& fn)
{
	forLineSection(
		section,
		[&](std::string str)
		{
			size_t pos = str.find_first_of("=");
			if (pos != std::string::npos)
			{
				const auto& key	  = str.substr(0, pos);
				const auto& value = str.substr(++pos, str.size());
				return fn(key, value);
			}
			else
			{
				Debug::error(
					"при попытке получить ключи и значения в файле[%s] в секции [%s] отсусвует разменователь [=]! Строка следующего вида [%s].",
					name().c_str(),
					section,
					str.c_str()
				);
			}

			return false;
		}
	);
}

std::expected<std::string, std::string> FileSystem::parametrSection(pcstr section, pcstr paramert)
{
	std::optional<std::string> kay_value{ std::nullopt };
	forLineParametrsSection(
		section,
		[&kay_value, paramert](std::string key, std::string value)
		{
			if (key.contains(paramert))
			{
				kay_value = value;
				return true;
			}

			return false;
		}
	);

	if (kay_value)
		return kay_value.value();

	return Debug::str_unexpected("Не удалось найти параметр [%s] в секции [%s]!", paramert, section);
}

void FileSystem::writeSectionParametr(pcstr section, pcstr paramert, pcstr value)
{
	auto insert = [=](auto&& it)
	{
		std::string str{ paramert };
		str += "=";
		str += value;
		_line_string.insert(it, str);
	};

	for (auto it = _line_string.begin(); it != _line_string.end(); it++)
	{
		auto& str = *it;

		const auto result = _validParametr(section, str);

		if (result.ran_paramert)
		{
			size_t pos = str.find_first_of("=");
			if (pos != std::string::npos)
			{
				const auto& key	  = str.substr(0, pos);
				const auto& value = str.substr(++pos, str.size());
				if (key.contains(paramert))
				{
					str = std::regex_replace(str, std::regex{ value }, value);
					return;
				}
			}
		}
		else if (result.section_end)
		{
			insert(--it);
			return;
		}
		else if (result.entered_section && !result.section_end)
		{
			insert(++it);
			return;
		}
	}

	if (!_line_string.empty())
		_line_string.push_back("\\n");

	std::string str{ "[]" };
	_line_string.push_back(str.insert(1, section));
	str.clear();

	str	 = paramert;
	str += "=";
	str += value;
	_line_string.push_back(str);
}

std::string FileSystem::name() const
{
	return _path_file.filename().string();
}

bool FileSystem::isOpen() const
{
	return _open_state;
}

void FileSystem::open(std::filesystem::path file, pcstr expansion, bool no_default_patch)
{
	if (isOpen())
		close();

	if (!no_default_patch)
		_path_file = (Core::get().currentPath() / file) += expansion;
	else
		_path_file = file += expansion;

	try
	{
		_stream.open(_path_file, std::ios::in);

		_line_string.clear();

		std::string str;
		while (getline(_stream, str))
			_line_string.push_back(str);

		_stream.close();
	}
	catch (const std::ios::failure& error)
	{
		Debug::error("Faile open fail [%s] error [%s]!", _path_file.string().c_str(), error.what());
		_open_state = false;
		return;
	}

	_open_state = true;
}

void FileSystem::close()
{
	_stream.open(_path_file, std::ios::out | std::ios::binary);
	for (auto& _string : _line_string)
		_stream << _string << std::endl;
	_stream.close();
	_open_state = false;
}
