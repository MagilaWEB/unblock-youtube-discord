#include <catch2/catch_test_macros.hpp>

#include "dns_config.h"

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
	// Plain hostnames are allowed; the global bootstrap resolves them.
	CHECK(isValidUpstreamAddress("dns.adguard.com:53"));
	CHECK(isValidUpstreamAddress("discord.com"));
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

TEST_CASE("upstreamNeedsBootstrap flags hostnames only", "[dns][config]")
{
	CHECK(upstreamNeedsBootstrap("https://cloudflare-dns.com/dns-query"));
	CHECK(upstreamNeedsBootstrap("https://dns.geohide.ru:8443/dns-query"));
	CHECK(upstreamNeedsBootstrap("tls://dns.adguard.com"));
	CHECK(upstreamNeedsBootstrap("dns.adguard.com:53"));

	CHECK_FALSE(upstreamNeedsBootstrap("1.1.1.1"));
	CHECK_FALSE(upstreamNeedsBootstrap("111.88.96.54:53"));
	CHECK_FALSE(upstreamNeedsBootstrap("https://1.1.1.1/dns-query"));
	CHECK_FALSE(upstreamNeedsBootstrap("sdns://AgMAAAAAAAAADzEuMC4wLjEgKHNpZ25hbCBzZXJ2ZXIp"));
}

TEST_CASE("isValidBootstrapAddress accepts IPv4 with optional port", "[dns][config]")
{
	CHECK(isValidBootstrapAddress("1.1.1.1"));
	CHECK(isValidBootstrapAddress("9.9.9.9"));
	CHECK(isValidBootstrapAddress("1.1.1.1:53"));
	CHECK_FALSE(isValidBootstrapAddress(""));
	CHECK_FALSE(isValidBootstrapAddress("dns.adguard.com"));
	CHECK_FALSE(isValidBootstrapAddress("1.2.3"));
	CHECK_FALSE(isValidBootstrapAddress("1.1.1.1:abc"));
}

TEST_CASE("parseBootstrapList splits, trims and dedupes", "[dns][config]")
{
	const auto parsed = parseBootstrapList("1.1.1.1, 8.8.8.8 ,9.9.9.9,1.1.1.1");
	REQUIRE(parsed.has_value());
	REQUIRE(parsed->size() == 3);
	CHECK((*parsed)[0] == "1.1.1.1");
	CHECK((*parsed)[1] == "8.8.8.8");
	CHECK((*parsed)[2] == "9.9.9.9");

	CHECK(parseBootstrapList("").value().empty());
	CHECK_FALSE(parseBootstrapList("1.1.1.1,dns.adguard.com").has_value());
}

TEST_CASE("parseProxyConfig reads full config", "[dns][config]")
{
	const std::string content = "# sample\n"
								"listen=127.0.0.1\n"
								"port=53\n"
								"timeout=12000\n"
								"bootstrap=1.1.1.1,8.8.8.8,9.9.9.9\n"
								"upstream=https://cloudflare-dns.com/dns-query\n"
								"upstream=1.1.1.1\n"
								"backup=C:/u/dns_proxy.adapters\n"
								"log=C:/tmp/dns.log\n";

	const auto [config, error] = parseProxyConfig(content);
	CHECK(error.empty());
	CHECK(config.listen == "127.0.0.1");
	CHECK(config.port == 53);
	CHECK(config.timeout_ms == 12'000);
	REQUIRE(config.bootstrap.size() == 3);
	CHECK(config.bootstrap[0] == "1.1.1.1");
	REQUIRE(config.upstreams.size() == 2);
	CHECK(config.upstreams[0] == "https://cloudflare-dns.com/dns-query");
	CHECK(config.upstreams[1] == "1.1.1.1");
	CHECK(config.backup_path == "C:/u/dns_proxy.adapters");
	CHECK(config.log_path == "C:/tmp/dns.log");
}

TEST_CASE("parseProxyConfig rejects bad input", "[dns][config]")
{
	const auto [c1, e1] = parseProxyConfig("port=53\n");
	CHECK(e1 == "No upstreams configured");

	const auto [c2, e2] = parseProxyConfig("upstream=discord.com:99:99\n");
	CHECK(e2 == "Bad upstream: discord.com:99:99");

	const auto [c3, e3] = parseProxyConfig("upstream=https://dns.google/dns-query\nport=99999\n");
	CHECK(e3 == "Bad port: 99999");

	const auto [c4, e4] = parseProxyConfig("upstream=https://dns.google/dns-query\nfrobnicate=1\n");
	CHECK(e4 == "Unknown key: frobnicate");

	const auto [c5, e5] = parseProxyConfig("bootstrap=nope\nupstream=1.1.1.1\n");
	CHECK(e5 == "Bad bootstrap: nope");

	const auto [c8, e8] = parseProxyConfig("timeout=abc\nupstream=1.1.1.1\n");
	CHECK(e8 == "Bad timeout: abc");

	const auto [c9, e9] = parseProxyConfig("timeout=100\nupstream=1.1.1.1\n");
	CHECK(e9 == "Bad timeout: 100");

	// A hostname upstream without any bootstrap cannot init in the DLL.
	const auto [c6, e6] = parseProxyConfig("upstream=https://dns.google/dns-query\n");
	CHECK(e6 == "Bootstrap required for hostname upstream: https://dns.google/dns-query");

	// Plain-IP upstream needs no bootstrap.
	const auto [c7, e7] = parseProxyConfig("upstream=1.1.1.1\n");
	CHECK(e7.empty());
}
