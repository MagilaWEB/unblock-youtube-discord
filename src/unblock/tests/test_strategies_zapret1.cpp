#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../strategies_zapret1.h"
#include "../strategies_zapret2.h"

// Test harness with friend access to StrategiesZapret1 internals.
// NOTE: needs the repository root as the working directory (ctest
// WORKING_DIRECTORY): the constructor reads the fake_bin_zapret1 config
// and asserts the fake blobs presence through Core paths.
class StrategiesZapret1Test
{
public:
	StrategiesZapret1 strategies;

	void							setFakeKey(const std::string& key) { strategies._fake_bind_key = key; }
	void							clearFakeParams() { strategies._fake_bin_params.clear(); }
	void							addFakeParam(const std::string& key) { strategies._fake_bin_params[key] = StrategiesZapret1::FakeBinParam{ .init = true }; }
	std::optional<std::string>		getFake(std::string_view line) { return strategies._getFake(line); }
	void							normalize(std::string& line) const { strategies._normalizeStrategyString(line); }
	bool							ignoring(std::string_view line) const { return strategies._ignoringLineStrategy(line); }
	void							ports(std::string& line) const { strategies._getAllPorts(line); }
	void							save(std::string_view line) { strategies._saveStrategies(line); }
	const std::vector<std::string>& built() const { return strategies._strategy_dpi; }

	void serviceConfig(const std::shared_ptr<File>& config) { strategies.serviceConfigFile(config); }
	void optionalServices(std::list<std::string> services) { strategies.changeOptionalServices(std::move(services)); }
};

