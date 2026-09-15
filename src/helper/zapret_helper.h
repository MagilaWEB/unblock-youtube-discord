#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "curl_client.h"
#include "helper_config.h"
#include "net.h"

/**
 * Background zapret helper: receives host lists over UDP (10000),
 * checks their availability via curl and reports the result back.
 */
class ZapretHelper
{
	inline static constexpr u32	   c_receive_port{ 10'000 };
	inline static constexpr u32	   c_ipc_port{ 9'999 };
	inline static constexpr size_t c_receive_buffer_size{ 65'536 };
	inline static constexpr auto   c_sleep_short{ std::chrono::milliseconds(100) };
	// Seen-list broadcast throttle: the full host list is re-sent at most
	// this often. Unthrottled it flooded loopback UDP (~10Hz x N hosts) and
	// drowned one-shot CHECKING/DONE signals, so the UI under-reported busy
	// workers.
	inline static constexpr auto c_seen_interval{ std::chrono::milliseconds(500) };

	struct ErrorInfo
	{
		std::chrono::steady_clock::time_point first{};
		std::chrono::steady_clock::time_point last{};
		std::string							  strategy{};
	};

	std::mutex									 _mutex;
	std::condition_variable						 _cv;
	mutable std::mutex							 _send_mutex;
	std::unordered_set<std::string>				 _queue;
	std::unordered_set<std::string>				 _known_hosts;
	std::unordered_set<std::string>				 _in_check;
	std::unordered_map<std::string, ErrorInfo>	 _error_hosts;
	std::unordered_map<std::string, std::string> _valid_hosts;
	UdpSocket									 _socket;
	std::array<char, c_receive_buffer_size>		 _buffer{};
	std::vector<std::thread>					 _pool;
	u32											 _target_ip{ htonl(INADDR_LOOPBACK) };
	std::atomic<bool>							 _running{ true };
	std::chrono::steady_clock::time_point		 _last_recheck{};
	std::chrono::steady_clock::time_point		 _last_seen_send{};

	// Runtime settings: file fallback at startup (cold start / PC reboot),
	// fresh values always arrive via UDP CONFIG: pushed by Unblock.
	u32					 _pool_size{ HelperConfig::defaults().pool_size };
	std::chrono::minutes _recheck_interval{ HelperConfig::defaults().recheck_interval_min };
	std::chrono::minutes _errors_progress_interval{ HelperConfig::defaults().errors_progress_min };
	std::chrono::seconds _errors_recheck_interval{ HelperConfig::defaults().errors_recheck_sec };

#ifdef HELPER_TESTS
	friend class ZapretHelperTest;
#endif

public:
	ZapretHelper() = default;
	~ZapretHelper();

	/** Main loop: receive UDP messages and drive the check queue. */
	int run();

	/** Apply settings: curl timeouts immediately, intervals on next idle
	 *  step, pool is resized live when workers are already running. */
	void		 applyConfig(const HelperConfig& cfg);
	/** Current settings snapshot (for CONFIG: merge base). */
	HelperConfig currentConfig() const;

private:
	/** Host is valid (not empty and contains at least one letter). */
	static bool _isValidHost(std::string_view host);

	/** Voice media endpoint (c-<region>-<hash>.discord.media): not an HTTP
	 *  site, curl cannot validate it. Such hosts are skipped so their
	 *  verdicts never poison auto_strategy. */
	static bool _isVoiceMediaHost(std::string_view host);

	/** Send a UDP message to target_ip and the given port. */
	void					   _send(std::string_view message, u32 port) const;
	/** Send a log entry to unblock (port 9999). */
	void					   _log(std::string_view text) const;
	/** Split a ':'-separated host string and enqueue the hosts. */
	void					   _addHost(std::string_view hosts);
	/** Handle an incoming message (LIST or CHECK). */
	void					   _handleMessage(std::string_view message);
	/** Check a host via curl and report OK/FAIL. */
	void					   _checkHost(std::string_view host);
	/** Pop the next host (from queue, then error hosts) under mutex. */
	std::optional<std::string> _popHost();
	/** True if any host is waiting and not currently being checked. */
	bool					   _hasPendingHost() const;
	/** Background worker: waits for hosts and checks them one by one. */
	void					   _workerRoutine();
	/** Spawn n workers (run() startup path). */
	void					   _startWorkers(u32 count);
	/** Join all workers, keep _running untouched (live pool resize). */
	void					   _stopWorkers();
	/** Stop workers and join the pool. */
	void					   _stopPool();
	/** Handle UDP CONFIG: payload (Unblock push). */
	void					   _handleConfigMessage(std::string_view payload);

	// Message formatters (pure, no I/O) — unit-testable.
	static std::string _makeLog(std::string_view text);
	static std::string _makeValidSignal(std::string_view host, std::string_view strategy);
	static std::string _makeErrorSignal(std::string_view host, std::string_view strategy);
	static std::string _makeDoneSignal(std::string_view host);
	static std::string _makeCheckingSignal(std::string_view host);
	static std::string _makeSeenSignal(std::string_view host);
	static std::string _makeOk(std::string_view host);
	static std::string _makeFail(std::string_view host);

	/** Re-check due (interval passed) or report seen hosts. Called when queue is empty. */
	void _idleStep();
};
