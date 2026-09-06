#include "utils.h"

bool utils::IsUTF8(std::string_view string)
{
	if (string.empty())
		return true;

	const auto* bytes = reinterpret_cast<const unsigned char*>(string.data());
	u32			num;
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
			if (idx + i >= string.size() || (bytes[idx + i] & 0xC0) != 0x80)
				return false;
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

		// NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage) — explicit size (len + 1) is passed to WinAPI.
		MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), len + 1, cache_str, len + 1);

		static thread_local char cache_str_result[4'096];
		RtlZeroMemory(&cache_str_result, sizeof(cache_str_result));

		WideCharToMultiByte(1'251, 0, &cache_str[0], len, &cache_str_result[0], len, nullptr, nullptr);

		return { cache_str_result };
	}

	return std::string{ utf8_str };
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

namespace
{
	// Извлечение порта и хоста из строки вида [host]:port или host:port
	std::optional<std::pair<std::string_view, std::string_view>> splitHostPort(std::string_view input)
	{
		if (input.empty())
			return std::nullopt;

		if (input.front() == '[')
		{
			size_t close = input.find(']');
			if (close == std::string_view::npos)
				return std::nullopt;

			std::string_view host = input.substr(1, close - 1);
			std::string_view port;
			if (close + 1 < input.size())
			{
				if (input[close + 1] != ':')
					return std::nullopt;

				port = input.substr(close + 2);
			}

			return std::make_pair(host, port);
		}

		size_t colon = input.rfind(':');
		if (colon == std::string_view::npos)
			return std::make_pair(input, std::string_view{});

		std::string_view port = input.substr(colon + 1);
		for (char c : port)
			if (!std::isdigit(c))
				return std::nullopt;

		if (input.rfind(':', colon - 1) != std::string_view::npos)
			return std::nullopt;

		return std::make_pair(input.substr(0, colon), port);
	}

	bool isValidIpv4(std::string_view address)
	{
		int				 octets = 0;
		std::string_view rest	= address;

		while (true)
		{
			auto pos = rest.find('.');
			auto oct = pos == std::string_view::npos ? rest : rest.substr(0, pos);

			if (oct.empty() || oct.size() > 3)
				return false;

			int value = 0;
			for (char ch : oct)
			{
				if (ch < '0' || ch > '9')
					return false;
				value = value * 10 + (ch - '0');
			}

			if (value > 255)
				return false;

			octets++;
			if (pos == std::string_view::npos)
				break;

			rest = rest.substr(pos + 1);
		}

		return octets == 4;
	}

	bool isValidIpv6(std::string_view address)
	{
		const auto double_colon = address.find("::");
		if (double_colon != address.rfind("::"))
			return false;

		const bool has_double = double_colon != std::string_view::npos;

		auto is_hex_group = [](std::string_view group)
		{
			if (group.empty() || group.size() > 4)
				return false;

			return std::ranges::all_of(
				group,
				[](char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }
			);
		};

		auto count_groups = [&is_hex_group](std::string_view part) -> int
		{
			if (part.empty())
				return 0;

			int	 count = 0;
			auto start = part.begin();
			while (true)
			{
				auto			 it = std::ranges::find(start, part.end(), ':');
				std::string_view group{ start, it };

				if (!is_hex_group(group))
					return -1;

				count++;
				if (it == part.end())
					break;

				start = it + 1;
			}

			return count;
		};

		if (has_double)
		{
			const int left	= count_groups(address.substr(0, double_colon));
			const int right = count_groups(address.substr(double_colon + 2));
			if (left < 0 || right < 0)
				return false;

			return left + right <= 7;
		}

		return count_groups(address) == 8;
	}
}	 // namespace

bool utils::isValidHostName(std::string_view str)
{
	if (str.empty() || str.size() > 253)
		return false;

	size_t start  = 0;
	int	   labels = 0;
	for (size_t i = 0; i <= str.size(); ++i)
	{
		if (i == str.size() || str[i] == '.')
		{
			if (i == start)
				return false;

			size_t len = i - start;
			if (len > 63)
				return false;

			for (size_t j = start; j < i; ++j)
			{
				char c = str[j];
				if (!std::isalnum(c) && c != '-' && c != '_')
					return false;

				if (c == '-' && (j == start || j == i - 1))
					return false;
			}

			++labels;
			start = i + 1;
		}
	}

	return labels > 0;
}

bool utils::isValidHostNamePort(std::string_view host)
{
	if (host.empty())
		return false;

	auto parts = splitHostPort(host);
	if (!parts)
		return false;

	auto & [host_part, port_part] = *parts;

	if (!port_part.empty())
	{
		int port	   = 0;
		auto [ptr, ec] = std::from_chars(port_part.data(), port_part.data() + port_part.size(), port);
		if (ec != std::errc{} || port < 0 || port > 65'535)
			return false;
	}

	if (host_part.empty())
		return false;

	if (host_part.front() == '[' && host_part.back() == ']')
	{
		std::string_view ipv6 = host_part.substr(1, host_part.size() - 2);
		return isValidIpv6(ipv6);
	}
	else if (host_part.find(':') != std::string_view::npos)
		return isValidIpv6(host_part);
	else if (host_part.find('.') != std::string_view::npos && isValidIpv4(host_part))
		return true;

	return isValidHostName(host_part);
}

bool utils::isValidNetwork(std::string_view network)
{
	if (network.empty())
		return false;

	std::string_view address = network;
	std::string_view prefix;

	if (auto pos = network.find('/'); pos != std::string_view::npos)
	{
		address = network.substr(0, pos);
		prefix	= network.substr(pos + 1);

		if (address.empty() || prefix.empty())
			return false;
	}

	const bool is_ipv6 = address.contains(':');

	if (!prefix.empty())
	{
		int value = 0;
		for (char ch : prefix)
		{
			if (ch < '0' || ch > '9')
				return false;
			value = value * 10 + (ch - '0');
			if (value > 128)
				return false;
		}

		if (is_ipv6 ? (value > 128) : (value > 32))
			return false;
	}

	return is_ipv6 ? isValidIpv6(address) : isValidIpv4(address);
}
