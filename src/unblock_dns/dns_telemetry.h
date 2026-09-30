#pragma once

#include "ag_dns.h"

#include <filesystem>
#include <string>

namespace dns
{
	// Opens the log file (truncated past 1MB) for appends.
	void openLog(const std::filesystem::path& path);

	// Appends one timestamped line under the log mutex.
	void logLine(const std::string& line);

	// DLL log sink: prefixes the level and forwards to logLine.
	void agLogCallback(void* attachment, ag_log_level level, const char* message, uint32_t length);

	// DLL request-processed sink: bumps the counters and logs errors throttled.
	void requestProcessedCallback(const ag_dns_request_processed_event* event);

	// Pushes the counters to the engine over UDP 9999 (IPCSignals LATEST keys).
	void pushStatus();
}	 // namespace dns
