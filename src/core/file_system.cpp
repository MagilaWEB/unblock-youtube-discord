#include "file_system.h"

static const std::regex r_section_name{ "\\[.*\\](?:.*|\\n)" };
static const std::regex reg_equally("\\=");

File::~File()
{
	close();
}

void File::forLine(std::function<bool(std::string)> fn)
{
	CRITICAL_SECTION_RAII(lock);
	if (!isOpen())
	{
		if (info_debug)
			Debug::warning("File [{}] not open!", _path_file.string().c_str());
		return;
	}

	if (_line_string.empty())
		return;

	for (auto& str : _line_string)
		if (fn(str))
			break;
}

void File::forLineSection(std::string_view section, std::function<bool(std::string&)> fn)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
	{
		if (info_debug)
			Debug::warning("File [{}] not open!", _path_file.string().c_str());
		return;
	}

	auto& list_string = _map_list_string[section.data()];
	if (list_string.empty())
	{
		bool		start{ false };
		std::string name_section{ std::format("[{}]", section) };
		forLine(
			[&](std::string str)
			{
				if ((!start) && std::regex_match(str, r_section_name) && str.contains(name_section))
				{
					start = true;
					return false;
				}

				if (!start)
					return false;

				if (std::regex_match(str, r_section_name))
					return true;

				if ((!str.empty()) && (!std::regex_match(str, std::regex{ "\n" })))
					list_string.emplace_back(str);

				return false;
			}
		);

		_normalize();
	}

	for (auto& str : list_string)
		if (fn(str))
			break;
}

void File::forLineParametersSection(std::string_view section, std::function<bool(std::string key, std::string value)> fn)
{
	CRITICAL_SECTION_RAII(lock);

	forLineSection(
		section,
		[this, section, fn](std::string str)
		{
			std::smatch para;
			if (std::regex_search(str, para, reg_equally))
			{
				auto key = para.prefix().str();
				utils::trim(key);
				auto value = para.suffix().str();
				utils::trim(value);
				return fn(key, value);
			}
			else
			{
				Debug::warning(
					"when trying to get the keys and values in the [{}] file in the [{}] section The kneader is missing [=]! A string of the "
					"following format [{}].",
					name().c_str(),
					section,
					str.c_str()
				);
			}

			return false;
		}
	);
}

std::optional<u32> File::positionSection(std::string_view section)
{
	u32			position{ 0 };
	std::string name_section{ std::format("[{}]", section) };

	bool is_section{ false };

	forLine(
		[&](std::string str)
		{
			if (std::regex_match(str, r_section_name))
			{
				position++;
				return is_section = str.contains(name_section);
			}

			return false;
		}
	);

	if (!is_section)
		return std::nullopt;

	return position;
}

template<concepts::VallidALL TypeReturn>
std::expected<TypeReturn, std::string> File::parameterSection(std::string_view section, std::string parameter)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
	{
		if (info_debug)
			Debug::warning("File [{}] not open!", _path_file.string().c_str());
		return Debug::str_unexpected("Не удалось найти параметр [{}] в секции [{}] файл не был открыт!", parameter, section);
	}

	std::optional<std::string> kay_value{ std::nullopt };
	forLineParametersSection(
		section,
		[&kay_value, parameter](std::string key, std::string value)
		{
			if (key == parameter.data())
			{
				kay_value = value;
				return true;
			}

			return false;
		}
	);

	if (kay_value)
	{
		using namespace concepts;
		if constexpr (VallidString<TypeReturn>)
		{
			if constexpr (VallidStringPctr<TypeReturn>)
				return kay_value.value().c_str();
			else
				return kay_value.value();
		}

		if constexpr (std::same_as<TypeReturn, bool>)
		{
			bool state;
			std::istringstream{ kay_value.value() } >> std::boolalpha >> state;
			return state;
		}
		else if constexpr (VallidNumber<TypeReturn>)
		{
			if constexpr (std::same_as<TypeReturn, float>)
				return std::stof(kay_value.value());
			else
				return std::stod(kay_value.value());
		}
		else if constexpr (VallidIntegerUsignet<TypeReturn>)
		{
			TypeReturn state{};
			std::istringstream{ kay_value.value() } >> state;
			return state;
		}
		else if constexpr (VallidInteger<TypeReturn>)
		{
			TypeReturn state{};
			std::istringstream{ kay_value.value() } >> state;
			return state;
		}
	}

	return Debug::str_unexpected("Не удалось найти параметр [{}] в секции [{}]!", parameter, section);
}

