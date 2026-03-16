#include "localization.h"

Localization::~Localization()
{
	_string_list.clear();
}

Localization& Localization::get()
{
#if __clang__
	[[clang::no_destroy]]
#endif
	static Localization lang{};
	return lang;
}

void Localization::set(std::string_view lang_id)
{
	FAST_LOCK(_lock);
	_string_list.clear();

	_lang_file_string->open(std::filesystem::path{ "ui" } / "text" / lang_id, ".list");

	std::string key{};
	_lang_file_string->forLine(
		[&](std::string str)
		{
			if (str.empty() || std::regex_match(str, std::regex{ "\n" }))
				return false;

			size_t pos = str.find_last_of("//", 2);
			if (pos != std::string::npos)
				return false;

			pos = str.find_first_of("=");
			if (pos != std::string::npos)
			{
				key				  = str.substr(0, pos);
				utils::trim(key);
				const auto& value = str.substr(++pos, str.size());
				_string_list.emplace(key, value);
			}
			else
			{
				auto& text	= _string_list[key];
				text	   += "\n";
				text	   += str;
			}

			return false;
		}
	);
}

pcstr Localization::translate(std::string_view str_id)
{
	FAST_LOCK_SHARED(_lock);

	auto it = _string_list.find(str_id.data());
	if (it != _string_list.end())
		return it->second.c_str();

	return str_id.data();
}
