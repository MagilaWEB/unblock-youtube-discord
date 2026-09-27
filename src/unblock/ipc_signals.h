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
 * Latest-only signal (telemetry, not an event):
 *   LATEST:name:value → getLatest("name")   keeps only the newest value,
 *                       read does NOT consume it (peek). Use for counters /
 *                       state snapshots that a reader polls; a FIFO would
 *                       grow unbounded because the TTL refreshes on push.
 *
 * Chunked snapshot (large sets, self-healing, not an event):
 *   SNAP:name:seq|idx|total|data → getLatest("name")  assembles all chunks
 *                       of the newest seq into one payload; read does NOT
 *                       consume. A newer seq discards an incomplete older
 *                       one, so a lost chunk is healed on the next tick.
 *                       Use for host lists too big for a single datagram.
 *
 * NAME allows: letters, digits, '.', '-', '_', '/', ':', '@'
 * VALUE in LOG is any text.
 * VALUE in BOOL must be true/false/1/0.
 * VALUE in U32 must be a number (float truncated to u32).
 * VALUE in LATEST/SNAP is any text.
 * Invalid values produce Debug::warning.
 *
 * Multiple signals with the same key are queued (FIFO).
 * On read, the oldest matching entry is removed (get*\has).
 * If TTL (300s) passes without being read, entries are cleaned up.
 *
 * Usage:
 *   auto& ipc = IPCSignals::get();
 *   if (auto s = ipc.getString("my_event"))  { ... }   // FIFO event
 *   if (auto n = ipc.getLatestU32("my_num")) { ... }   // telemetry peek
 *   if (ipc.snapshotContains("hosts", "a.com")) { ... }// snapshot lookup
 */

class IPCSignals
{
public:
	[[nodiscard]] static IPCSignals&								 get();
	[[nodiscard]] std::optional<std::string>						 getString(std::string_view name);
	[[nodiscard]] std::optional<bool>								 getBool(std::string_view name);
	[[nodiscard]] std::optional<float>								 getFloat(std::string_view name);
	[[nodiscard]] std::optional<uint32_t>							 getU32(std::string_view name);
	[[nodiscard]] bool												 has(std::string_view name, std::string_view value);
	/** Newest LATEST/SNAP value without removing it. */
	[[nodiscard]] std::optional<std::string>						 getLatest(std::string_view name);
	[[nodiscard]] std::optional<uint32_t>							 getLatestU32(std::string_view name);
	/** Age of the newest LATEST/SNAP value; nullopt when the key is unknown. */
	[[nodiscard]] std::optional<std::chrono::steady_clock::duration> latestAge(std::string_view name);
	/** True when the newest snapshot payload contains the token as a whole
	 *  line or a "token=..." entry (payload lines are '\n'-separated). */
	[[nodiscard]] bool												 snapshotContains(std::string_view name, std::string_view token);
	void															 clear(std::string_view name);
	void															 clearAll();

	IPCSignals(const IPCSignals&)			 = delete;
	IPCSignals& operator=(const IPCSignals&) = delete;

private:
	IPCSignals();
	~IPCSignals();

private:
	struct Entry
	{
		std::vector<std::string>			  values;
		std::chrono::steady_clock::time_point created;
	};

	struct LatestEntry
	{
		std::string							  value;
		std::chrono::steady_clock::time_point created;
	};

	// In-progress chunked snapshot per key. A chunk of a newer seq replaces
	// the whole building state, so a lost chunk is healed next tick.
	struct SnapBuilding
	{
		uint64_t								  seq{ 0 };
		uint32_t								  total{ 0 };
		std::unordered_map<uint32_t, std::string> chunks;
	};

	std::unordered_map<std::string, Entry>		  _data;
	std::unordered_map<std::string, LatestEntry>  _latest;
	std::unordered_map<std::string, SnapBuilding> _snap_building;
	mutable std::mutex							  _mutex;

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

	static bool				   _isValidType(std::string_view t);
	static bool				   _isValidName(std::string_view n);
	static bool				   _isValidValue(std::string_view type, std::string_view val);
	void					   _listen();
	void					   _cleanExpired();
	void					   _handleSnap(std::string_view key, std::string_view val);
	std::optional<std::string> _take(std::string_view name);
};
