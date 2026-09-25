#include <catch2/catch_test_macros.hpp>

#include "../dns_host.h"

TEST_CASE("parseDnsPin accepts domain->IP", "[dns][pins]")
{
	const auto pin = parseDnsPin("updates.discord.com->162.159.138.232");
	REQUIRE(pin.has_value());
	CHECK(pin->first == "updates.discord.com");
	CHECK(pin->second == "162.159.138.232");
}

TEST_CASE("parseDnsPin tolerates surrounding whitespace", "[dns][pins]")
{
	const auto pin = parseDnsPin("  instagram.com  ->  157.240.0.174  ");
	REQUIRE(pin.has_value());
	CHECK(pin->first == "instagram.com");
	CHECK(pin->second == "157.240.0.174");
}

TEST_CASE("parseDnsPin rejects inverted IP->domain order", "[dns][pins]")
{
	// The old discord.list mistake: must not parse, otherwise the hosts
	// line would be written backwards and ignored by Windows.
	CHECK_FALSE(parseDnsPin("162.159.138.232->updates.discord.com").has_value());
}

TEST_CASE("parseDnsPin rejects plain domains and garbage", "[dns][pins]")
{
	CHECK_FALSE(parseDnsPin("discord.com").has_value());
	CHECK_FALSE(parseDnsPin("").has_value());
	CHECK_FALSE(parseDnsPin("# comment").has_value());
	CHECK_FALSE(parseDnsPin("   ").has_value());
	CHECK_FALSE(parseDnsPin("example.com->not-an-ip").has_value());
	CHECK_FALSE(parseDnsPin("999.999.999.999->example.com").has_value());
	CHECK_FALSE(parseDnsPin("no-arrow-here 1.2.3.4").has_value());
	CHECK_FALSE(parseDnsPin("a->b->c").has_value());
}
