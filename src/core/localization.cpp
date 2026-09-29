#include "localization.h"

Localization::~Localization()
{
	_string_list.clear();
}

Localization& Localization::get()
{
	static Localization lang;
	return lang;
}

void Localization::set(std::string_view lang_id)
{
	FAST_LOCK(_lock);
	_string_list.clear();

	_lang_file_string->open(std::filesystem::path{ "ui" } / "text" / lang_id, ".list");

	if (_lang_file_string->empty())
		_lang_file_string->open(std::filesystem::path{ "ui" } / "text" / "US", ".list");

	std::string key{};
	_lang_file_string->forLine(
		[&](std::string str)
		{
			if (str.empty() || str.starts_with("//"))
				return false;

			const size_t pos = str.find_first_of('=');
			if (pos != std::string::npos)
			{
				key = str.substr(0, pos);
				utils::trim(key);
				_string_list.emplace(key, std::string{ std::string_view{ str }.substr(pos + 1) });
			}
			else
			{
				auto& text	= _string_list[key];
				text	   += "\n" + str;
			}

			return false;
		}
	);
}

std::string Localization::translate(std::string_view str_id)
{
	FAST_LOCK_SHARED(_lock);

	auto it = _string_list.find(str_id);
	if (it != _string_list.end())
		return it->second;

	return std::string{ str_id };
}
