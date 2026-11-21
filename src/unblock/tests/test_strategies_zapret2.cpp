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

// 1.5.0 never switched to %TEMPLATES_HEAD%: it keeps its own header, so the
// blob definitions living in the shared HEAD do not reach it. The voice drover
// inlined into that set references JUNK_00/JUNK_01, whose definitions sat only
// in the shared header, and every 1.5.0 config shipped two blobs that did not
// exist (_blob_init_to_zapret warned at load). The definitions are now inline
// in each 1.5.0 header; this keeps them there when a plan changes.
TEST_CASE("1.5.0 strategy configs define every blob they reference", "[zapret2][templates]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "discord", "youtube", "game_mod" });

	t.strategies.changeDirVersion("1.5.0");

	static const std::regex			   blob_ref{ R"(:blob=([^:]+)|:seqovl_pattern=([^:]+)|:pattern=([^:]+))" };
	// Engine built-ins: winws2 registers these names itself (ApplyDefaultBlobs
	// in zapret2's nfq2/nfqws.c), fake_default_udp/tcp are config-defined and
	// must have a --blob= line of their own.
	static const std::set<std::string> builtin{ "fake_default_tls", "fake_default_http", "fake_default_quic" };

	const auto files = t.strategies.getStrategyList();
	REQUIRE_FALSE(files.empty());

	for (const auto& file : files)
	{
		t.strategies.changeStrategy(file);

		const auto& args = t.strategies.getStrategy();

		std::set<std::string> defined;
		for (const auto& line : args)
			if (line.starts_with("--blob="))
				defined.insert(line.substr(7, line.find(':', 7) - 7));

		for (const auto& line : args)
			for (auto it = std::sregex_iterator(line.begin(), line.end(), blob_ref); it != std::sregex_iterator(); ++it)
			{
				std::string key;
				for (auto group : std::ranges::iota_view(1U, (*it).size()))
					if ((*it)[group].matched)
						key = (*it)[group].str();

				CHECK_FALSE(key.empty());

				if (!builtin.contains(key))
					CHECK(defined.contains(key));
			}
	}
}

// The YouTube streaming profile: its own QUIC plan scoped to the video hosts,
// with --payload=all so the desync can touch the start of the whole flow rather
// than only the QUIC Initial. It has to sit in front of the generic UDP/443
// profile: a connection binds to the first matching profile, and that ordering
// is what keeps every other QUIC flow out of it.
TEST_CASE("youtube streaming gets its own QUIC profile ahead of the generic one", "[zapret2][templates]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "youtube" });

	t.strategies.changeDirVersion("1.6.2");
	t.strategies.changeStrategy("strategy_super_c.config");

	const auto& args = t.strategies.getStrategy();

	auto index_of = [&args](std::string_view needle)
	{
		for (size_t i = 0; i < args.size(); ++i)
			if (args[i].contains(needle))
				return i;
		return args.size();
	};

	const auto stream  = index_of("--hostlist-domains=googlevideo.com,c.youtube.com");
	const auto generic = index_of("--payload=quic_initial");

	REQUIRE(stream < args.size());
	REQUIRE(generic < args.size());
	CHECK(stream < generic);

	// The payload scope is the point of the profile, and it must stay inside it:
	// everything after the generic profile takes over is back to quic_initial.
	bool payload_all = false;
	for (size_t i = stream; i < generic; ++i)
		payload_all = payload_all || args[i] == "--payload=all";
	CHECK(payload_all);

	CHECK(index_of("--out-range=-n4") < generic);
	CHECK(index_of("--lua-desync=fake:blob=QUIC_4PDA:repeats=6:badsum:strategy=12") < generic);

	// The generic UDP/443 profile lives in [END] now: its %BLOCKLIST% resolves
	// to all.list and it no longer disappears when youtube is off. Section
	// markers are not part of the assembled args, so the youtube run is bounded
	// by the stream profile and the END TCP profile - the run itself must not
	// carry quic_initial, and the profile must sit past the END one.
	const auto youtube_end = index_of("--filter-tcp=80,443");
	const auto end_tls	   = index_of("--import=START_TLS");
	REQUIRE(youtube_end < args.size());
	REQUIRE(end_tls < args.size());

	bool quic_in_youtube = false;
	for (size_t i = stream; i < youtube_end; ++i)
		quic_in_youtube = quic_in_youtube || args[i].contains("--payload=quic_initial");
	CHECK_FALSE(quic_in_youtube);

	CHECK(generic > end_tls);
}

