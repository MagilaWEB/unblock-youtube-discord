#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "../zapret_helper.h"

/** Test harness with friend access to ZapretHelper internals. */
class ZapretHelperTest
{
public:
	ZapretHelper helper;

	bool isValidHost(std::string_view host) const { return ZapretHelper::_isValidHost(host); }

	void addHost(std::string_view host) { helper._addHost(host); }

	void handleMessage(std::string_view message) { helper._handleMessage(message); }

	const std::unordered_set<std::string>& knownHosts() const { return helper._known_hosts; }
	const std::unordered_set<std::string>& queue() const { return helper._queue; }
	const std::unordered_map<std::string, std::string>& errorHosts() const { return helper._error_hosts; }
	const std::unordered_map<std::string, std::string>& valid() const { return helper._valid; }
};

// ─── _isValidHost ─────────────────────────────────────────────

TEST_CASE("isValidHost empty", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK_FALSE(t.isValidHost(""));
}

TEST_CASE("isValidHost digits only (IP)", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK_FALSE(t.isValidHost("1.2.3.4"));
	CHECK_FALSE(t.isValidHost("0"));
}

TEST_CASE("isValidHost separators only", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK_FALSE(t.isValidHost(".:-"));
}

TEST_CASE("isValidHost plain domain", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK(t.isValidHost("google.com"));
	CHECK(t.isValidHost("www.google.com"));
	CHECK(t.isValidHost("example.org"));
}

TEST_CASE("isValidHost mixed letters and digits", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK(t.isValidHost("a1"));
	CHECK(t.isValidHost("1a"));
	CHECK(t.isValidHost("xn--80aswg.xn--p1ai"));
}

TEST_CASE("isValidHost single letter", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK(t.isValidHost("a"));
}

TEST_CASE("isValidHost underscores/hyphens are rejected without letters", "[helper][valid]")
{
	ZapretHelperTest t;
	CHECK_FALSE(t.isValidHost("_-_"));
	CHECK_FALSE(t.isValidHost("123-456"));
}

// ─── _addHost ──────────────────────────────────────────────────

TEST_CASE("addHost valid host goes to known and queue", "[helper][add]")
{
	ZapretHelperTest t;
	t.addHost("google.com");
	CHECK(t.knownHosts().contains("google.com"));
	CHECK(t.queue().contains("google.com"));
}

TEST_CASE("addHost invalid host is ignored", "[helper][add]")
{
	ZapretHelperTest t;
	t.addHost("1.2.3.4");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
}

TEST_CASE("addHost empty host is ignored", "[helper][add]")
{
	ZapretHelperTest t;
	t.addHost("");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
}

TEST_CASE("addHost duplicate host does not duplicate", "[helper][add]")
{
	ZapretHelperTest t;
	t.addHost("google.com");
	t.addHost("google.com");
	CHECK(t.knownHosts().size() == 1);
	CHECK(t.queue().size() == 1);
}

// ─── _handleMessage: LIST ──────────────────────────────────────

TEST_CASE("LIST multiple hosts", "[helper][list]")
{
	ZapretHelperTest t;
	t.handleMessage("LIST:google.com:youtube.com:discord.com");
	CHECK(t.knownHosts().size() == 3);
	CHECK(t.queue().size() == 3);
	CHECK(t.queue().contains("google.com"));
	CHECK(t.queue().contains("youtube.com"));
	CHECK(t.queue().contains("discord.com"));
}

TEST_CASE("LIST with duplicates keeps unique hosts", "[helper][list]")
{
	ZapretHelperTest t;
	t.handleMessage("LIST:a.com:b.com:a.com");
	CHECK(t.knownHosts().size() == 2);
	CHECK(t.queue().size() == 2);
}

TEST_CASE("LIST with invalid entries filters them out", "[helper][list]")
{
	ZapretHelperTest t;
	t.handleMessage("LIST:a.com:1.2.3.4:b.com:444");
	CHECK(t.knownHosts().size() == 2);
	CHECK(t.queue().contains("a.com"));
	CHECK(t.queue().contains("b.com"));
	CHECK_FALSE(t.queue().contains("1.2.3.4"));
	CHECK_FALSE(t.queue().contains("444"));
}

