#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
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
	inline static constexpr size_t c_receive_buffer_size{ 65'536 };
	inline static constexpr auto   c_recheck_interval{ std::chrono::minutes(1) };
	inline static constexpr auto   c_sleep_short{ std::chrono::milliseconds(150) };

	std::mutex								_mutex;
	mutable std::mutex						_send_mutex;
	std::unordered_set<std::string>			_queue;
	std::unordered_set<std::string>			_known_hosts;
	std::unordered_map<std::string, std::string> _error_hosts;
	std::unordered_map<std::string, std::string> _valid;
	UdpSocket									 _socket;
	std::array<char, c_receive_buffer_size>		 _buffer{};
	std::thread									 _worker;
	u32											 _target_ip{ htonl(INADDR_LOOPBACK) };
	std::atomic<bool>							 _running{ true };
	std::chrono::steady_clock::time_point		 _last_recheck{};

public:
	ZapretHelper() = default;
	~ZapretHelper();

	/** Main loop: receive UDP messages and drive the check queue. */
	int run();

private:
	/** Host is valid (not empty and contains at least one letter). */
	static bool _isValidHost(std::string_view host);

	/** Send a UDP message to target_ip and the given port. */
	void _send(std::string_view message, u32 port) const;
	/** Send a log entry to unblock (port 9999). */
	void _log(std::string_view text) const;
	/** Split a ':'-separated host string and enqueue the hosts. */
	void _addHost(std::string_view hosts);
	/** Handle an incoming message (LIST or CHECK). */
	void _handleMessage(std::string_view message);
	/** Check a host via curl and report OK/FAIL. */
	void _checkHost(std::string_view host) const;
	/** Background worker: checks queued hosts in batches. */
	void _workerLoop();
};
