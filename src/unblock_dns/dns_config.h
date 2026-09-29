#pragma once

// Header-only proxy config: plain key=value lines, written by the engine.
//   listen=127.0.0.1
//   port=53
//   timeout=15000
//   bootstrap=1.1.1.1,8.8.8.8,9.9.9.9
//   upstream=https://cloudflare-dns.com/dns-query
//   upstream=1.1.1.1
// The bootstrap list is shared by every upstream that has to resolve a
// hostname (DoH/DoT/QUIC and plain hostnames); plain-IP upstreams ignore it.
// A hostname upstream with an empty bootstrap is rejected here, because the
// DLL refuses to init it (AE_EMPTY_BOOTSTRAP) at runtime. Lines starting with
// '#' and blank lines are ignored. Pure logic, covered by unit tests.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct DnsProxyConfig
{
	std::string listen{ "127.0.0.1" };
	uint16_t	port{ 53 };
	// Upstream exchange timeout. Private resolvers (GeoHide) are slow from
	// time to time, so the default is deliberately generous and editable.
	uint32_t	timeout_ms{ 15'000 };

	std::string log_path;
	std::string backup_path;

	// Plain DNS servers used to resolve hostnames of the upstreams below.
	std::vector<std::string> bootstrap;
	// Upstream addresses only (no per-upstream bootstrap).
	std::vector<std::string> upstreams;
};

inline bool isIpv4Address(std::string_view s)
{
	size_t dots	  = 0;
	size_t part	  = 0;
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
// to the DLL (it validates at init and reports precisely). Hostname
// upstreams are allowed here regardless of bootstrap: the whole config is
// checked for a usable bootstrap separately in parseProxyConfig().
inline bool isValidUpstreamAddress(const std::string& address)
{
	if (address.empty() || address.contains(' '))
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

	return isHostname(host);
}

// True when the upstream has to resolve a hostname through the bootstrap
// list (DoH/DoT/QUIC/crypto, or a plain hostname). DNS stamps carry their
// own address and plain IPv4 entries resolve nothing.
inline bool upstreamNeedsBootstrap(const std::string& address)
{
	if (address.starts_with("sdns://"))
		return false;

	std::string_view host{ address };
	const size_t	 scheme = host.find("://");
	if (scheme != std::string_view::npos)
		host.remove_prefix(scheme + 3);

	const size_t slash = host.find('/');
	if (slash != std::string_view::npos)
		host = host.substr(0, slash);

	const size_t at = host.rfind('@');
	if (at != std::string_view::npos)
		host.remove_prefix(at + 1);

	const size_t colon = host.rfind(':');
	if (colon != std::string_view::npos)
		host = host.substr(0, colon);

	return !isIpv4Address(host);
}

// A bootstrap entry is a plain DNS server address: IPv4 with an optional port.
inline bool isValidBootstrapAddress(std::string_view value)
{
	if (value.empty())
		return false;

	const size_t colon = value.rfind(':');
	if (colon != std::string_view::npos)
	{
		const std::string_view port{ value.data() + colon + 1, value.size() - colon - 1 };
		if (port.empty() || !std::ranges::all_of(port, [](unsigned char c) { return c >= '0' && c <= '9'; }))
			return false;
		value = value.substr(0, colon);
	}
	return isIpv4Address(value);
}

inline std::string trimConfigLine(std::string_view line)
{
	const size_t begin = line.find_first_not_of(" \t\r\n");
	if (begin == std::string_view::npos)
		return {};

	const size_t end = line.find_last_not_of(" \t\r\n");
	return std::string{ line.substr(begin, end - begin + 1) };
}

// Parses a comma-separated bootstrap list. Returns nullopt when any entry is
// not a plain IPv4[:port] address. Empty entries are skipped, duplicates
// dropped. An empty list is valid (returns an empty vector).
inline std::optional<std::vector<std::string>> parseBootstrapList(std::string_view value)
{
	std::vector<std::string> out;

	size_t pos = 0;
	while (pos <= value.size())
	{
		size_t end = value.find(',', pos);
		if (end == std::string_view::npos)
			end = value.size();

		std::string part = trimConfigLine(value.substr(pos, end - pos));
		pos				 = end + 1;

		if (part.empty())
			continue;
		if (!isValidBootstrapAddress(part))
			return std::nullopt;
		if (std::ranges::find(out, part) == out.end())
			out.push_back(std::move(part));
	}

	return out;
}

// Parses whole config content. Returns {config, error}; error is empty on
// success. A config without upstreams, or with hostname upstreams but no
// bootstrap, is an error (the DLL could not init them).
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
		pos					   = end + 1;

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
				if (port <= 0 || port > 65'535)
					return { config, "Bad port: " + value };
				config.port = static_cast<uint16_t>(port);
			}
			catch (...)
			{
				return { config, "Bad port: " + value };
			}
		}
		else if (key == "timeout")
		{
			try
			{
				const int timeout = std::stoi(value);
				if (timeout < 1'000 || timeout > 120'000)
					return { config, "Bad timeout: " + value };
				config.timeout_ms = static_cast<uint32_t>(timeout);
			}
			catch (...)
			{
				return { config, "Bad timeout: " + value };
			}
		}
		else if (key == "bootstrap")
		{
			auto list = parseBootstrapList(value);
			if (!list)
				return { config, "Bad bootstrap: " + value };
			config.bootstrap = std::move(*list);
		}
		else if (key == "upstream")
		{
			if (!isValidUpstreamAddress(value))
				return { config, "Bad upstream: " + value };
			config.upstreams.push_back(value);
		}
		else if (key == "log")
			config.log_path = value;
		else if (key == "backup")
			config.backup_path = value;
		else
			return { config, "Unknown key: " + key };
	}

	if (config.upstreams.empty())
		return { config, "No upstreams configured" };

	if (config.bootstrap.empty())
		for (const auto& upstream : config.upstreams)
			if (upstreamNeedsBootstrap(upstream))
				return { config, "Bootstrap required for hostname upstream: " + upstream };

	return { config, {} };
}
