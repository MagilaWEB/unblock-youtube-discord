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
#include "net.h"

/**
 * Background zapret helper: receives host lists over UDP (10000),
 * checks their availability via curl and reports the result back.
 */
class ZapretHelper
{
	inline static constexpr u32	   c_receive_port{ 10'000 };
	inline static constexpr u32	   c_ipc_port{ 9'999 };
	inline static constexpr u32	   c_pool_size{ 20 };
	inline static constexpr size_t c_receive_buffer_size{ 65'536 };
	inline static constexpr auto   c_recheck_interval{ std::chrono::minutes(30) };
	inline static constexpr auto   c_errors_progress_recheck_interval{ std::chrono::minutes(3) };
	inline static constexpr auto   c_errors_recheck_interval{ std::chrono::seconds(30) };
	inline static constexpr auto   c_sleep_short{ std::chrono::milliseconds(100) };

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

#ifdef HELPER_TESTS
	friend class ZapretHelperTest;
#endif

public:
	ZapretHelper() = default;
	~ZapretHelper();

	/** Main loop: receive UDP messages and drive the check queue. */
	int run();

private:
	/** Host is valid (not empty and contains at least one letter). */
	static bool _isValidHost(std::string_view host);

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
	/** Stop workers and join the pool. */
	void					   _stopPool();

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
