#pragma once

// Header-only proxy config: plain key=value lines, written by the engine.
//   listen=127.0.0.1
//   port=53
//   upstream=https://cloudflare-dns.com/dns-query|1.1.1.1,1.0.0.1
//   upstream=https://dns.google/dns-query|8.8.8.8,8.8.4.4
// The bootstrap part after '|' may be empty. Lines starting with '#'
// and blank lines are ignored. Pure logic, covered by unit tests.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct DnsUpstreamConfig
{
	std::string				 address;
	std::vector<std::string> bootstrap;
};

struct DnsProxyConfig
{
	std::string listen{ "127.0.0.1" };
	uint16_t	port{ 53 };

	std::string log_path;
	std::string status_path;
	std::string backup_path;

	std::vector<DnsUpstreamConfig> upstreams;
};

inline bool isIpv4Address(std::string_view s)
{
	size_t dots = 0;
	size_t part = 0;
	size_t digits = 0;
	for (char ch : s)
	{
		if (ch >= '0' && ch <= '9')
		{
			++digits;
			part = part * 10 + static_cast<size_t>(ch - '0');
			if (part > 255 || digits > 3)
				return false;
			continue;
		}
		if (ch == '.')
		{
			if (digits == 0)
				return false;
			++dots;
			part   = 0;
			digits = 0;
			continue;
		}
		return false;
	}
	return dots == 3 && digits > 0;
}

inline bool isHostname(std::string_view s)
{
	if (s.empty() || s.size() > 253)
		return false;

	size_t label_len = 0;
	bool   has_dot	 = false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		const char ch = s[i];
		if (ch == '.')
		{
			if (label_len == 0)
				return false;
			label_len = 0;
			has_dot	  = true;
			continue;
		}
		if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-'))
			return false;
		if (ch == '-' && label_len == 0)
			return false;
		++label_len;
		if (label_len > 63)
			return false;
	}
	return has_dot && label_len > 0;
}

// Strict enough to catch typos, loose enough to leave protocol details
// to the DLL (it validates at init and reports precisely). When the plain
// address is a hostname (not IPv4), a bootstrap list is mandatory — the
// DLL refuses to init such an upstream otherwise (AE_EMPTY_BOOTSTRAP).
inline bool isValidUpstreamAddress(const std::string& address, bool has_bootstrap = true)
{
	if (address.empty() || address.find(' ') != std::string::npos)
		return false;

	for (const char* scheme : { "tcp://", "tls://", "https://", "quic://", "h3://", "sdns://" })
	{
		const std::string prefix{ scheme };
		if (address.starts_with(prefix))
			return address.size() > prefix.size();
	}

	// Plain "host[:port]".
	std::string_view host{ address };
	const size_t	 colon = address.rfind(':');
	if (colon != std::string::npos)
	{
		const std::string_view port{ address.data() + colon + 1, address.size() - colon - 1 };
		if (port.empty() || !std::ranges::all_of(port, [](unsigned char c) { return c >= '0' && c <= '9'; }))
			return false;
		host = std::string_view{ address.data(), colon };
	}

	if (isIpv4Address(host))
		return true;

	// A digits-and-dots string that failed IPv4 validation is a malformed
	// address (e.g. "1.2.3", "999.1.1.1"), not a hostname.
	if (std::ranges::all_of(host, [](unsigned char c) { return (c >= '0' && c <= '9') || c == '.'; }))
		return false;

	return isHostname(host) && has_bootstrap;
}

inline std::string trimConfigLine(std::string_view line)
{
	const size_t begin = line.find_first_not_of(" \t\r\n");
	if (begin == std::string_view::npos)
		return {};

	const size_t end = line.find_last_not_of(" \t\r\n");
	return std::string{ line.substr(begin, end - begin + 1) };
}

// Parses one "upstream=<addr>[|<boot,csv>]" value. Returns nullopt when the
// address part is invalid; a missing bootstrap list is fine (empty).
inline std::optional<DnsUpstreamConfig> parseUpstreamValue(const std::string& value)
{
	const size_t	  sep  = value.find('|');
	const std::string addr = trimConfigLine(value.substr(0, sep));

	DnsUpstreamConfig out{ addr, {} };
	if (sep != std::string::npos)
	{
		std::string_view rest{ value.data() + sep + 1, value.size() - sep - 1 };
		while (!rest.empty())
		{
			const size_t comma = rest.find(',');
			std::string	 part = trimConfigLine(rest.substr(0, comma));
			if (!part.empty())
				out.bootstrap.push_back(std::move(part));

			if (comma == std::string_view::npos)
				break;
			rest.remove_prefix(comma + 1);
		}
	}

	if (!isValidUpstreamAddress(addr, !out.bootstrap.empty()))
		return std::nullopt;

	return out;
}

// Parses whole config content. Returns {config, error}; error is empty on
// success. A config without upstreams is an error (nothing to serve).
inline std::pair<DnsProxyConfig, std::string> parseProxyConfig(const std::string& content)
{
	DnsProxyConfig config;
	size_t		   pos = 0;

	while (pos <= content.size())
	{
		size_t end = content.find('\n', pos);
		if (end == std::string::npos)
			end = content.size();

		const std::string line = trimConfigLine(std::string_view{ content.data() + pos, end - pos });
		pos					 = end + 1;

		if (line.empty() || line.starts_with('#'))
			continue;

		const size_t eq = line.find('=');
		if (eq == std::string::npos)
			return { config, "Bad line (no '='): " + line };

		const std::string key	= trimConfigLine(line.substr(0, eq));
		const std::string value = trimConfigLine(line.substr(eq + 1));

		if (key == "listen")
			config.listen = value;
		else if (key == "port")
		{
			try
			{
				const int port = std::stoi(value);
				if (port <= 0 || port > 65535)
					return { config, "Bad port: " + value };
				config.port = static_cast<uint16_t>(port);
			}
			catch (...)
			{
				return { config, "Bad port: " + value };
			}
		}
		else if (key == "upstream")
		{
			auto upstream = parseUpstreamValue(value);
			if (!upstream)
				return { config, "Bad upstream: " + value };
			config.upstreams.push_back(std::move(*upstream));
		}
		else if (key == "log")
			config.log_path = value;
		else if (key == "status")
			config.status_path = value;
		else if (key == "backup")
			config.backup_path = value;
		else
			return { config, "Unknown key: " + key };
	}

	if (config.upstreams.empty())
		return { config, "No upstreams configured" };

	return { config, {} };
}
