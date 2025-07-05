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
	{
		Debug::warning("Файл [%s] пустой!", _path_file.string().c_str());
		return;
	}

	for (auto& str : _line_string)
		if (fn(str))
			break;
}

void FileSystem::forLineSection(pcstr section, std::function<bool(std::string str)>&& fn)
{
	bool	   is_section{ false };
	std::regex r{ "\\[.*\\]" };

	forLine(
		[=, &is_section](std::string str)
		{
			if (str.empty())
				return false;

			const bool r_regex = std::regex_match(str, r);
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
				const auto key	 = str.substr(0, pos);
				const auto value = str.substr(++pos, str.size());
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

std::expected<std::pair<std::string, std::string>, std::string> FileSystem::parametrSection(pcstr section, pcstr paramert)
{
	std::optional<std::pair<std::string, std::string>> kay_value{ std::nullopt };
	forLineParametrsSection(
		section,
		[&kay_value, paramert](std::string key, std::string value)
		{
			if (key.contains(paramert))
			{
				kay_value = { key, value };
				return true;
			}

			return false;
		}
	);

	if (kay_value)
		return kay_value.value();

	return Debug::str_unexpected("Не удалось найти параметр [%s] в секции [%s]!", paramert, section);
}

std::string FileSystem::name() const
{
	return _path_file.filename().string();
}

bool FileSystem::isOpen() const
{
	return _stream.is_open();
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

		_stream.seekg(0, std::ios::beg);
	}
	catch (const std::ios::failure& error)
	{
		Debug::error("Faile open fail [%s] error [%s]!", _path_file.string().c_str(), error.what());
	}
}

void FileSystem::close()
{
	if (isOpen())
		_stream.close();
}
