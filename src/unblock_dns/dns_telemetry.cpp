#include "dns_telemetry.h"

#include <winsock2.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace dns
{
	namespace
	{
		struct Logger
		{
			std::mutex	  mutex;
			std::ofstream file;

			void open(const std::filesystem::path& path)
			{
				// Cap the log: start fresh past 1MB.
				std::error_code ec;
				if (std::filesystem::file_size(path, ec) > 1'024 * 1'024)
					std::filesystem::resize_file(path, 0, ec);

				file.open(path, std::ios::app);
			}

			void write(const std::string& line)
			{
				const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

				char stamp[32]{};
				ctime_s(stamp, sizeof(stamp), &now);
				stamp[strcspn(stamp, "\n")] = '\0';

				std::lock_guard lock{ mutex };
				if (file.is_open())
					file << "[" << stamp << "] " << line << "\n" << std::flush;
			}
		};

		Logger s_log;

		struct Stats
		{
			std::atomic_uint64_t queries{ 0 };
			std::atomic_uint64_t cache_hits{ 0 };
			std::atomic_uint64_t errors{ 0 };
			std::atomic_int32_t	 last_upstream{ -1 };
		};

		Stats s_stats;

		// Telemetry to the engine over UDP 9999 (IPCSignals). LATEST keys are
		// latest-only: the engine peeks them without consuming, so pushing every
		// few seconds never grows a queue (unlike the STRING event FIFO).
		void sendIpc(std::string_view message)
		{
			static SOCKET s_sock = INVALID_SOCKET;
			if (s_sock == INVALID_SOCKET)
				s_sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (s_sock == INVALID_SOCKET)
				return;

			sockaddr_in to{};
			to.sin_family	   = AF_INET;
			to.sin_port		   = htons(9'999);
			to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			sendto(s_sock, message.data(), static_cast<int>(message.size()), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
		}
	}	 // namespace

	void openLog(const std::filesystem::path& path)
	{
		s_log.open(path);
	}

	void logLine(const std::string& line)
	{
		s_log.write(line);
	}

	void agLogCallback(void* /*attachment*/, ag_log_level level, const char* message, uint32_t length)
	{
		if (!message || length == 0)
			return;

		std::string text{ message, length };
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
			text.pop_back();

		const char* prefix = "?";
		switch (level)
		{
		case AGLL_ERR:
			prefix = "E";
			break;
		case AGLL_WARN:
			prefix = "W";
			break;
		case AGLL_INFO:
			prefix = "I";
			break;
		case AGLL_DEBUG:
			prefix = "D";
			break;
		case AGLL_TRACE:
			prefix = "T";
			break;
		}

		logLine(std::string{ "[ag:" } + prefix + "] " + text);
	}

	void requestProcessedCallback(const ag_dns_request_processed_event* event)
	{
		if (!event)
			return;

		s_stats.queries.fetch_add(1);
		if (event->cache_hit)
			s_stats.cache_hits.fetch_add(1);
		if (event->error)
		{
			s_stats.errors.fetch_add(1);
			// Throttled: a browser sprays HTTPS(65)/AAAA queries and each
			// rejected one would otherwise flood the log. One line per second
			// is enough to see what is failing.
			static std::atomic_int64_t s_last_error_log_ms{ 0 };
			const auto now_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			int64_t	   previous = s_last_error_log_ms.load();
			if (now_ms - previous >= 1'000 && s_last_error_log_ms.compare_exchange_strong(previous, now_ms))
				logLine(
					std::string{ "query error [" } + (event->domain ? event->domain : "?") + " " + (event->type ? event->type : "?")
					+ "]: " + event->error
				);
		}
		if (event->upstream_id)
			s_stats.last_upstream.store(*event->upstream_id);
	}

	void pushStatus()
	{
		sendIpc("LATEST:dns.queries:" + std::to_string(s_stats.queries.load()));
		sendIpc("LATEST:dns.cache_hits:" + std::to_string(s_stats.cache_hits.load()));
		sendIpc("LATEST:dns.errors:" + std::to_string(s_stats.errors.load()));
	}
}	 // namespace dns