template CORE_API std::expected<std::string, std::string> File::parameterSection<std::string>(std::string_view section, std::string parameter);
template CORE_API std::expected<cpcstr, std::string> File::parameterSection<cpcstr>(std::string_view section, std::string parameter);
template CORE_API std::expected<pcstr, std::string> File::parameterSection<pcstr>(std::string_view section, std::string parameter);
template CORE_API std::expected<bool, std::string> File::parameterSection<bool>(std::string_view section, std::string parameter);
template CORE_API std::expected<float, std::string> File::parameterSection<float>(std::string_view section, std::string parameter);
template CORE_API std::expected<s32, std::string> File::parameterSection<s32>(std::string_view section, std::string parameter);
template CORE_API std::expected<u8, std::string> File::parameterSection<u8>(std::string_view section, std::string parameter);
template CORE_API std::expected<u32, std::string> File::parameterSection<u32>(std::string_view section, std::string parameter);
template CORE_API std::expected<u64, std::string> File::parameterSection<u64>(std::string_view section, std::string parameter);

void File::writeText(std::string_view str)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
		_open_state = true;

	_is_write = true;

	_line_string.emplace_back(str);
}

void File::writeSectionParameter(std::string_view section, std::string parameter, std::string value_argument)
{
	CRITICAL_SECTION_RAII(lock);

	if (!isOpen())
		_open_state = true;

	_is_write = true;

	bool stoped{ false };
	forLineSection(
		section,
		[&stoped, parameter, value_argument](std::string& str)
		{
			std::smatch para;
			if (std::regex_search(str, para, reg_equally))
			{
				auto key = para.prefix().str();
				utils::trim(key);
				auto value = para.suffix().str();
				utils::trim(value);

				if (key == parameter)
				{
					str	   = std::regex_replace(str, std::regex{ value }, value_argument.data());
					stoped = true;
					return true;
				}
			}

			return false;
		}
	);

	if (stoped)
		return;

	_map_list_string[section.data()].emplace_back(std::format("{}={}", parameter, value_argument));
	_normalize();
}

std::string File::name() const
{
	return _path_file.filename().string();
}

path File::getPath() const
{
	return _path_file;
}

size_t File::lineSize() const
{
	return _line_string.size();
}

bool File::isOpen() const
{
	return _open_state;
}

bool File::empty() const
{
	return _line_string.empty() && _map_list_string.empty();
}

void File::open(std::filesystem::path file, std::string_view expansion, bool no_default_patch)
{
	CRITICAL_SECTION_RAII(lock);
	_is_write = false;

	if (isOpen())
	{
		close();
		_is_write = false;
	}

	if (!no_default_patch)
		_path_file = (Core::get().currentPath() / file) += expansion;
	else
		_path_file = file += expansion;

	_stream.open(_path_file, std::ios::in);

	if (!_stream.is_open())
	{
		if (info_debug)
			Debug::warning("File open fail [{}]!", _path_file.string().c_str());
		_open_state = false;
		_stream.close();
		return;
	}

	_line_string.clear();
	_map_list_string.clear();

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
	_map_list_string.clear();
}

void File::save()
{
	if (!_is_write)
		return;

	_stream.open(_path_file, std::ios::out | std::ios::binary);

	_normalize();

	_removeEmptyLine();

	for (auto& _string : _line_string)
		_stream << _string << std::endl;
	_stream.close();

	_is_write = false;
}

void File::close()
{
	CRITICAL_SECTION_RAII(lock);
	_open_state = false;
	save();
	clear();
}

void File::_normalize()
{
	for (auto& [section, list_string] : _map_list_string)
	{
		std::string key{ std::format("[{}]", section) };

		bool   is_section{ false };
		size_t it = 0;
		for (; it < _line_string.size(); it++)
		{
			auto& str = _line_string[it];
			if ((!is_section) && std::regex_match(str, r_section_name) && str.contains(key))
				is_section = true;
			else if (is_section && std::regex_match(str, r_section_name))
				break;

			if (is_section)
			{
				_line_string.erase(_line_string.begin() + it);
				it--;
			}
		}

		_line_string.insert(_line_string.begin() + it, key);

		for (auto& str : list_string)
			_line_string.insert(_line_string.begin() + ++it, str);

		if (++it < _line_string.size())
			_line_string.insert(_line_string.begin() + it, "\n");
	}
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