TEST_CASE("LIST with empty tail", "[helper][list]")
{
	ZapretHelperTest t;
	t.handleMessage("LIST:a.com:b.com:");
	CHECK(t.knownHosts().size() == 2);
	CHECK(t.queue().size() == 2);
}

TEST_CASE("LIST empty", "[helper][list]")
{
	ZapretHelperTest t;
	t.handleMessage("LIST:");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
}

TEST_CASE("LIST re-adds already known host", "[helper][list]")
{
	ZapretHelperTest t;
	t.handleMessage("LIST:a.com");
	t.handleMessage("LIST:a.com");
	CHECK(t.knownHosts().size() == 1);
	CHECK(t.queue().size() == 1);
}

// ─── _handleMessage: CHECK ─────────────────────────────────────

TEST_CASE("CHECK adds valid host", "[helper][check]")
{
	ZapretHelperTest t;
	t.handleMessage("CHECK:google.com");
	CHECK(t.knownHosts().contains("google.com"));
	CHECK(t.queue().contains("google.com"));
}

TEST_CASE("CHECK with invalid host ignored", "[helper][check]")
{
	ZapretHelperTest t;
	t.handleMessage("CHECK:1.2.3.4");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
}

TEST_CASE("CHECK empty ignored", "[helper][check]")
{
	ZapretHelperTest t;
	t.handleMessage("CHECK:");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
}

// ─── _handleMessage: STAT ──────────────────────────────────────

TEST_CASE("STAT records valid strategy", "[helper][stat]")
{
	ZapretHelperTest t;
	t.handleMessage("STAT:google.com:5");
	CHECK(t.valid().contains("google.com"));
	CHECK(t.valid().at("google.com") == "5");
}

TEST_CASE("STAT with empty strategy ignored", "[helper][stat]")
{
	ZapretHelperTest t;
	t.handleMessage("STAT:google.com:");
	CHECK(t.valid().empty());
}

TEST_CASE("STAT with invalid host ignored", "[helper][stat]")
{
	ZapretHelperTest t;
	t.handleMessage("STAT:1.2.3.4:5");
	CHECK(t.valid().empty());
}

TEST_CASE("STAT clears error for the host", "[helper][stat]")
{
	ZapretHelperTest t;
	t.handleMessage("ERR:google.com:3");
	CHECK(t.errorHosts().contains("google.com"));

	t.handleMessage("STAT:google.com:7");
	CHECK_FALSE(t.errorHosts().contains("google.com"));
	CHECK(t.valid().at("google.com") == "7");
}

// ─── _handleMessage: ERR ───────────────────────────────────────

TEST_CASE("ERR records strategy", "[helper][err]")
{
	ZapretHelperTest t;
	t.handleMessage("ERR:google.com:3");
	CHECK(t.errorHosts().contains("google.com"));
	CHECK(t.errorHosts().at("google.com") == "3");
}

TEST_CASE("ERR empty strategy recorded as empty string", "[helper][err]")
{
	ZapretHelperTest t;
	t.handleMessage("ERR:google.com:");
	CHECK(t.errorHosts().contains("google.com"));
	CHECK(t.errorHosts().at("google.com").empty());
}

TEST_CASE("ERR with invalid host ignored", "[helper][err]")
{
	ZapretHelperTest t;
	t.handleMessage("ERR:1.2.3.4:3");
	CHECK(t.errorHosts().empty());
}

TEST_CASE("ERR overwrites previous strategy", "[helper][err]")
{
	ZapretHelperTest t;
	t.handleMessage("ERR:google.com:3");
	t.handleMessage("ERR:google.com:9");
	CHECK(t.errorHosts().at("google.com") == "9");
}

// ─── _handleMessage: unknown ───────────────────────────────────

TEST_CASE("unknown prefix ignored", "[helper][unknown]")
{
	ZapretHelperTest t;
	t.handleMessage("FOO:google.com");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
	CHECK(t.valid().empty());
	CHECK(t.errorHosts().empty());
}

TEST_CASE("empty message ignored", "[helper][unknown]")
{
	ZapretHelperTest t;
	t.handleMessage("");
	CHECK(t.knownHosts().empty());
	CHECK(t.queue().empty());
}
