#include <catch2/catch_test_macros.hpp>

import std;
import core;
import core.file_system;
import core.localization;
import core.types;
import unblock.dns_proxy;
import unblock.tg_proxy;

// The UI offers these as the reset targets and the proxies fall back to them,
// so a renamed key or an unparsable config must fail here and not in the field.
// Needs the repository root as the working directory (see the CMakeLists note).
TEST_CASE("dns defaults come from configs/dns_proxy.config", "[unblock][defaults]")
{
	File config{ false };
	config.open(Core::get().configsPath() / "dns_proxy", ".config", true);
	REQUIRE(config.isOpen());

	CHECK(DnsProxy::defaultUpstreams() == config.parameterSectionVector("DEFAULTS", "upstream").value());
	CHECK(DnsProxy::defaultBootstrap() == config.parameterSectionVector("DEFAULTS", "bootstrap").value());
	CHECK(DnsProxy::defaultTimeout() == config.parameterSection<u32>("DEFAULTS", "timeout").value());

	// The shipped list keeps its priority order: GeoHide first, the rest only
	// as fallbacks.
	CHECK(DnsProxy::defaultUpstreams().front() == "https://geohide.ru/dns-query");
	CHECK(DnsProxy::defaultUpstreams().size() > 1);
	CHECK_FALSE(DnsProxy::defaultBootstrap().empty());
}

TEST_CASE("telegram proxy defaults come from configs/tg_proxy.config", "[unblock][defaults]")
{
	File config{ false };
	config.open(Core::get().configsPath() / "tg_proxy", ".config", true);
	REQUIRE(config.isOpen());

	CHECK(TgProxy::defaultHost() == config.parameterSection<std::string>("DEFAULTS", "host").value());
	CHECK(TgProxy::defaultPort() == config.parameterSection<std::string>("DEFAULTS", "port").value());
	CHECK(TgProxy::defaultCfproxyDomain() == config.parameterSection<std::string>("DEFAULTS", "cfproxy_domain").value());
	CHECK(TgProxy::defaultSecret() == config.parameterSection<std::string>("DEFAULTS", "secret").value());

	const auto dc_ip = config.parameterSectionVector("DEFAULTS", "dc_ip").value();
	REQUIRE(dc_ip.size() == 4);
	CHECK(TgProxy::defaultDcIp()[0] == dc_ip[0]);
	CHECK(TgProxy::defaultDcIp()[3] == dc_ip[3]);
}

// The hints that cite a default must keep a {} placeholder: the value is
// substituted at runtime, so changing a config does not leave a stale number in
// the translations.
TEST_CASE("default hints take their values from the configs", "[unblock][defaults]")
{
	Localization::get().set("US");

	const std::string bootstrap_hint = Localization::Str{ "str_dns_proxy_bootstrap_description", "1.2.3.4" }();
	CHECK(bootstrap_hint.contains("1.2.3.4"));
	CHECK_FALSE(bootstrap_hint.contains("1.1.1.1"));

	const std::string host_hint = Localization::Str{ "str_proxy_tg_host_description", "5.6.7.8" }();
	CHECK(host_hint.contains("5.6.7.8"));
	CHECK_FALSE(host_hint.contains("127.0.0.1"));

	const std::string port_hint = Localization::Str{ "str_proxy_tg_port_description", "1234" }();
	CHECK(port_hint.contains("1234"));
	CHECK_FALSE(port_hint.contains("9101"));
}
