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
