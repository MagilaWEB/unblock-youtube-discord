#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/** Helper pool load snapshot ("queued|in_check|known"). Header-inline so
 *  unit tests link without the engine. */
struct HelperStats
{
	size_t queued{ 0 };
	size_t in_check{ 0 };
	size_t known{ 0 };
};

inline std::optional<HelperStats> parseHelperStats(std::string_view text)
{
	HelperStats out{};
	size_t*		fields[3]{ &out.queued, &out.in_check, &out.known };
	size_t		index = 0;

	for (auto part_range : text | std::views::split('|'))
	{
		if (index >= 3)
			return std::nullopt;

		const std::string_view part{ std::ranges::data(part_range), std::ranges::size(part_range) };

		unsigned long long num{};
		const auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), num);
		if (ec != std::errc{} || ptr != part.data() + part.size())
			return std::nullopt;

		*fields[index++] = static_cast<size_t>(num);
	}

	if (index != 3)
		return std::nullopt;

	return out;
}

/** In-flight helper checks with a short done grace. The helper reports a
 *  fast DONE edge while the terminal valid/error/exhausted verdict only
 *
 * arrives with the next snapshot, so removing a host on DONE alone makes
 *  the checking list blink. Recently finished hosts stay visible until a
 *
 * verdict arrives or the grace expires. Header-inline for unit tests. */
class HelperCheckingTracker
{
public:
	explicit HelperCheckingTracker(std::chrono::steady_clock::duration grace_ttl = std::chrono::seconds(1)) : _grace_ttl(grace_ttl) {}

	void checkingEdge(std::string host, std::chrono::steady_clock::time_point now)
	{
		(void)now;
		_active.insert(host);
		_grace.erase(host);
	}

	void doneEdge(std::string host, bool terminal, std::chrono::steady_clock::time_point now)
	{
		_active.erase(host);
		if (terminal)
		{
			_grace.erase(host);
			return;
		}

		_grace[std::move(host)] = now;
	}

	void verdict(std::string_view host)
	{
		const std::string name{ host };
		_active.erase(name);
		_grace.erase(name);
	}

	std::vector<std::string> visible(std::chrono::steady_clock::time_point now)
	{
		prune(now);

		std::vector<std::string> out;
		out.reserve(_active.size() + _grace.size());
		for (const auto& host : _active)
			out.emplace_back(host);
		for (const auto& [host, _] : _grace)
			if (!_active.contains(host))
				out.emplace_back(host);
		return out;
	}

	void clear()
	{
		_active.clear();
		_grace.clear();
	}

private:
	void prune(std::chrono::steady_clock::time_point now)
	{
		for (auto it = _grace.begin(); it != _grace.end();)
			if (now - it->second > _grace_ttl)
				it = _grace.erase(it);
			else
				++it;
	}

	std::unordered_set<std::string>										   _active;
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> _grace;
	std::chrono::steady_clock::duration									   _grace_ttl;
};

/** Aggregation of the helper IPC on port 9999: the host verdict sets
 *  (checking/seen/error/valid/exhausted) rebuilt from the helper snapshots,
 *
 * plus the CONFIG:/LIST: pushes back to the helper process. */
class HelperState final
{
public:
	std::vector<std::string>						 checkingHosts();
	std::vector<std::string>						 seenHosts();
	std::vector<std::pair<std::string, std::string>> errorHosts();
	std::vector<std::pair<std::string, std::string>> validHosts();
	/** Fully-tried hosts: every strategy failed, nothing left to attempt. */
	std::vector<std::pair<std::string, std::string>> exhaustedHosts();

	/** Last helper pool load snapshot (zeros when the helper is silent). */
	HelperStats stats();

	/** Drops every list without waiting for the TTL (used on start/stop). */
	void clear();

	/** Fresh [HELPER] payload from the UI (in-memory userConfig, not the
	 *  stale on-disk file). Sent as UDP CONFIG: right after the helper is
	 *
	 * launched and on Apply while it is running. */
	void setConfigMessage(std::string message) { _config_message = std::move(message); }
	void pushConfig() const;
	/** Sends the LIST: payload carrying the bare hostnames to probe. */
	void sendHostList(const std::vector<std::string>& hosts) const;

private:
	// The helper re-broadcasts the full host snapshot every tick, always
	// including helper_seen, so its age is the liveness signal. No fresh
	// snapshot past the TTL means every list is stale.
	static constexpr auto c_signal_ttl{ std::chrono::seconds(5) };

	bool _dropExpired();

	// The checking/error/valid/exhausted lists are mutually exclusive: one
	// host lives in exactly one of them (seen stays out of the sync).
	HelperCheckingTracker						 _checking;
	std::unordered_set<std::string>				 _seen;
	std::unordered_map<std::string, std::string> _errors;
	std::unordered_map<std::string, std::string> _valid;
	std::unordered_map<std::string, std::string> _exhausted;
	HelperStats									 _stats{};
	std::string									 _config_message{};

	// Guards every container above (Ui::update() and the passive autopick
	// worker both touch them).
	std::mutex _lock;
};
