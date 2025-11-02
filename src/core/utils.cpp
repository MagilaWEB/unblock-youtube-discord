#include "utils.h"

bool utils::IsUTF8(pcstr string)
{
	if (!string)
		return true;

	const unsigned char* bytes = reinterpret_cast<const unsigned char*>(string);
	u32					 num;
	while (*bytes != 0x00)
	{
		if ((*bytes & 0x80) == 0x00)
		{
			// U+0000 to U+007F
			num = 1;
		}
		else if ((*bytes & 0xE0) == 0xC0)
		{
			// U+0080 to U+07FF
			num = 2;
		}
		else if ((*bytes & 0xF0) == 0xE0)
		{
			// U+0800 to U+FFFF
			num = 3;
		}
		else if ((*bytes & 0xF8) == 0xF0)
		{
			// U+10000 to U+10FFFF
			num = 4;
		}
		else
			return false;

		bytes += 1;
		for (u32 i = 1; i < num; ++i)
		{
			if ((*bytes & 0xC0) != 0x80)
				return false;
			bytes += 1;
		}
	}

	return true;
}

std::string utils::UTF8_to_CP1251(pcstr str)
{
	if (IsUTF8(str))
	{
		const int len = static_cast<int>(strlen(str));

		static thread_local wchar_t cache_str[4'096];
		RtlZeroMemory(&cache_str, sizeof(cache_str));

		MultiByteToWideChar(CP_UTF8, 0, str, len + 1, cache_str, len + 1);

		static thread_local char cache_str_result[4'096];
		RtlZeroMemory(&cache_str_result, sizeof(cache_str_result));

		WideCharToMultiByte(1'251, 0, &cache_str[0], len, &cache_str_result[0], len, 0, 0);

		return { cache_str_result };
	}

	return { str };
}

void utils::ltrim(std::string& str)
{
	auto iterator = std::find_if(str.begin(), str.end(), [](char ch) { return !std::isspace<char>(ch, std::locale::classic()); });
	str.erase(str.begin(), iterator);
}

void utils::rtrim(std::string& str)
{
	auto iterator = std::find_if(str.rbegin(), str.rend(), [](char ch) { return !std::isspace<char>(ch, std::locale::classic()); });
	str.erase(iterator.base(), str.end());
}

void utils::trim(std::string& str)
{
	rtrim(str);
	ltrim(str);
}