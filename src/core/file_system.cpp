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

void FileSystem::_forLineSection(pcstr section, std::function<bool(ItOptionsParamerts&)>&& fn)
{
	const static std::regex r_section_name{ "\\[.*\\](?:.*|\\n)" };
	ItOptionsParamerts		option_it{};

	for (option_it.iterator = _line_string.begin(); option_it.iterator < _line_string.end();)
	{
		const auto& str = *option_it.iterator;

		if (str.empty())
		{
			option_it.ran_paramert = false;
			option_it.section_end  = false;
			++option_it.iterator;
			continue;
		}

		auto iterator = [&]
		{
			if (++option_it.iterator == _line_string.end())
			{
				option_it.entered_section = false;
				option_it.ran_paramert	  = false;
				option_it.section_end	  = true;
				fn(option_it);
				return true;
			}

			return false;
		};

		const bool presunably_section = std::regex_match(str, r_section_name);

		if (option_it.entered_section == false && presunably_section && str.contains(section))
			option_it.entered_section = true;
		else if (option_it.entered_section && !presunably_section)
		{
			option_it.ran_paramert = true;
			option_it.section_end  = false;

			if (fn(option_it))
				break;

			if (iterator())
				break;

			continue;
		}
		else if (option_it.entered_section && presunably_section)
		{
			option_it.entered_section = false;
			option_it.ran_paramert	  = false;
			option_it.section_end	  = true;

			if (fn(option_it))
				break;

			if (iterator())
				break;

			continue;
		}

		option_it.ran_paramert = false;
		option_it.section_end  = false;

		if (fn(option_it))
			break;

		iterator();
	}
}

void FileSystem::forLineSection(pcstr section, std::function<bool(std::string str)>&& fn)
{
	_forLineSection(
		section,
		[&](const ItOptionsParamerts& it)
		{
			if (it.ran_paramert)
				if (fn(*it.iterator))
					return true;

			return it.section_end;
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
					"при попытке получить ключи и значения в файле[%s] в секции [%s] отсутствует "
					"разминователь [=]! Строка следующего вида [%s].",
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

void FileSystem::writeSectionParametr(pcstr section, pcstr paramert, pcstr value_argument)
{
	u32	 save_iterator{ 0 };
	auto insert = [this, paramert, value_argument](auto&& it)
	{
		std::string str{ paramert };
		str += "=";
		str += value_argument;
		_line_string.insert(it, str);
	};

	bool stoped{ false };

	_forLineSection(
		section,
		[&](ItOptionsParamerts& it)
		{
			if (it.ran_paramert)
			{
				auto&  str = *it.iterator;
				size_t pos = str.find_first_of("=");
				if (pos != std::string::npos)
				{
					const auto& key	  = str.substr(0, pos);
					const auto& value = str.substr(++pos, str.size());
					if (key.contains(paramert))
					{
						str	   = std::regex_replace(str, std::regex{ value }, value_argument);
						stoped = true;
						_writeToFile();
						return true;
					}
				}
			}
			else if (it.section_end)
			{
				insert(it.iterator);
				stoped = true;
				_writeToFile();
				return true;
			}

			return false;
		}
	);

	if (stoped)
		return;

	if (!_line_string.empty())
		_line_string.push_back("\\n");

	std::string str{ "[]" };
	_line_string.push_back(str.insert(1, section));
	str.clear();

	str	 = paramert;
	str += "=";
	str += value_argument;
	_line_string.push_back(str);
	_writeToFile();
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
		Debug::error("File open fail [%s] error [%s]!", _path_file.string().c_str(), error.what());
		_open_state = false;
		return;
	}

	_open_state = true;
}

void FileSystem::close()
{
	_writeToFile();

	_open_state = false;
}

void FileSystem::_writeToFile()
{
	_stream.open(_path_file, std::ios::out | std::ios::binary);
	for (auto& _string : _line_string)
		_stream << _string << std::endl;
	_stream.close();
}
