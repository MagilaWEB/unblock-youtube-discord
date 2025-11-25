#include "file_system.h"

File::~File()
{
	close();
}

void File::forLine(std::function<bool(std::string)> fn)
{
	CRITICAL_SECTION_RAII(lock);
	if (!isOpen())
	{
		Debug::warning("File [%s] not open!", _path_file.string().c_str());
		return;
	}

	if (_line_string.empty())
		return;

	for (auto& str : _line_string)
		if (fn(str))
			break;
}

void File::_forLineSection(pcstr section, std::function<bool(ItParameters&)> fn)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
	{
		Debug::warning("File [%s] not open!", _path_file.string().c_str());
		return;
	}

	const std::regex r_section_name{ "\\[.*\\](?:.*|\\n)" };
	ItParameters	 option_it{};

	for (option_it.iterator = _line_string.begin(); option_it.iterator < _line_string.end();)
	{
		const auto& str = *option_it.iterator;

		if (str.empty() || std::regex_match(str, std::regex{ "\n" }))
		{
			++option_it.iterator;
			++option_it.i;
			continue;
		}

		auto iterator = [&]
		{
			++option_it.i;
			if (++option_it.iterator == _line_string.end())
			{
				option_it.iterator--;
				option_it.section_end = true;
				fn(option_it);
				return true;
			}

			return false;
		};


		std::string name_section{ "[]" };
		name_section.insert(1, section);

		const bool presumably_section = std::regex_match(str, r_section_name);
		const bool is_str_name		  = str.contains(name_section);

		if (option_it.entered_section == false && presumably_section && is_str_name)
		{
			option_it.entered_section = true;
			if (iterator())
				break;
			continue;
		}

		if (option_it.entered_section && !presumably_section)
		{
			option_it.ran_parameter = true;

			if (fn(option_it))
				break;

			if (iterator())
				break;

			continue;
		}

		if (option_it.entered_section && presumably_section && !is_str_name)
		{
			option_it.ran_parameter = false;
			option_it.section_end	= true;

			if (fn(option_it))
			{
				option_it.entered_section = false;
				break;
			}

			option_it.entered_section = false;

			continue;
		}

		if (fn(option_it))
		{
			option_it.ran_parameter = false;
			option_it.section_end	= false;
			break;
		}

		option_it.ran_parameter = false;
		option_it.section_end	= false;

		if (iterator())
			break;
	}
}

void File::forLineSection(pcstr section, std::function<bool(std::string str)> fn)
{
	CRITICAL_SECTION_RAII(lock);

	_forLineSection(
		section,
		[fn](const ItParameters& it)
		{
			if (it.section_end)
				return true;

			if(it.ran_parameter)
				if (fn(*it.iterator))
					return true;

			return false;
		}
	);
}

