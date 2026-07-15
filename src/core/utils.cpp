#include "utils.h"

bool utils::IsUTF8(std::string_view string)
{
	if (string.empty())
		return true;

	const auto* bytes = reinterpret_cast<const unsigned char*>(string.data());
	u32					 num;
	for (size_t idx = 0; idx < string.size();)
	{
		if ((bytes[idx] & 0x80) == 0x00)
			num = 1;
		else if ((bytes[idx] & 0xE0) == 0xC0)
			num = 2;
		else if ((bytes[idx] & 0xF0) == 0xE0)
			num = 3;
		else if ((bytes[idx] & 0xF8) == 0xF0)
			num = 4;
		else
			return false;

		for (u32 i = 1; i < num; ++i)
		{
			if (idx + i >= string.size() || (bytes[idx + i] & 0xC0) != 0x80)
				return false;
		}
		idx += num;
	}

	return true;
}

std::string utils::UTF8_to_CP1251(std::string_view utf8_str)
{
	if (IsUTF8(utf8_str))
	{
		const int len = static_cast<int>(utf8_str.length());

		static thread_local wchar_t cache_str[4'096];
		RtlZeroMemory(&cache_str, sizeof(cache_str));

		MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), len + 1, cache_str, len + 1);

		static thread_local char cache_str_result[4'096];
		RtlZeroMemory(&cache_str_result, sizeof(cache_str_result));

		WideCharToMultiByte(1'251, 0, &cache_str[0], len, &cache_str_result[0], len, nullptr, nullptr);

		return { cache_str_result };
	}

	return std::string{utf8_str};
}

std::wstring utils::UTF8_to_UTF16(std::string_view utf8_str)
{
	if (utf8_str.empty())
		return std::wstring();

	int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), nullptr, 0);

	if (size_needed <= 0)
	{
		Debug::warning("UTF8_to_UTF16 Couldn't convert");
		return std::wstring();
	}

	std::vector<wchar_t> buffer(static_cast<size_t>(size_needed) + 1);
	int					 result = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), buffer.data(), size_needed);

	if (size_needed <= 0)
	{
		Debug::warning("UTF8_to_UTF16 Couldn't convert");
		return std::wstring();
	}

	return std::wstring(buffer.data(), static_cast<size_t>(result));
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
