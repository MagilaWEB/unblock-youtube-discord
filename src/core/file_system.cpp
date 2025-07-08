#include "pch.h"
#include "file_system.h"

const static std::regex regex_section_name{ "(?:.*|\\n)\\[.*\\](?:.*|\\n)" };

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

void FileSystem::forLineSection(pcstr section, std::function<bool(std::string str)>&& fn)
{
	bool is_section{ false };

	forLine(
		[=, &is_section](std::string str)
		{
			if (str.empty())
				return false;

			const bool r_regex = std::regex_match(str, regex_section_name);
			if (is_section == false && r_regex && str.contains(section))
				is_section = true;
			else if (!is_section)
				return false;
			else if (!r_regex)
				return fn(str);
			else if (r_regex)
				return true;

			return false;
		}
	);
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
	bool is_section{ false };

	for (auto& str : _line_string)
	{
		if (!str.empty())
		{
			const bool r_regex = std::regex_match(str, regex_section_name);
			if (is_section == false && r_regex && str.contains(section))
				is_section = true;
			else if (is_section && !r_regex)
			{
				size_t pos = str.find_first_of("=");
				if (pos != std::string::npos)
				{
					const auto& key	  = str.substr(0, pos);
					const auto& value = str.substr(++pos, str.size());
					if (key.contains(paramert))
					{
						str = std::regex_replace(str, std::regex(value), value);
						return;
					}
				}
			}
			else if (r_regex)
				return;
		}
	}

	if (is_section)
	{
		is_section = false;

		auto it = _line_string.begin();

		auto insert = [&]
		{
			std::stringstream s_stream{};
			s_stream << paramert << "=" << value;
			it++;
			_line_string.insert(it, s_stream.str());
		};

		for (; it != _line_string.end(); it++)
		{
			const auto& str = *it;

			if (!str.empty())
			{
				const bool r_regex = std::regex_match(str, regex_section_name);
				if (is_section == false && r_regex && str.contains(section))
					is_section = true;
				else if (is_section && !r_regex)
				{
					insert();
					return;
				}
				else if (r_regex)
					return;
			}
		}

		insert();

		return;
	}

	std::stringstream s_stream{};
	if (!_line_string.empty())
	{
		s_stream << std::endl;
		_line_string.push_back(s_stream.str());
		s_stream.str("");
	}

	s_stream << "[" << section << "]";
	_line_string.push_back(s_stream.str());

	s_stream.str("");
	s_stream << paramert << "=" << value;
	_line_string.push_back(s_stream.str());
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
