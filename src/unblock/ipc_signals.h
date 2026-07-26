#pragma once
/*
 * IPCSignals — UDP-based IPC from Lua (zapret) to C++ (unblock).
 *
 * Lua sends:   TYPE:KEY:VALUE    to 127.0.0.1:9999
 *
 * Log signals (printed immediately, not stored):
 *   LOG:OK:text      → Debug::ok("IPC: text")
 *   LOG:WARNING:text → Debug::warning("IPC: text")
 *   LOG:INFO:text    → Debug::info("IPC: text")
 *
 * Data signals (buffered in FIFO queue per key, with TTL):
 *   BOOL:name:value   → getBool("name")     value: true/false/1/0
 *   STRING:name:value → getString("name")   value: any text
 *   U32:name:number   → getU32("name")      value: unsigned integer
 *   FLOAT:name:number → getFloat("name")    value: float
 *
 * NAME allows: letters, digits, '.', '-', '_', '/', ':', '@'
 * VALUE in LOG is any text.
 * VALUE in BOOL must be true/false/1/0.
 * VALUE in U32 must be a number (float truncated to u32).
 * Invalid values produce Debug::warning.
 *
 * Multiple signals with the same key are queued (FIFO).
 * On read, the oldest matching entry is removed (get*\has).
 * If TTL (300s) passes without being read, entries are cleaned up.
 *
 * Usage:
 *   auto& ipc = IPCSignals::get();
 *   if (ipc.has("exhausted", "discord.com")) { ... }
 *   if (auto s = ipc.getString("my_text"))   { ... }
 *   if (auto n = ipc.getU32("my_num"))       { ... }
 *   if (auto f = ipc.getFloat("my_float"))   { ... }
 */

class IPCSignals
{
public:
	[[nodiscard]] static IPCSignals& get();

	[[nodiscard]] std::optional<std::string> getString(std::string_view name);
	[[nodiscard]] std::optional<bool>		 getBool(std::string_view name);
	[[nodiscard]] std::optional<float>		 getFloat(std::string_view name);
	[[nodiscard]] std::optional<uint32_t>	 getU32(std::string_view name);
	[[nodiscard]] bool						 has(std::string_view name, std::string_view value);

	void clear(std::string_view name);
	void clearAll();

private:
	struct Entry
	{
		std::vector<std::string>			  values;
		std::chrono::steady_clock::time_point created;
	};

	std::unordered_map<std::string, Entry> _data;
	mutable std::mutex					   _mutex;

	struct Socket
	{
		Socket() = default;
		~Socket()
		{
			if (fd != INVALID_SOCKET)
				closesocket(fd);
		}
		Socket(const Socket&)			 = delete;
		Socket& operator=(const Socket&) = delete;
		SOCKET	fd{ INVALID_SOCKET };
	};

	std::unique_ptr<Socket> _sock;
	std::thread				_listener;
	std::atomic<bool>		_stop{ false };
	std::chrono::seconds	_ttl{ 300 };

	IPCSignals();
	~IPCSignals();
	IPCSignals(const IPCSignals&)			 = delete;
	IPCSignals& operator=(const IPCSignals&) = delete;

	static bool				   _isValidType(std::string_view t);
	static bool				   _isValidName(std::string_view n);
	static bool				   _isValidValue(std::string_view type, std::string_view val);
	void					   _listen();
	void					   _cleanExpired();
	std::optional<std::string> _take(std::string_view name);
};