// Same ordering guarantee for the older sets, which splice the profiles from
// the config instead of the shared tail: the youtube section must still open
// before the generic UDP/443 profile in 1.4.16.
TEST_CASE("1.4.16 keeps the youtube QUIC profile ahead of the generic one", "[zapret2][templates]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "youtube" });

	t.strategies.changeDirVersion("1.4.16");
	t.strategies.changeStrategy("strategy_10.config");

	const auto& args = t.strategies.getStrategy();

	auto index_of = [&args](std::string_view needle)
	{
		for (size_t i = 0; i < args.size(); ++i)
			if (args[i].contains(needle))
				return i;
		return args.size();
	};

	const auto stream  = index_of("--hostlist-domains=googlevideo.com,c.youtube.com");
	const auto generic = index_of("--payload=quic_initial");

	REQUIRE(stream < args.size());
	REQUIRE(generic < args.size());
	CHECK(stream < generic);

	bool payload_all = false;
	for (size_t i = stream; i < generic; ++i)
		payload_all = payload_all || args[i] == "--payload=all";
	CHECK(payload_all);

	CHECK(index_of("--out-range=-n4") < generic);
	CHECK(index_of("--lua-desync=fake:blob=QUIC_4PDA:repeats=6:badsum:strategy=12") < generic);

	// The generic UDP/443 profile lives in [END] now: its %BLOCKLIST% resolves
	// to all.list and it no longer disappears when youtube is off. Section
	// markers are not part of the assembled args, so the youtube run is bounded
	// by the stream profile and the END TCP profile - the run itself must not
	// carry quic_initial, and the profile must sit past the END one.
	const auto youtube_end = index_of("--filter-tcp=80,443");
	const auto end_tls	   = index_of("--import=START_TLS");
	REQUIRE(youtube_end < args.size());
	REQUIRE(end_tls < args.size());

	bool quic_in_youtube = false;
	for (size_t i = stream; i < youtube_end; ++i)
		quic_in_youtube = quic_in_youtube || args[i].contains("--payload=quic_initial");
	CHECK_FALSE(quic_in_youtube);

	CHECK(generic > end_tls);
}

// The generic UDP/443 profile lives in [END] and covers every blocklisted host,
// so it must outlive a configuration without youtube: while it sat inside the
// [youtube] section, switching youtube off dropped it from the assembled args
// and left the QUIC flows of every other service undesynced.
TEST_CASE("generic QUIC profile survives with youtube off", "[zapret2][templates]")
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

	CHECK_FALSE(contains("--hostlist-domains=googlevideo.com,c.youtube.com"));	  // youtube is off
	CHECK(contains("--payload=quic_initial"));									  // the generic profile stays
}

// ... and in 1.5.0, whose tail is inlined into every config.
TEST_CASE("1.5.0 keeps the youtube QUIC profile ahead of the generic one", "[zapret2][templates]")
{
	StrategiesZapret2Test t;

	auto config = std::make_shared<File>();
	config->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	REQUIRE(config->isOpen());
	t.serviceConfig(config);
	t.optionalServices({ "youtube" });

	t.strategies.changeDirVersion("1.5.0");
	t.strategies.changeStrategy("strategy_super.config");

	const auto& args = t.strategies.getStrategy();

	auto index_of = [&args](std::string_view needle)
	{
		for (size_t i = 0; i < args.size(); ++i)
			if (args[i].contains(needle))
				return i;
		return args.size();
	};

	const auto stream  = index_of("--hostlist-domains=googlevideo.com,c.youtube.com");
	const auto generic = index_of("--payload=quic_initial");

	REQUIRE(stream < args.size());
	REQUIRE(generic < args.size());
	CHECK(stream < generic);

	bool payload_all = false;
	for (size_t i = stream; i < generic; ++i)
		payload_all = payload_all || args[i] == "--payload=all";
	CHECK(payload_all);

	CHECK(index_of("--out-range=-n4") < generic);
	CHECK(index_of("--lua-desync=fake:blob=QUIC_4PDA:repeats=6:badsum:strategy=12") < generic);

	// The generic UDP/443 profile lives in [END] now: its %BLOCKLIST% resolves
	// to all.list and it no longer disappears when youtube is off. Section
	// markers are not part of the assembled args, so the youtube run is bounded
	// by the stream profile and the END TCP profile - the run itself must not
	// carry quic_initial, and the profile must sit past the END one.
	const auto youtube_end = index_of("--filter-tcp=80,443");
	const auto end_tls	   = index_of("--import=START_TLS");
	REQUIRE(youtube_end < args.size());
	REQUIRE(end_tls < args.size());

	bool quic_in_youtube = false;
	for (size_t i = stream; i < youtube_end; ++i)
		quic_in_youtube = quic_in_youtube || args[i].contains("--payload=quic_initial");
	CHECK_FALSE(quic_in_youtube);

	CHECK(generic > end_tls);
}
