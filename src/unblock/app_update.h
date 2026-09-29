#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>

class HttpsLoad;

/** Application self-update. Checks GitHub for a newer release and hands the
 *  download off to the standalone unblock_update.exe. */
class AppUpdater final
{
public:
	/** Returns the newer version string, or nullopt when up to date / on error. */
	std::optional<std::string> check() const;

	/** Downloads the release, stages unblock_update.exe and spawns it. */
	bool run();

	/** Download progress in [0,1] of the active update, 0 when idle. */
	float progress() const;

private:
	mutable std::mutex		   _load_mutex;
	std::shared_ptr<HttpsLoad> _load;
};
