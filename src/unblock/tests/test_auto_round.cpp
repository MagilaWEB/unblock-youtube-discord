#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_set>

// unblock.h (via ipc_signals.h) needs SOCKET; the app TU gets it through
// the service headers, the test TU includes it directly.
#include <winsock2.h>

#include "../unblock.h"

namespace
{
	std::unordered_set<std::string> makeSet(std::initializer_list<const char*> items)
	{
		std::unordered_set<std::string> out;
		for (auto* item : items)
			out.emplace(item);
		return out;
	}
}	 // namespace

TEST_CASE("judgeAutoRound empty never wins", "[auto][judge]")
{
	CHECK_FALSE(judgeAutoRound(0, 0));
}

TEST_CASE("judgeAutoRound all alive wins", "[auto][judge]")
{
	CHECK(judgeAutoRound(0, 10));
}

TEST_CASE("judgeAutoRound 10 percent dead wins", "[auto][judge]")
{
	CHECK(judgeAutoRound(1, 10));
	CHECK(judgeAutoRound(2, 20));
}

TEST_CASE("judgeAutoRound over 10 percent dead fails", "[auto][judge]")
{
	CHECK_FALSE(judgeAutoRound(2, 10));
	CHECK_FALSE(judgeAutoRound(1, 5));
	CHECK_FALSE(judgeAutoRound(3, 20));
}

TEST_CASE("autoRoundSettled empty never settles", "[auto][settle]")
{
	CHECK_FALSE(autoRoundSettled({}, makeSet({ "a.com" }), {}));
}

TEST_CASE("autoRoundSettled waits for every host", "[auto][settle]")
{
	const auto expected = makeSet({ "a.com", "b.com" });
	CHECK_FALSE(autoRoundSettled(expected, makeSet({ "a.com" }), {}));
	CHECK_FALSE(autoRoundSettled(expected, {}, makeSet({ "a.com" })));
	CHECK(autoRoundSettled(expected, makeSet({ "a.com" }), makeSet({ "b.com" })));
	CHECK(autoRoundSettled(expected, makeSet({ "a.com", "b.com" }), {}));
}

TEST_CASE("autoRoundSettled error without exhausted is not terminal", "[auto][settle]")
{
	// Error hosts are not consulted at all: hunting in progress.
	const auto expected = makeSet({ "a.com" });
	CHECK_FALSE(autoRoundSettled(expected, {}, {}));
}

TEST_CASE("isHelperHostName mirrors helper filter", "[auto][hostname]")
{
	CHECK(isHelperHostName("google.com"));
	CHECK(isHelperHostName("a1"));
	CHECK_FALSE(isHelperHostName("1.2.3.4"));
	CHECK_FALSE(isHelperHostName(""));
	CHECK_FALSE(isHelperHostName("8080"));
}

TEST_CASE("parseHelperStats full", "[auto][stats]")
{
	const auto stats = parseHelperStats("47:20:200");
	REQUIRE(stats.has_value());
	CHECK(stats->queued == 47);
	CHECK(stats->in_check == 20);
	CHECK(stats->known == 200);
}

TEST_CASE("parseHelperStats zeros", "[auto][stats]")
{
	const auto stats = parseHelperStats("0:0:0");
	REQUIRE(stats.has_value());
	CHECK(stats->queued == 0);
	CHECK(stats->in_check == 0);
	CHECK(stats->known == 0);
}

TEST_CASE("parseHelperStats garbage rejected", "[auto][stats]")
{
	CHECK_FALSE(parseHelperStats("").has_value());
	CHECK_FALSE(parseHelperStats("47:20").has_value());
	CHECK_FALSE(parseHelperStats("47:20:x").has_value());
	CHECK_FALSE(parseHelperStats("47::200").has_value());
	CHECK_FALSE(parseHelperStats("47:20:200:1").has_value());
}
