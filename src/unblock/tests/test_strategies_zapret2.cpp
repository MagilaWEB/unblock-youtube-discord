#include "test_forwards.h"

#include <catch2/catch_test_macros.hpp>

import std;
import core;
import core.file_system;
import unblock.strategies_zapret2;

// Test harness with friend access to StrategiesZapret2 internals.
// NOTE: needs the repository root as the working directory (ctest
// WORKING_DIRECTORY): the port expansion reads configs/service_setting.config
// through Core paths.
class StrategiesZapret2Test
{
public:
	StrategiesZapret2 strategies;

	void normalize(std::string& line) { strategies._normalizeFinalLine(line); }
	void serviceConfig(const std::shared_ptr<File>& config) { strategies.serviceConfigFile(config); }
	void optionalServices(std::list<std::string> services) { strategies.changeOptionalServices(std::move(services)); }
};

TEST_CASE("normalizeFinalLine expands service ports", "[zapret2][ports]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "discord" });

	// Every zapret2 config carries these macros in its whitelist lines;
	// without the expansion winws2 lost every service port and never
	// intercepted the voice/game traffic at all.
	std::string line = "--wf-tcp=80,443,%DiscordPort%,%GamePort%";
	t.normalize(line);
	CHECK(line == "--wf-tcp=80,443,2053,2083,2087,2096,8443");

	line = "--wf-udp=443,%DiscordPortUPD%,%GamePort%";
	t.normalize(line);
	CHECK(line == "--wf-udp=443,19294-19344,50000-50100");
}

TEST_CASE("normalizeFinalLine sweeps leftover placeholders", "[zapret2][ports]")
{
	StrategiesZapret2Test t;

	std::string line = "--lua-init=%INIT_LUA%";
	t.normalize(line);
	CHECK(line.find('%') == std::string::npos);
}

// The four 1.6.x configs keep only their plans: the header and the tail (voice
// template plus the profiles) live in configs/templates.config and are spliced
// in by the generator before the section scan.
TEST_CASE("strategy config includes the shared templates", "[zapret2][templates]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "discord", "game_mod" });

	t.strategies.changeDirVersion("1.6.2");
	t.strategies.changeStrategy("strategy_super_d.config");

	const auto& args = t.strategies.getStrategy();

	auto contains = [&args](std::string_view needle)
	{ return std::ranges::any_of(args, [needle](const std::string& line) { return line.contains(needle); }); };

	// A token the generator does not expand would silently vanish in
	// _normalizeFinalLine, so the sweep is the main guard of the include.
	for (const auto& line : args)
		CHECK(line.find('%') == std::string::npos);

	CHECK(contains("--ctrack-disable=0"));	  // %TEMPLATES_HEAD%
	CHECK(contains("--blob=VOICE_PRE:@"));	  // blob used only inside the shared tail
	CHECK(contains("--lua-desync=fake:blob=VOICE_PRE:strategy=1"));
	CHECK(contains("--import=START_VOICE"));	// profiles from the shared tail
	CHECK(contains("--import=START_TLS"));
	CHECK(contains("--template=START_QUIC"));	 // the plan itself stays in the config
}

// The older versions share the same building blocks: their plans and the
// per-strategy choices (payload, import) stay in the config, everything else
// comes from configs/templates.config.
TEST_CASE("older strategy configs use the shared blocks", "[zapret2][templates]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "discord", "youtube", "game_mod" });

	t.strategies.changeDirVersion("1.4.16");
	t.strategies.changeStrategy("strategy_10.config");

	const auto& args = t.strategies.getStrategy();

	auto contains = [&args](std::string_view needle)
	{ return std::ranges::any_of(args, [needle](const std::string& line) { return line.contains(needle); }); };

	for (const auto& line : args)
		CHECK(line.find('%') == std::string::npos);

	CHECK(contains("--wf-tcp-in=80,443,2053,2083,2087,2096,8443"));	   // %TEMPLATES_WF% + ports
	CHECK(contains("--hostlist-domains=discord.media"));			   // %TEMPLATES_DISCORD_MEDIA%
	CHECK(contains("--import=START_DISCORD"));
	CHECK(contains("--filter-udp=0,1024-65535"));	 // %TEMPLATES_GAME_UDP% + ports
	CHECK(contains("--template=START_QUIC"));		 // plans stay in the config
}