void File::forLineParametersSection(pcstr section, std::function<bool(std::string key, std::string value)> fn)
{
	CRITICAL_SECTION_RAII(lock);

	forLineSection(
		section,
		[this, section, fn](std::string str)
		{
			size_t pos = str.find_first_of("=");
			if (pos != std::string::npos)
			{
				auto key = str.substr(0, pos);
				utils::trim(key);
				auto value = str.substr(++pos, str.size());
				utils::trim(value);
				return fn(key, value);
			}
			else
			{
				Debug::warning(
					"при попытке получить ключи и значения в файле [%s] в секции [%s] отсутствует "
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

template<typename TypeReturn>
std::expected<TypeReturn, std::string> File::parameterSection(pcstr section, pcstr parameter)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
	{
		Debug::warning("File [%s] not open!", _path_file.string().c_str());
		return Debug::str_unexpected("Не удалось найти параметр [%s] в секции [%s] файл не был открыт!", parameter, section);
	}

	std::optional<std::string> kay_value{ std::nullopt };
	forLineParametersSection(
		section,
		[&kay_value, parameter](std::string key, std::string value)
		{
			if (key.contains(parameter))
			{
				kay_value = value;
				return true;
			}

			return false;
		}
	);

	if (kay_value)
	{
		if constexpr (std::is_same_v<TypeReturn, std::string> || std::is_same_v<TypeReturn, pcstr>)
		{
			if constexpr (std::is_same_v<TypeReturn, pcstr>)
				return kay_value.value().c_str();
			else
				return kay_value.value();
		}

		if constexpr (std::is_same_v<TypeReturn, bool>)
		{
			bool state;
			std::istringstream{ kay_value.value() } >> std::boolalpha >> state;
			return state;
		}
		else if constexpr (std::is_same_v<TypeReturn, float>)
			return std::stof(kay_value.value());
		else if constexpr (std::is_same_v<TypeReturn, u32> || std::is_same_v<TypeReturn, long long>)
		{
			u32 state;
			std::istringstream{ kay_value.value() } >> state;
			return state;
		}
		else if constexpr (std::is_same_v<TypeReturn, s32> || std::is_same_v<TypeReturn, int>)
		{
			int state;
			std::istringstream{ kay_value.value() } >> state;
			return state;
		}

		//			static_assert(false, "Only std data types are supported string || const char* || pcstr, u32 || long long, s32 || int, bool!");
	}

	return Debug::str_unexpected("Не удалось найти параметр [%s] в секции [%s]!", parameter, section);
}

template CORE_API std::expected<std::string, std::string> File::parameterSection<std::string>(pcstr section, pcstr parameter);
template CORE_API std::expected<pcstr, std::string> File::parameterSection<pcstr>(pcstr section, pcstr parameter);
template CORE_API std::expected<bool, std::string> File::parameterSection<bool>(pcstr section, pcstr parameter);
template CORE_API std::expected<float, std::string> File::parameterSection<float>(pcstr section, pcstr parameter);
template CORE_API std::expected<int, std::string> File::parameterSection<int>(pcstr section, pcstr parameter);
template CORE_API std::expected<u32, std::string> File::parameterSection<u32>(pcstr section, pcstr parameter);
template CORE_API std::expected<long long, std::string> File::parameterSection<long long>(pcstr section, pcstr parameter);

void File::writeText(std::string str)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
		_open_state = true;

	_is_write = true;

	_line_string.emplace_back(str);
}

void File::writeSectionParameter(pcstr section, pcstr parameter, pcstr value_argument)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
		_open_state = true;

	_is_write = true;

	bool stoped{ false };

	_forLineSection(
		section,
		[&](ItParameters& it)
		{
			if (it.ran_parameter && !it.section_end)
			{
				auto&  str = *it.iterator;
				size_t pos = str.find_first_of("=");
				if (pos != std::string::npos)
				{
					auto key = str.substr(0, pos);
					utils::trim(key);
					auto value = str.substr(++pos, str.size());
					utils::trim(value);

					if (key.contains(parameter))
					{
						str	   = std::regex_replace(str, std::regex{ value }, value_argument);
						stoped = true;
						return true;
					}
				}
			}
			else if (it.section_end)
			{
				if (!it.entered_section)
				{
					if (!_line_string.empty())
						_line_string.emplace_back("\n");

					std::string str{ "[]" };
					_line_string.emplace_back(str.insert(1, section));
				}

				std::string str{ parameter };
				str += "=";
				str += value_argument;
				if (!it.entered_section)
					_line_string.insert(_line_string.end(), str);
				else
					_line_string.insert(_line_string.begin() + (it.i - 1), str);

				stoped = true;
				return true;
			}

			return false;
		}
	);

	if (stoped)
		return;

	if (!_line_string.empty())
		_line_string.emplace_back("\n");

	std::string str{ "[]" };
	_line_string.emplace_back(str.insert(1, section));
	str.clear();

	str	 = parameter;
	str += "=";
	str += value_argument;
	_line_string.emplace_back(str);
}

std::string File::name() const
{
	return _path_file.filename().string();
}

size_t File::lineSize() const
{
	return _line_string.size();
}

bool File::isOpen() const
{
	return _open_state;
}

void File::open(std::filesystem::path file, pcstr expansion, bool no_default_patch)
{
	CRITICAL_SECTION_RAII(lock);

	if (isOpen())
		close();

	if (!no_default_patch)
		_path_file = (Core::get().currentPath() / file) += expansion;
	else
		_path_file = file += expansion;

	_stream.open(_path_file, std::ios::in);

	if (!_stream.is_open())
	{
		Debug::warning("File open fail [%s]!", _path_file.string().c_str());
		_open_state = false;
		_stream.close();
		return;
	}

	_line_string.clear();

	std::string str;
	while (getline(_stream, str))
		_line_string.emplace_back(str);

	_removeEmptyLine();

	_stream.close();

	_open_state = true;
}

void File::clear()
{
	CRITICAL_SECTION_RAII(lock);

	_is_write = true;

	_line_string.clear();
}

void File::close()
{
	CRITICAL_SECTION_RAII(lock);

	_writeToFile();
	_open_state = false;
}

void File::_removeEmptyLine()
{
	bool front{ true };
	u32	 empty_line{ 0 };
	std::erase_if(
		_line_string,
		[&front, &empty_line](const std::string& str)
		{
			if (front && (str.empty() || std::regex_match(str, std::regex{ "\n" })))
				return true;
			else if (front)
				front = false;

			if (!front && str.empty())
				empty_line++;
			else
				empty_line = 0;

			if (empty_line > 1)
				return true;

			return false;
		}
	);

	for (auto& str : _line_string)
		utils::trim(str);
}

void File::_writeToFile()
{
	if (!_is_write)
		return;

	_stream.open(_path_file, std::ios::out | std::ios::binary);

	_removeEmptyLine();

	for (auto& _string : _line_string)
		_stream << _string << std::endl;
	_stream.close();

	_is_write = false;
}
