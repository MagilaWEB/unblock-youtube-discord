#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include "types.h"

/**
 * Runtime settings of zapret-helper, section [HELPER] in user/setting.config.
 *
 * The helper is a standalone exe (CURL + ws2_32 only), so this
 * parser is
 * intentionally dependency-free: no core/File, no regex. It reads ONLY
 * the [HELPER] section, everything else is ignored. Missing file
 * or broken
 * values fall back to compiled-in defaults.
 *
 * NOTE: user/setting.config on disk is stale while unblock is running
 * (File::save
 * happens on close), so the file is only a fallback for cold
 * start / PC reboot. The fresh values always arrive via UDP CONFIG: pushed
 * by
 * Unblock::startService right after the helper is launched.
 */
struct HelperConfig
{
	u32 pool_size{ 20 };			   // worker threads, 1..64
	u32 check_timeout_sec{ 6 };		   // curl overall timeout, 1..60
	u32 connect_timeout_sec{ 5 };	   // curl connect timeout, 1..30
	u32 max_redirects{ 5 };			   // curl follow redirects, 0..10
	u32 recheck_interval_min{ 30 };	 // full recheck of known hosts, 5..180
	u32 errors_progress_min{ 3 };	 // aggressive recheck phase after first error, 1..30
	u32 errors_recheck_sec{ 30 };	 // stale-error recheck interval, 5..300

	/** Compiled-in defaults (match the old constexpr values). */
	static HelperConfig defaults() { return HelperConfig{}; }

	/** Clamp every field into its valid range. */
	void normalize();

	/** Resolve <exe_dir>/../user/setting.config (bin -> root/user). */
	static std::filesystem::path resolvePath();

	/** Load from the resolved path; missing/broken file -> defaults. */
	static HelperConfig load();
	/** Load from an explicit path (tests, custom layouts). */
	static HelperConfig loadFrom(const std::filesystem::path& path);

	/** Parse UDP payload "k=v;k=v..." (CONFIG: body) over the base config. */
	static HelperConfig parsePayload(std::string_view payload, const HelperConfig& base = defaults());

	/** Serialize to UDP message "CONFIG:k=v;k=v...". */
	std::string makeMessage() const;
};
