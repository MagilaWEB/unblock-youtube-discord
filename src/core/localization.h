#pragma once

#include "file_system.h"

class Localization final
{
	FastLock						   _lock;
	Ptr<File>						   _lang_file_string{ false };
	std::map<std::string, std::string> _string_list;

public:
	struct Str
	{
		Str() = delete;
		Str(pcstr str_id) : _str_id(str_id) {}
		Str(std::string str_id) : _str_id(str_id) {}
		Str(std::string_view str_id) : _str_id(std::string{ str_id }) {}

		std::string operator()()
		{
			if (!_str_id.empty())
				return Localization::get().translate(_str_id);

			return "warning: id text nullptr!";
		}

		std::string _str_id;
	};

public:
	Localization() = default;
	~Localization();

	static Localization& get();

	void set(std::string_view lang_id);

	std::string translate(std::string_view str_id);
};