static bool endsWith(const std::string& value, const std::string& suffix)
{
	return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

TEST_CASE("ctor reads fake profiles", "[zapret1][fake]")
{
	StrategiesZapret1Test t;

	const auto& params = t.strategies.getFakeBinList();
	CHECK_FALSE(params.empty());
	REQUIRE(params.contains("VK"));
	REQUIRE(params.contains("GOOGLE"));

	CHECK(params.at("VK").domain == "vk.com");
	CHECK(params.at("GOOGLE").domain == "www.google.com");
	CHECK(endsWith(params.at("VK").file_clienthello, "tls_clienthello_vk_com.bin"));
	CHECK(endsWith(params.at("VK").file_initial, "quic_initial_vk_com.bin"));
	CHECK(params.at("VK").init);

	REQUIRE(params.contains("4PDA"));
	CHECK(params.at("4PDA").domain == "4pda.to");
	CHECK(endsWith(params.at("4PDA").file_clienthello, "tls_clienthello_4pda_to.bin"));
	CHECK(endsWith(params.at("4PDA").file_initial, "quic_initial_4pda_to.bin"));

	REQUIRE(params.contains("SOCHI"));
	CHECK(params.at("SOCHI").domain.empty());
	CHECK(endsWith(params.at("SOCHI").file_clienthello, "tls_clienthello_sochi_park.bin"));

	std::vector<std::string> keys;
	for (const auto& [key, _] : params)
		keys.push_back(key);
	CHECK_FALSE(keys.empty());
}

TEST_CASE("changeDirVersion lists configs sorted", "[zapret1][config]")
{
	StrategiesZapret1Test t;

	t.strategies.changeDirVersion("1.4.16");
	const auto& list = t.strategies.getStrategyList();
	REQUIRE(list.size() == 19);
	CHECK(list.front() == "strategy_0.config");
	CHECK(list.back() == "strategy_18.config");

	t.strategies.changeDirVersion("1.4.10");
	CHECK(t.strategies.getStrategyList().size() == 19);
	CHECK(t.strategies.getStrategySize() == 19);

	t.strategies.changeDirVersion("1.6.2");
	const auto& flowseal = t.strategies.getStrategyList();
	REQUIRE(flowseal.size() == 22);
	CHECK(flowseal.front() == "strategy_0.config");
	CHECK(flowseal.back() == "strategy_21.config");
}

TEST_CASE("changeFakeKey roundtrip", "[zapret1][fake]")
{
	StrategiesZapret1Test t;

	t.strategies.changeFakeKey("VK");
	CHECK(t.strategies.getKeyFakeBin() == "VK");

	t.strategies.changeFakeKey("");
	CHECK(t.strategies.getKeyFakeBin().empty());
}

TEST_CASE("nextFakeKey cycles profiles with wraparound", "[zapret1][fake]")
{
	StrategiesZapret1Test t;

	const auto& params = t.strategies.getFakeBinList();
	REQUIRE(params.size() >= 2);

	std::vector<std::string> keys;
	for (const auto& [key, _] : params)
		keys.push_back(key);

	for (size_t i = 0; i < keys.size(); ++i)
	{
		auto next = t.strategies.nextFakeKey(keys[i]);
		REQUIRE(next.has_value());
		CHECK(*next == keys[(i + 1) % keys.size()]);
	}

	auto unknown = t.strategies.nextFakeKey("NO_SUCH_PROFILE");
	REQUIRE(unknown.has_value());
	CHECK(*unknown == keys.front());
}

TEST_CASE("nextFakeKey degrades without profiles", "[zapret1][fake]")
{
	StrategiesZapret1Test t;

	t.clearFakeParams();
	CHECK_FALSE(t.strategies.nextFakeKey("VK").has_value());

	t.addFakeParam("SOLO");
	CHECK_FALSE(t.strategies.nextFakeKey("SOLO").has_value());
}

TEST_CASE("getFake expands placeholders", "[zapret1][fake]")
{
	StrategiesZapret1Test t;
	t.strategies.changeFakeKey("VK");

	auto tls = t.getFake("%FAKE_TLS%");
	REQUIRE(tls.has_value());
	CHECK(tls->starts_with("--dpi-desync-fake-tls \""));
	CHECK(endsWith(*tls, "tls_clienthello_vk_com.bin\""));

	auto quic = t.getFake("%FAKE_QUIC%");
	REQUIRE(quic.has_value());
	CHECK(quic->starts_with("--dpi-desync-fake-quic \""));
	CHECK(endsWith(*quic, "quic_initial_vk_com.bin\""));

	auto tls_domain = t.getFake("%FAKE_TLS_DOMAIN%");
	REQUIRE(tls_domain.has_value());
	CHECK(*tls_domain == "--dpi-desync-fake-tls-mod=rnd,dupsid,sni=vk.com");

	auto host_domain = t.getFake("--filter-tcp=443\n%FAKE_HOST_DOMAIN%");
	REQUIRE(host_domain.has_value());
	CHECK(host_domain->find("--dpi-desync-hostfakesplit-mod=host=vk.com") != std::string::npos);

	auto seqovl = t.getFake("%SEQOVL_PATTERN%");
	REQUIRE(seqovl.has_value());
	CHECK(seqovl->find("--dpi-desync-split-seqovl-pattern \"") != std::string::npos);
	CHECK(endsWith(*seqovl, "tls_clienthello_vk_com.bin\""));

	auto unknown_udp = t.getFake("%FAKE_UNKNOWN%");
	REQUIRE(unknown_udp.has_value());
	CHECK(unknown_udp->find("--dpi-desync-fake-unknown-udp \"") != std::string::npos);

	auto discord = t.getFake("%FAKE_DISCORD%");
	REQUIRE(discord.has_value());
	CHECK(discord->find("--dpi-desync-fake-discord \"") != std::string::npos);

	auto stun = t.getFake("%FAKE_STUN%");
	REQUIRE(stun.has_value());
	CHECK(stun->find("--dpi-desync-fake-stun \"") != std::string::npos);

	auto http = t.getFake("%FAKE_HTTP%");
	REQUIRE(http.has_value());
	CHECK(http->find("--dpi-desync-fake-http \"") != std::string::npos);

	auto hello = t.getFake("--dpi-desync-fake-tls=%FAKE_CLIENT_HELLO%");
	REQUIRE(hello.has_value());
	CHECK(hello->find("tls_clienthello_vk_com.bin") != std::string::npos);
	CHECK(hello->find("%FAKE_CLIENT_HELLO%") == std::string::npos);

	auto initial = t.getFake("--dpi-desync-fake-quic=%FAKE_INITIAL%");
	REQUIRE(initial.has_value());
	CHECK(initial->find("quic_initial_vk_com.bin") != std::string::npos);

	CHECK_FALSE(t.getFake("--dpi-desync=fake").has_value());
}

TEST_CASE("getFake without a profile drops the line", "[zapret1][fake]")
{
	StrategiesZapret1Test t;
	t.setFakeKey("");

	auto expanded = t.getFake("%FAKE_TLS%");
	REQUIRE(expanded.has_value());
	CHECK(expanded->empty());

	// Plain lines still pass through to the base handler.
	CHECK_FALSE(t.getFake("--dpi-desync=fake").has_value());
}

TEST_CASE("normalizeStrategyString", "[zapret1][normalize]")
{
	StrategiesZapret1Test t;

	std::string line = "--wf-tcp=80,443,";
	t.normalize(line);
	CHECK(line == "--wf-tcp=80,443");

	line = "--filter-tcp=,80,,443";
	t.normalize(line);
	CHECK(line == "--filter-tcp=80,443");

	line = "--dpi-desync=fake";
	t.normalize(line);
	CHECK(line == "--dpi-desync=fake");
}

TEST_CASE("ignoringLineStrategy", "[zapret1][normalize]")
{
	StrategiesZapret1Test t;

	CHECK(t.ignoring(""));
	CHECK(t.ignoring("// comment"));
	CHECK_FALSE(t.ignoring("--wf-tcp=80,443"));
	CHECK_FALSE(t.ignoring("%BLOCKLIST%"));
}

TEST_CASE("getAllPorts substitutes service ports", "[zapret1][ports]")
{
	StrategiesZapret1Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "discord" });

	std::string line = "--wf-tcp=80,443,%DiscordPort%,%GamePort%";
	t.ports(line);
	CHECK(line == "--wf-tcp=80,443,2053,2083,2087,2096,8443");

	line = "--filter-udp=%DiscordPortUPD%";
	t.ports(line);
	CHECK(line == "--filter-udp=19294-19344,50000-50100");
}

