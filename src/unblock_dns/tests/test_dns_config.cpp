#include <catch2/catch_test_macros.hpp>

#include "../dns_config.h"

TEST_CASE("isValidUpstreamAddress accepts known forms", "[dns][config]")
{
	CHECK(isValidUpstreamAddress("1.1.1.1"));
	CHECK(isValidUpstreamAddress("8.8.8.8:53"));
	CHECK(isValidUpstreamAddress("tcp://8.8.8.8:53"));
	CHECK(isValidUpstreamAddress("tls://dns.adguard.com"));
	CHECK(isValidUpstreamAddress("tls://dns.adguard.com:853"));
	CHECK(isValidUpstreamAddress("https://cloudflare-dns.com/dns-query"));
	CHECK(isValidUpstreamAddress("https://dns.quad9.net:5053/dns-query"));
	CHECK(isValidUpstreamAddress("quic://dns.adguard.com:853"));
	CHECK(isValidUpstreamAddress("h3://dns.adguard.com/dns-query"));
	CHECK(isValidUpstreamAddress("sdns://AgMAAAAAAAAADzEuMC4wLjEgKHNpZ25hbCBzZXJ2ZXIp"));
}

TEST_CASE("isValidUpstreamAddress plain hostname needs bootstrap", "[dns][config]")
{
	CHECK(isValidUpstreamAddress("dns.adguard.com:53", true));
	CHECK_FALSE(isValidUpstreamAddress("dns.adguard.com:53", false));
	CHECK_FALSE(isValidUpstreamAddress("discord.com", false));
}

TEST_CASE("isValidUpstreamAddress rejects garbage", "[dns][config]")
{
	CHECK_FALSE(isValidUpstreamAddress(""));
	CHECK_FALSE(isValidUpstreamAddress("https://"));
	CHECK_FALSE(isValidUpstreamAddress("https://cloudflare-dns.com/dns-query with space"));
	CHECK_FALSE(isValidUpstreamAddress("1.2.3"));
	CHECK_FALSE(isValidUpstreamAddress("1.2.3.4:abc"));
	CHECK_FALSE(isValidUpstreamAddress("ftp://1.2.3.4"));
	CHECK_FALSE(isValidUpstreamAddress("-bad.com"));
	CHECK_FALSE(isValidUpstreamAddress("999.1.1.1"));
}

TEST_CASE("parseUpstreamValue splits bootstrap", "[dns][config]")
{
	const auto parsed = parseUpstreamValue("https://cloudflare-dns.com/dns-query|1.1.1.1, 1.0.0.1");
	REQUIRE(parsed.has_value());
	CHECK(parsed->address == "https://cloudflare-dns.com/dns-query");
	REQUIRE(parsed->bootstrap.size() == 2);
	CHECK(parsed->bootstrap[0] == "1.1.1.1");
	CHECK(parsed->bootstrap[1] == "1.0.0.1");
}

TEST_CASE("parseUpstreamValue tolerates empty bootstrap", "[dns][config]")
{
	const auto parsed = parseUpstreamValue("tls://dns.adguard.com");
	REQUIRE(parsed.has_value());
	CHECK(parsed->bootstrap.empty());
	CHECK_FALSE(parseUpstreamValue("nope->1.2.3.4").has_value());
}

TEST_CASE("parseProxyConfig reads full config", "[dns][config]")
{
	const std::string content = "# sample\n"
								"listen=127.0.0.1\n"
								"port=53\n"
								"upstream=https://cloudflare-dns.com/dns-query|1.1.1.1,1.0.0.1\n"
								"upstream=https://dns.google/dns-query|8.8.8.8\n"
								"status=C:/u/dns_proxy.status\n"
								"backup=C:/u/dns_proxy.adapters\n"
								"log=C:/tmp/dns.log\n";

	const auto [config, error] = parseProxyConfig(content);
	CHECK(error.empty());
	CHECK(config.listen == "127.0.0.1");
	CHECK(config.port == 53);
	REQUIRE(config.upstreams.size() == 2);
	CHECK(config.upstreams[0].bootstrap.size() == 2);
	CHECK(config.upstreams[1].bootstrap.size() == 1);
	CHECK(config.status_path == "C:/u/dns_proxy.status");
	CHECK(config.backup_path == "C:/u/dns_proxy.adapters");
	CHECK(config.log_path == "C:/tmp/dns.log");
}

TEST_CASE("parseProxyConfig rejects bad input", "[dns][config]")
{
	CHECK_FALSE(parseProxyConfig("upstream=https://dns.google/dns-query\n").second.empty() == false);

	const auto [c1, e1] = parseProxyConfig("port=53\n");
	CHECK(e1 == "No upstreams configured");

	const auto [c2, e2] = parseProxyConfig("upstream=discord.com\n");
	CHECK(e2 == "Bad upstream: discord.com");

	const auto [c3, e3] = parseProxyConfig("upstream=https://dns.google/dns-query\nport=99999\n");
	CHECK(e3 == "Bad port: 99999");

	const auto [c4, e4] = parseProxyConfig("upstream=https://dns.google/dns-query\nfrobnicate=1\n");
	CHECK(e4 == "Unknown key: frobnicate");
}