TEST_CASE("save expands %FAKE% and splits =", "[zapret1][save]")
{
	StrategiesZapret1Test t;

	// Classic zapret takes file arguments space-separated, not with '='.
	t.save("--dpi-desync-fake-tls=\"%FAKE%stun.bin\"");
	REQUIRE(t.built().size() == 1);

	const auto& line = t.built().back();
	CHECK(line.starts_with("--dpi-desync-fake-tls \""));
	CHECK(endsWith(line, "stun.bin\""));
	CHECK(line.find('=') == std::string::npos);
	CHECK(line.find('%') == std::string::npos);

	// Comment lines are skipped.
	t.save("// a comment");
	CHECK(t.built().size() == 1);
}

TEST_CASE("full pipeline builds clean winws args", "[zapret1][pipeline]")
{
	StrategiesZapret1Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);

	t.strategies.changeCustomLists({}, {}, {}, {});
	t.optionalServices({ "discord", "youtube", "game_mod" });
	t.strategies.changeDirVersion("1.4.16");
	t.strategies.changeFakeKey("VK");
	t.strategies.changeStrategy("strategy_0.config");

	CHECK(t.strategies.getStrategyFileName() == "strategy_0.config");

	const auto& args = t.strategies.getStrategy();
	CHECK_FALSE(args.empty());

	for (const auto& line : args)
	{
		INFO(line);
		CHECK(line.find('%') == std::string::npos);
		CHECK(line.find("fale") == std::string::npos);
		CHECK(line.find("MessengersPort") == std::string::npos);
		CHECK(line.find("TorrentPort") == std::string::npos);
	}
}

TEST_CASE("1.6.2 flowseal pipeline builds clean winws args", "[zapret1][pipeline]")
{
	StrategiesZapret1Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);

	t.strategies.changeCustomLists({}, {}, {}, {});
	t.optionalServices({ "discord", "youtube", "game_mod" });
	t.strategies.changeDirVersion("1.6.2");
	t.strategies.changeFakeKey("VK");
	t.strategies.changeStrategy("strategy_0.config");

	const auto& args = t.strategies.getStrategy();
	REQUIRE_FALSE(args.empty());

	bool has_voice = false;
	for (const auto& line : args)
	{
		INFO(line);
		CHECK(line.find('%') == std::string::npos);
		CHECK(line.find("%BIN%") == std::string::npos);
		CHECK(line.find("%LISTS%") == std::string::npos);
		CHECK(line.find("GameFilter") == std::string::npos);
		CHECK(line.find("--filter-l3") == std::string::npos);
		if (line.contains("--filter-l7=discord,stun"))
			has_voice = true;
	}
	CHECK(has_voice);
}

TEST_CASE("zapret2 pipeline expands lua-init and blob defs", "[zapret2][pipeline]")
{
	StrategiesZapret2 strategies;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	strategies.serviceConfigFile(config);

	strategies.changeCustomLists({}, {}, {}, {});
	strategies.changeOptionalServices({ "discord", "youtube", "game_mod" });
	strategies.changeDirVersion("1.6.0");
	strategies.changeStrategy("strategy_super_a.config");

	const auto& args = strategies.getStrategy();
	REQUIRE_FALSE(args.empty());

	bool has_lua_init	= false;
	bool has_blob_def	= false;
	bool has_raw_marker = false;
	for (const auto& line : args)
	{
		INFO(line);
		if (line.starts_with("--lua-init="))
			has_lua_init = true;
		if (line.starts_with("--blob=") && line.find('@') != std::string::npos)
			has_blob_def = true;
		if (line.contains("%INIT_"))
			has_raw_marker = true;
	}

	CHECK(has_lua_init);
	CHECK(has_blob_def);
	CHECK_FALSE(has_raw_marker);
}

TEST_CASE("zapret2 1.6.2 super_c/super_d build clean args", "[zapret2][pipeline]")
{
	for (const std::string name : { "strategy_super_c.config", "strategy_super_d.config" })
	{
		StrategiesZapret2 strategies;

		auto config = std::make_shared<File>();
		config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
		REQUIRE(config->isOpen());
		strategies.serviceConfigFile(config);

		strategies.changeCustomLists({}, {}, {}, {});
		strategies.changeOptionalServices({ "discord", "youtube", "game_mod" });
		strategies.changeDirVersion("1.6.2");
		strategies.changeStrategy(name);

		const auto& args = strategies.getStrategy();
		REQUIRE_FALSE(args.empty());

		bool has_lua_init = false;
		bool has_new_blob = false;
		for (const auto& line : args)
		{
			INFO(name + ": " + line);
			CHECK(line.find('%') == std::string::npos);
			if (line.starts_with("--lua-init="))
				has_lua_init = true;
			if (line.contains("TLS_SOCHI") || line.contains("QUIC_4PDA") || line.contains("DISCORD_ACTIVE") || line.contains("STUN2")
				|| line.contains("GAME_ACTIVE") || line.contains("TLS_4PDA") || line.contains("TLS_MAX"))
				has_new_blob = true;
		}

		CHECK(has_lua_init);
		CHECK(has_new_blob);
	}
}
