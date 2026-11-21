module helper.zapret_helper;

import ext.windows;
import common.net_state;
import helper.curl_client;
import helper.config;
import helper.net;
import helper.types;

namespace
{
	// Names of the two zapret engine services (see unblock.zapret1_engine /
	// unblock.zapret2_engine): the helper stops exactly these for a VPN pause
	// and leaves the DNS and Telegram services untouched.
	inline constexpr char c_zapret1_service[]{ "zapret1" };
	inline constexpr char c_zapret2_service[]{ "zapret2" };

	// Longest wait for a stopped zapret service to report SERVICE_STOPPED
	// before the helper gives up (the SCM usually answers in well under).
	inline constexpr auto c_service_stop_wait{ std::chrono::seconds(10) };

	// Throttle for the "couldn't stop" log: a stop that keeps failing must not
	// flood the log every tick.
	inline constexpr auto c_vpn_stop_fail_interval{ std::chrono::seconds(10) };

	// Longest wait for a starting zapret service to reach SERVICE_RUNNING.
	// StartService only queues the start and answers at once, so a payload that
	// dies on launch cannot be told apart from a live one without waiting the
	// state out — the resume used to report success for exactly that case.
	inline constexpr auto c_service_start_wait{ std::chrono::seconds(10) };

	// Throttle for the "couldn't restart" log, same reasoning as the stop one.
	inline constexpr auto c_vpn_start_fail_interval{ std::chrono::seconds(10) };

	// How many ticks the resume keeps retrying a zapret service that will not
	// come up. After that the pause is released so the app's own start path
	// (which recreates the service) can take over: a pause nobody can clear is
	// worse than a logged failure.
	inline constexpr std::uint32_t c_vpn_restore_attempts{ 5 };

	// SCM handles for one call, both closed on scope exit. A null service
	// handle means the manager or the service is absent: querying the state
	// of a service that was never created must be a quiet "not running".
	struct ServiceHandle
	{
		SC_HANDLE manager{ nullptr };
		SC_HANDLE service{ nullptr };

		~ServiceHandle()
		{
			if (service)
				CloseServiceHandle(service);

			if (manager)
				CloseServiceHandle(manager);
		}
	};

	ServiceHandle openService(LPCSTR name)
	{
		ServiceHandle handle;
		handle.manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
		if (!handle.manager)
			return handle;

		handle.service = OpenService(handle.manager, name, SC_MANAGER_ALL_ACCESS);
		return handle;
	}

	DWORD serviceState(SC_HANDLE service)
	{
		SERVICE_STATUS_PROCESS status{};
		DWORD				   needed = 0;

		if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed))
			return SERVICE_STOPPED;

		return status.dwCurrentState;
	}

	bool serviceRunning(LPCSTR name)
	{
		ServiceHandle handle = openService(name);
		if (!handle.service)
			return false;

		const DWORD state = serviceState(handle.service);
		return state == SERVICE_RUNNING || state == SERVICE_START_PENDING;
	}

	// Returns true once the service reports SERVICE_STOPPED (already stopped
	// counts), false when it never does within c_service_stop_wait.
	bool stopService(LPCSTR name)
	{
		ServiceHandle handle = openService(name);
		if (!handle.service)
			return false;

		if (serviceState(handle.service) == SERVICE_STOPPED)
			return true;

		SERVICE_STATUS status{};
		if (!ControlService(handle.service, SERVICE_CONTROL_STOP, &status))
			return false;

		const auto deadline = std::chrono::steady_clock::now() + c_service_stop_wait;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (serviceState(handle.service) == SERVICE_STOPPED)
				return true;

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		return false;
	}

	// Brings the service back up and waits until it really runs. StartService
	// only queues the start: a service whose process dies right after (a stale
	// registration, a payload that refuses to launch) came back as "accepted"
	// and the resume silently left the bypass down. Also re-asserts the start
	// type the app creates its services with — a leftover "disabled" blocks
	// every StartService with 1058 and never heals by itself.
	bool startService(LPCSTR name)
	{
		ServiceHandle handle = openService(name);
		if (!handle.service)
			return false;

		ChangeServiceConfigW(
			handle.service,
			SERVICE_NO_CHANGE,
			SERVICE_AUTO_START,
			SERVICE_NO_CHANGE,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);

		if (!StartService(handle.service, 0, nullptr))
		{
			const DWORD state = serviceState(handle.service);
			if (state != SERVICE_RUNNING && state != SERVICE_START_PENDING)
				return false;
		}

		const auto deadline = std::chrono::steady_clock::now() + c_service_start_wait;
		while (std::chrono::steady_clock::now() < deadline)
		{
			const DWORD state = serviceState(handle.service);
			if (state == SERVICE_RUNNING)
				return true;

			// The process died on launch: waiting longer is pointless.
			if (state == SERVICE_STOPPED)
				return false;

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		return false;
	}

	// Split "left:right" at the first delimiter; right is empty when absent.
	std::pair<std::string_view, std::string_view> splitOnce(std::string_view text, char delimiter)
	{
		const auto pos = text.find(delimiter);
		if (pos == std::string_view::npos)
			return { text, {} };

		return { text.substr(0, pos), text.substr(pos + 1) };
	}
}	 // namespace

ZapretHelper::~ZapretHelper()
{
	{
		// Flip under the mutex: a worker checking the predicate and going
		// to sleep must not slip between the flag change and the notify
		// (missed wakeup -> join hangs forever).
		std::lock_guard lock(_mutex);
		_running = false;
	}
	_cv.notify_all();
	_stopWorkers();
}

HelperConfig ZapretHelper::currentConfig() const
{
	HelperConfig cfg;
	cfg.pool_size			 = _pool_size;
	cfg.recheck_interval_min = static_cast<u32>(_recheck_interval.count());
	cfg.errors_progress_min	 = static_cast<u32>(_errors_progress_interval.count());
	cfg.errors_recheck_sec	 = static_cast<u32>(_errors_recheck_interval.count());

	return cfg;
}

void ZapretHelper::applyConfig(const HelperConfig& cfg)
{
	HelperConfig norm = cfg;
	norm.normalize();

	_pool_size				  = norm.pool_size;
	_recheck_interval		  = std::chrono::minutes{ norm.recheck_interval_min };
	_errors_progress_interval = std::chrono::minutes{ norm.errors_progress_min };
	_errors_recheck_interval  = std::chrono::seconds{ norm.errors_recheck_sec };

	if (!_pool.empty() && _pool.size() != _pool_size)
	{
		_restartWorkers(_pool_size);
		_log(std::format("config applied: pool={}", _pool_size));
	}
}

void ZapretHelper::_handleConfigMessage(std::string_view payload)
{
	// Merge over compiled defaults; every key is carried in the message,
	// so a partial payload cannot resurrect stale values.
	HelperConfig cfg = HelperConfig::parsePayload(payload);
	applyConfig(cfg);
	_log(std::format("config updated: {}", cfg.makeMessage()));
}

bool ZapretHelper::_isValidHost(std::string_view host)
{
	return !host.empty() && std::ranges::any_of(host, [](char ch) { return std::isalpha(static_cast<unsigned char>(ch)); });
}

bool ZapretHelper::_isVoiceMediaHost(std::string_view host)
{
	// Voice endpoints live on *.discord.media subdomains in several shapes
	// (c-<region>-<hash> and bare <region><digits> without the c- prefix).
	// The bare domain is a website and keeps the regular check, so only
	// suffix matches qualify. These hosts are checked by the voice-path
	// probe (WebSocket upgrade), never by the plain curl.
	return host.ends_with(".discord.media");
}

void ZapretHelper::_send(std::string_view message, u32 port) const
{
	std::lock_guard lock(_send_mutex);
	_socket.sendTo(message, _target_ip, port);
}

void ZapretHelper::_log(std::string_view text) const
{
	_send(_makeLog(text), c_ipc_port);
}

void ZapretHelper::_sendSnapshot(std::string_view key, std::string_view payload) const
{
	const std::uint64_t seq	  = _snap_seq.fetch_add(1) + 1;
	const size_t		total = payload.empty() ? 1 : (payload.size() + c_snapshot_chunk - 1) / c_snapshot_chunk;

	for (size_t i = 0; i < total; ++i)
	{
		const size_t begin = i * c_snapshot_chunk;
		const size_t len   = std::min(c_snapshot_chunk, payload.size() - begin);
		_send(_makeSnapshotChunk(key, seq, i, total, payload.substr(begin, len)), c_ipc_port);
	}
}

std::string ZapretHelper::_makeLog(std::string_view text)
{
	return std::format("LOG:INFO:helper:{}", text);
}

std::string ZapretHelper::_makeDoneSignal(std::string_view host)
{
	return std::format("STRING:helper_done:{}", host);
}

std::string ZapretHelper::_makeCheckingSignal(std::string_view host)
{
	return std::format("STRING:helper_checking:{}", host);
}

std::string ZapretHelper::_makeSnapshotChunk(std::string_view key, std::uint64_t seq, size_t idx, size_t total, std::string_view data)
{
	return std::format("SNAP:{}:{}|{}|{}|{}", key, seq, idx, total, data);
}

std::string ZapretHelper::_makeStatsSignal(size_t queued, size_t in_check, size_t known)
{
	return std::format("LATEST:helper_stats:{}|{}|{}", queued, in_check, known);
}

void ZapretHelper::_addHost(std::string_view host)
{
	if (!_isValidHost(host))
		return;

	_known_hosts.insert(std::string{ host });
	_queue.insert(std::string{ host });
	// No notify here: bulk LIST wakes the whole pool once, a single CHECK
	// wakes one worker (see _handleMessage).
}

void ZapretHelper::_handleMessage(std::string_view message)
{
	// Zapret1 has no helper protocol at all, and a VPN pause freezes the pump:
	// in both cases nothing may enqueue work or wake the pool.
	if (_vpn_only || _vpn_paused)
		return;
	if (message.starts_with("LIST:"))
	{
		std::lock_guard lock(_mutex);
		for (const auto part : message.substr(5) | std::views::split(':'))
			_addHost(std::string_view{ part });

		_log(std::format("list added {} hosts", _queue.size()));

		// Bulk insert: wake the whole pool at once. A single notify_one
		// here ramps concurrency up one worker per check completion.
		_cv.notify_all();
	}
	else if (message.starts_with("CHECK:"))
	{
		std::lock_guard lock(_mutex);
		const auto		host = std::string{ splitOnce(message.substr(6), ':').first };
		// A host already being probed cannot be enqueued again (the pump must
		// not drive duplicates). But a CHECK right now means lua changed the
		// strategy: kill the stale probe and re-probe under the new one.
		if (_in_check.contains(host))
			_requestRecheck(host);
		else
			_addHost(host);
		_cv.notify_one();	 // single host: one worker is enough
	}
	else if (message.starts_with("READY:"))
	{
		// Zapret heartbeat. The first beat wakes the dormant helper: workers
		// are spawned here, so no probe can race the strategy's startup.
		// Later beats only refresh the liveness stamp.
		_last_ready = std::chrono::steady_clock::now();
		if (!_zapret_ready.exchange(true))
		{
			if (!_workers_started)
			{
				std::lock_guard lock(_mutex);
				_workers_started = true;
				_startWorkers(_pool_size);
			}
			_log("zapret ready: helper awake");
		}
	}
	else if (message.starts_with("VALID:"))
	{
		std::lock_guard lock(_mutex);
		const auto [host_sv, strat] = splitOnce(message.substr(6), ':');
		const auto host				= std::string{ host_sv };
		if (_isValidHost(host) && !strat.empty())
		{
			_valid_hosts[host] = std::string{ strat };
			_error_hosts.erase(host);
			// Packet-level recovery: lua confirms a working strategy, so a
			// stale fully-tried mark must go.
			_exhausted_hosts.erase(host);
			// A verdict exists now: drop the unjudged/probe marks.
			_unjudged_hosts.erase(host);
			_probed_at.erase(host);

			if (!_known_hosts.contains(host))
				_known_hosts.insert(host);
		}
	}
	else if (message.starts_with("ERR:"))
	{
		std::lock_guard lock(_mutex);
		const auto [host_sv, strat] = splitOnce(message.substr(4), ':');
		const auto host				= std::string{ host_sv };
		// Any ERR is a verdict: drop the unjudged/probe marks even when the
		// host is already terminal (exhausted).
		if (_isValidHost(host))
		{
			_unjudged_hosts.erase(host);
			_probed_at.erase(host);
		}
		// A fully-tried host stays terminal until VALID; a later packet-level
		// ERR must not drag it back into the error bucket (that refusal is
		// what let autopick never settle).
		if (_isValidHost(host) && !_exhausted_hosts.contains(host))
		{
			// Duplicate ERR while the host is already tracked: refresh only
			// the strategy name. first/last timestamps are owned by the first
			// report so packet spam on port 10000 cannot postpone the
			// scheduled recheck in _idleStep.
			if (const auto it = _error_hosts.find(host); it != _error_hosts.end())
			{
				it->second.strategy = std::string{ strat };
			}
			else
			{
				const auto now = std::chrono::steady_clock::now();
				ErrorInfo  info;
				info.first	  = now;
				info.last	  = now;
				info.strategy = std::string{ strat };
				_error_hosts.emplace(host, std::move(info));
				_valid_hosts.erase(host);
				_cv.notify_all();
			}
		}

		// lua switched strategy for this host: the probe that is running now
		// tests the strategy that was just rejected, so let it die and re-probe
		// immediately instead of holding a worker to the curl timeout.
		if (_isValidHost(host) && _in_check.contains(host))
			_requestRecheck(host);
	}
	else if (message.starts_with("EXHAUSTED:"))
	{
		std::lock_guard lock(_mutex);
		const auto [host_sv, strat] = splitOnce(message.substr(10), ':');
		const auto host				= std::string{ host_sv };
		if (_isValidHost(host) && !strat.empty())
		{
			// First report owns the timestamps (packet spam must not
			// postpone anything); duplicates refresh the strategy name.
			// A fully-tried host invalidates any earlier valid mark.
			if (const auto it = _exhausted_hosts.find(host); it != _exhausted_hosts.end())
			{
				it->second.strategy = std::string{ strat };
			}
			else
			{
				const auto now = std::chrono::steady_clock::now();
				ErrorInfo  info;
				info.first	  = now;
				info.last	  = now;
				info.strategy = std::string{ strat };
				_exhausted_hosts.emplace(host, std::move(info));
				_valid_hosts.erase(host);
			}

			// The fully-tried mark supersedes an error mark: the host now
			// lives in exactly one terminal bucket and is rechecked on the
			// stale-error interval instead of the aggressive error loop.
			_error_hosts.erase(host);
			// A verdict exists now: drop the unjudged/probe marks.
			_unjudged_hosts.erase(host);
			_probed_at.erase(host);

			if (!_known_hosts.contains(host))
				_known_hosts.insert(host);
		}
	}
	else if (message.starts_with("CONFIG:"))
	{
		// Fresh settings pushed by Unblock (the on-disk file is stale while
		// unblock is running). Pool resize happens inside applyConfig().
		_handleConfigMessage(message.substr(7));
	}
	else if (message.starts_with("RELOAD:"))
	{
		applyConfig(HelperConfig::load());
		_log("config reloaded from file");
	}
}

void ZapretHelper::_cancelProbes()
{
	// Caller holds _mutex. Unlike a recheck, the host is not re-queued: the
	// resume re-queues the whole known set anyway, and while the tunnel is up
	// the queue must stay empty.
	for (auto& entry : _cancel)
		entry.second->store(true);
}

void ZapretHelper::_requestRecheck(const std::string& host)
{
	// Caller holds _mutex. The probe polls its own flag without the lock, so
	// storing through the shared_ptr is what the worker thread actually sees.
	if (const auto it = _cancel.find(host); it != _cancel.end())
		it->second->store(true);

	_pending.insert(host);
}

void ZapretHelper::_endCheck(const std::string& host)
{
	// Caller holds _mutex.
	_cancel.erase(host);
	_in_check.erase(host);

	// A recheck requested while the probe ran must land in the queue now that
	// the worker is free again; that is the whole point of aborting the probe.
	if (_pending.erase(host))
		_queue.insert(host);
}

void ZapretHelper::_checkHost(std::string_view host, const CurlAbort& abort)
{
	const bool voice = _isVoiceMediaHost(host);

	_log(std::format("{} {}", voice ? "voice check" : "check", host));

	const auto result = voice ? CurlClient::checkVoiceHost(std::string{ host }, abort) : CurlClient::checkHost(std::string{ host }, abort);
	_send(_makeDoneSignal(host), c_ipc_port);

	// A cancelled probe yields no verdict: the abort means its traffic is not
	// evidence (a VPN pause ran it through the tunnel, a recheck superseded
	// it). The done signal above still went out, so the "checking" mark clears.
	if (abort())
	{
		_log(std::format("cancelled {}", host));
		return;
	}

	// Stamp the probe. If lua never delivers a verdict for this host, the
	// stale stamp promotes it into the unjudged bucket in _refreshUnjudged.
	{
		std::lock_guard lock(_mutex);
		_probed_at[std::string{ host }] = std::chrono::steady_clock::now();
	}

	if (result)
	{
		// Recovery is confirmed by lua's VALID, not by the probe itself: the
		// fully-tried mark is cleared only when the host lands in the valid
		// list, so the dead set stays terminal until then. The probe only
		// pumps traffic; lua raises that VALID on the next confirmation.
		_log(std::format("ok {} http={}", host, result.value()));
	}
	else
	{
		// Resolver died before the first packet (pulled cable, dead DNS):
		// no strategy can fix it and lua will never see the host (no
		// traffic exists to strategize), so mark it fully-tried right
		// away instead of waiting for a lua wrap that never comes.
		if (CurlClient::isTerminalError(result.error()) && _isValidHost(host))
		{
			std::lock_guard lock(_mutex);
			const auto		now = std::chrono::steady_clock::now();
			if (const auto it = _exhausted_hosts.find(std::string{ host }); it != _exhausted_hosts.end())
			{
				it->second.last = now;
			}
			else
			{
				ErrorInfo info;
				info.first	  = now;
				info.last	  = now;
				info.strategy = "dns";
				_exhausted_hosts.emplace(std::string{ host }, std::move(info));
				_valid_hosts.erase(std::string{ host });
			}

			if (!_known_hosts.contains(std::string{ host }))
				_known_hosts.emplace(host);

			// Fast-path snapshot so unblock can fast-fail the host without
			// waiting for the next 500ms tick.
			_sendSnapshot(
				"helper_exhausted",
				_exhausted_hosts | std::views::transform([](const auto& kv) { return kv.first + "=" + kv.second.strategy; })
					| std::views::join_with('\n') | std::ranges::to<std::string>()
			);

			_log(std::format("dns-dead {} (terminal)", host));
		}
		else
		{
			// A failed voice probe is a verdict, not a pump hiccup: the
			// WebSocket exchange hits the same appdata path the app needs, so
			// the stale VALID no longer holds. Demote the host while the
			// regular curl probes stay purely traffic pumps.
			if (voice)
				_demoteVoiceOnProbeFailure(host);

			_log(std::format("fail {} curl={}", host, result.error()));
		}
	}
}

std::optional<std::string> ZapretHelper::_popHost()
{
	if (!_queue.empty())
	{
		auto		it	 = _queue.begin();
		std::string host = *it;
		_queue.erase(it);
		_in_check.insert(host);

		for (auto& h : _in_check)
			_send(_makeCheckingSignal(h), c_ipc_port);

		return host;
	}

	return std::nullopt;
}

void ZapretHelper::_workerRoutine(u32 epoch)
{
	while (_running)
	{
		std::unique_lock lock(_mutex);
		// A VPN pause stops the pump itself, not just the enqueues: the engines
		// are down, no verdict can come, and probing hosts whose traffic goes
		// through the tunnel only burns the pool. The resume re-queues the whole
		// known set and wakes the pool.
		_cv.wait(lock, [this, epoch] { return !_running || epoch != _pool_epoch.load() || (!_queue.empty() && !_vpn_paused); });

		if (!_running || epoch != _pool_epoch.load())
			return;

		auto host = _popHost();
		if (!host)
			continue;

		// The probe's abort flag is bound to this host before the lock drops:
		// a CHECK/ERR arriving for it from now on sets the flag, and the
		// running probe dies at the next libcurl callback.
		const auto cancel = std::make_shared<std::atomic<bool>>(false);
		_cancel[*host]	  = cancel;

		lock.unlock();

		_checkHost(*host, [cancel] { return cancel->load(); });

		lock.lock();
		_endCheck(*host);
		_cv.notify_all();
		lock.unlock();
	}
}

void ZapretHelper::_startWorkers(u32 count)
{
	// Capture the epoch here, under the caller's context: threads that
	// actually start running after a restart bump must still carry the
	// generation they were created in, otherwise they adopt the new epoch,
	// wait on an empty queue forever and join hangs.
	const u32 epoch = _pool_epoch.load();
	for (u32 i = 0; i < count; ++i)
		_pool.emplace_back([this, epoch] { _workerRoutine(epoch); });
}

void ZapretHelper::_stopWorkers()
{
	_cv.notify_all();

	for (auto& worker : _pool)
		if (worker.joinable())
			worker.join();

	_pool.clear();
}

void ZapretHelper::_restartWorkers(u32 count)
{
	// Retire the current generation: waiting workers see the epoch bump
	// and exit, workers mid-check exit at the loop top. Join waits at most
	// one check duration (up to ~14s for a voice probe), then the new
	// generation starts. Joining without the epoch bump deadlocked: the old
	// workers wait on !_running, which stays true while live.
	// The bump itself is under the mutex: bumping outside opened a
	// check-then-block window where the notify is missed and join hangs.
	{
		std::lock_guard lock(_mutex);
		_pool_epoch.fetch_add(1);
	}
	_cv.notify_all();

	for (auto& worker : _pool)
		if (worker.joinable())
			worker.join();

	_pool.clear();
	_startWorkers(count);
}

void ZapretHelper::_stopPool()
{
	{
		// Same missed-wakeup rule as in the destructor: flag under mutex.
		std::lock_guard lock(_mutex);
		_running = false;
	}
	_cv.notify_all();
	_stopWorkers();
}

void ZapretHelper::_checkZapretLiveness()
{
	if (!_zapret_ready.load())
		return;

	if (std::chrono::steady_clock::now() - _last_ready <= c_zapret_timeout)
		return;

	// Zapret went quiet: stop feeding the queue. Workers stay alive and idle
	// (a join here could block the loop for a whole probe), so the next READY
	// simply resumes work.
	_zapret_ready = false;
	{
		std::lock_guard lock(_mutex);
		_queue.clear();
	}
	_log("zapret heartbeat lost: host work suspended");
}

void ZapretHelper::_checkVpnPause()
{
	const auto now = std::chrono::steady_clock::now();
	if (now - _last_vpn_check < c_vpn_check_interval)
		return;

	_last_vpn_check = now;

	std::string which;
	const bool	active = net::vpnTunnelActive(which);

	// The pause indicator names the tunnel in the UI, so the name goes out next
	// to the flag. Both are re-asserted while the pause lasts, and the latest
	// name lives in _vpn_name: during a retry the tunnel is already gone and
	// `which` is empty, yet the indicator still has to say whose pause it is.
	auto announcePause = [&]
	{
		_send("LATEST:zapret.vpn_paused:1", c_ipc_port);
		_send("LATEST:zapret.vpn_name:" + _vpn_name, c_ipc_port);
	};

	if (active)
	{
		// The flag tracks the transition in both directions: leaving it set
		// after the tunnel went away made every later detection silent, so a
		// second pause looked like the helper had never seen the VPN at all.
		if (active != _vpn_seen_detected)
		{
			// Evidence in the app log that the helper saw the tunnel at all:
			// without this line the pause never even started.
			_log(std::format("VPN tunnel detected [{}]", which));
		}

		_vpn_seen_detected = true;
		_vpn_name		   = which;

		if (!_vpn_paused)
		{
			// Capture the engines that are live BEFORE stopping anything: only
			// those are ours to bring back. A zapret the user kept off must
			// stay off (the DNS/Telegram services are never touched at all).
			_vpn_zapret1_was_running = serviceRunning(c_zapret1_service);
			_vpn_zapret2_was_running = serviceRunning(c_zapret2_service);
			_vpn_zapret1_down		 = false;
			_vpn_zapret2_down		 = false;
			_vpn_restore_attempts	 = 0;

			// Announce the pause BEFORE stopping anything: the receiver shows
			// "engine stopped" the moment an engine goes down, and stopping can
			// take seconds, so a late "1" would leave a window where the app
			// reports a stopped engine with no VPN explanation.
			_vpn_paused = true;
			announcePause();

			// Freeze the pump at once: the verdicts are meaningless under a
			// tunnel, and waiting for the heartbeat timeout would keep feeding
			// the queue for a whole minute. In-flight probes are cancelled, not
			// joined (a join here would block the loop): the abort flag is set,
			// and _checkHost drops the verdict of a cancelled probe - traffic
			// that went through the tunnel is not evidence.
			_zapret_ready = false;
			{
				std::lock_guard lock(_mutex);
				_queue.clear();
				_cancelProbes();
			}
		}

		// Held down on every tick, not once: the stop can fail while the service
		// is still shutting down, and the engine can come back (the app restarts
		// it with the tunnel up). A single attempt left traffic going through a
		// bypass the VPN makes pointless.
		// Any engine that is running while the tunnel is up gets stopped; the
		// captured set only decides what the resume brings back. The helper is
		// started together with the engine, so its first check races the
		// engine's own start: without this the pause recorded "nothing was
		// running" and the engine kept desyncing traffic behind the VPN.
		if (serviceRunning(c_zapret1_service))
		{
			if (!_vpn_zapret1_was_running)
			{
				_vpn_zapret1_was_running = true;
				_log("zapret1 started while the VPN is up: stopping it (and restoring it later)");
			}
			else if (_vpn_zapret1_down)
				_log("zapret1 came back while the VPN is up: stopping it again");

			if (stopService(c_zapret1_service))
				_vpn_zapret1_down = true;
			else if (now - _last_vpn_stop_fail > c_vpn_stop_fail_interval)
			{
				_last_vpn_stop_fail = now;
				_log("couldn't stop the zapret1 service for the VPN pause");
			}
		}

		if (serviceRunning(c_zapret2_service))
		{
			if (!_vpn_zapret2_was_running)
			{
				_vpn_zapret2_was_running = true;
				_log("zapret2 started while the VPN is up: stopping it (and restoring it later)");
			}
			else if (_vpn_zapret2_down)
				_log("zapret2 came back while the VPN is up: stopping it again");

			if (stopService(c_zapret2_service))
				_vpn_zapret2_down = true;
			else if (now - _last_vpn_stop_fail > c_vpn_stop_fail_interval)
			{
				_last_vpn_stop_fail = now;
				_log("couldn't stop the zapret2 service for the VPN pause");
			}
		}

		// Re-asserted every tick, not only on the transition: the receiver
		// drops a LATEST value after a few minutes, so a long pause must be
		// kept alive or the UI would fall back to "engine stopped".
		announcePause();
		return;
	}

	if (_vpn_paused)
	{
		// Resume only what the pause captured, and only once it is really up: a
		// restart that failed must be retried on the next tick instead of being
		// forgotten, otherwise the bypass stays down with a "restored" line in
		// the log. Driven by the captured set, so the flags clear one by one.
		bool retry = false;

		if (_vpn_zapret1_was_running)
		{
			if (startService(c_zapret1_service))
				_vpn_zapret1_was_running = false;
			else
			{
				retry = true;

				if (now - _last_vpn_start_fail > c_vpn_start_fail_interval)
				{
					_last_vpn_start_fail = now;
					_log("couldn't restart the zapret1 service after the VPN pause");
				}
			}
		}

		if (_vpn_zapret2_was_running)
		{
			if (startService(c_zapret2_service))
				_vpn_zapret2_was_running = false;
			else
			{
				retry = true;

				if (now - _last_vpn_start_fail > c_vpn_start_fail_interval)
				{
					_last_vpn_start_fail = now;
					_log("couldn't restart the zapret2 service after the VPN pause");
				}
			}
		}

		if (retry)
		{
			// Keep announcing the pause while the retries last: reporting a
			// plain stop here would hide an engine the helper is still bringing
			// back.
			if (++_vpn_restore_attempts < c_vpn_restore_attempts)
			{
				announcePause();
				return;
			}

			_log("gave up restarting the zapret service after the VPN pause");
		}

		_vpn_restore_attempts	 = 0;
		_vpn_zapret1_was_running = false;
		_vpn_zapret2_was_running = false;
		_vpn_zapret1_down		 = false;
		_vpn_zapret2_down		 = false;
		_vpn_paused				 = false;

		// Everything the helper learned is meaningless behind a tunnel: drop the
		// verdicts so the hosts are re-probed once the engines are back.
		std::size_t queued = 0;
		{
			std::lock_guard lock(_mutex);
			_error_hosts.clear();
			_valid_hosts.clear();
			_exhausted_hosts.clear();
			_unjudged_hosts.clear();
			_probed_at.clear();

			// Pump the whole known set right away, not at the next recheck (ten
			// minutes by default): the engines were restarted with no memory of
			// the tunnel, and until the hosts are probed they have no verdicts
			// at all. The known set is exactly the traffic that went through the
			// tunnel - the app's LIST plus every host lua asked about in live
			// traffic.
			for (const auto& host : _known_hosts)
				if (!_in_check.contains(host))
					_queue.insert(host);

			queued = _queue.size();
		}

		// The verdicts are gone, so the receivers must learn it now: the next
		// periodic snapshot would leave a dead-host list standing for another
		// second, and every host in it looks dead until it is probed again.
		_sendSnapshot("helper_exhausted", "");
		_sendSnapshot("helper_unjudged", "");
		_sendSnapshot("helper_error", "");
		_sendSnapshot("helper_valid", "");
		_cv.notify_all();

		_log(std::format("VPN tunnel gone: zapret services restored, {} hosts queued", queued));
	}

	// Kept alive while unpaused too: a stale "1" from a previous helper
	// incarnation (killed right after pausing) would otherwise show a ghost
	// pause until the receiver's TTL expiry.
	_vpn_seen_detected = false;
	_send("LATEST:zapret.vpn_paused:0", c_ipc_port);
}

void ZapretHelper::_refreshUnjudged()
{
	// Caller holds _mutex. A probe that finished but produced no verdict at all
	// (no VALID/ERR/EXHAUSTED and no error entry from an earlier verdict) is
	// promoted into the unjudged bucket once the stale interval passed.
	// Sticky: only a verdict removes it.
	//
	// An error entry is a real lua verdict (ERR, or a failed voice probe), so
	// it owns the host even when a later probe yields no new verdict: the error
	// list keeps the last judged strategy name, and only a fresh verdict
	// (VALID/EXHAUSTED/another ERR) moves the host. Demoting an error here
	// labelled a host that lua had just failed as "no-verdict", which is the
	// exact contradiction the lists must not show.
	const auto now = std::chrono::steady_clock::now();
	for (const auto& [host, probed] : _probed_at)
	{
		if (_valid_hosts.contains(host) || _exhausted_hosts.contains(host) || _error_hosts.contains(host))
			continue;

		if (_unjudged_hosts.contains(host))
			continue;

		if (_queue.contains(host) || _in_check.contains(host))
			continue;

		if ((now - probed) < _errors_recheck_interval)
			continue;

		ErrorInfo info;
		info.first	  = now;
		info.last	  = now;
		info.strategy = "no-verdict";
		_unjudged_hosts.emplace(host, std::move(info));
	}
}

void ZapretHelper::_demoteVoiceOnProbeFailure(std::string_view host)
{
	std::lock_guard lock(_mutex);

	const std::string key{ host };
	const auto		  it = _valid_hosts.find(key);
	if (it == _valid_hosts.end())
		return;

	const std::string strategy = it->second;
	const auto		  now	   = std::chrono::steady_clock::now();

	if (const auto err = _error_hosts.find(key); err != _error_hosts.end())
		err->second.strategy = strategy;
	else
	{
		ErrorInfo info;
		info.first	  = now;
		info.last	  = now;
		info.strategy = strategy;
		_error_hosts.emplace(key, std::move(info));
	}

	_valid_hosts.erase(it);
	_unjudged_hosts.erase(key);
	_probed_at.erase(key);
}

void ZapretHelper::_idleStep()
{
	const auto now = std::chrono::steady_clock::now();

	bool					 grew		   = false;
	bool					 send_snapshot = false;
	std::vector<std::string> seen;
	std::vector<std::string> valid_lines, error_lines, exhausted_lines, unjudged_lines;
	size_t					 stat_queued = 0, stat_in_check = 0, stat_known = 0;
	{
		std::lock_guard lock(_mutex);

		if ((now - _last_recheck) > _recheck_interval)
		{
			for (const auto& host : _known_hosts)
				if (!_queue.contains(host) && !_in_check.contains(host))
				{
					_queue.insert(host);
					grew = true;
				}

			_last_recheck = now;
		}

		_refreshUnjudged();

		// Throttled broadcast: snapshot under lock, send after unlock.
		// The receiver keeps the last list, no need to resend the full
		// set every 100ms loop iteration.
		if ((now - _last_seen_send) >= c_seen_interval)
		{
			seen.assign(_known_hosts.begin(), _known_hosts.end());
			valid_lines		= _valid_hosts | std::views::transform([](const auto& kv) { return kv.first + "=" + kv.second; })
							| std::ranges::to<std::vector<std::string>>();
			error_lines		= _error_hosts | std::views::transform([](const auto& kv) { return kv.first + "=" + kv.second.strategy; })
							| std::ranges::to<std::vector<std::string>>();
			exhausted_lines = _exhausted_hosts | std::views::transform([](const auto& kv) { return kv.first + "=" + kv.second.strategy; })
							| std::ranges::to<std::vector<std::string>>();
			unjudged_lines	= _unjudged_hosts | std::views::transform([](const auto& kv) { return kv.first + "=" + kv.second.strategy; })
							| std::ranges::to<std::vector<std::string>>();
			stat_queued		= _queue.size();
			stat_in_check	= _in_check.size();
			stat_known		= _known_hosts.size();
			_last_seen_send = now;
			send_snapshot	= true;
		}

		for (auto& [host, info] : _error_hosts)
		{
			if (!_known_hosts.contains(host))
				_known_hosts.insert(host);

			// Host is currently being checked - do not re-enqueue it.
			if (_queue.contains(host) || _in_check.contains(host))
				continue;

			if ((now - info.first) < _errors_progress_interval)
			{
				_queue.insert(host);
				grew = true;
			}
			else if ((now - info.last) > _errors_recheck_interval)
			{
				info.last = now;
				_queue.insert(host);
				grew = true;
			}
		}

		// Dead hosts are rechecked on the same slow cadence as stale errors:
		// recovery (curl OK -> lua VALID) is detected while the host never
		// drops out of the exhausted bucket on its own.
		for (auto& [host, info] : _exhausted_hosts)
		{
			if (_queue.contains(host) || _in_check.contains(host))
				continue;

			if ((now - info.last) > _errors_recheck_interval)
			{
				info.last = now;
				_queue.insert(host);
				grew = true;
			}
		}

		// Unjudged hosts are re-probed on the same slow cadence: a new probe
		// may finally produce a verdict, and the mark is dropped by that
		// verdict (VALID/ERR/EXHAUSTED), never by the recheck itself.
		for (auto& [host, info] : _unjudged_hosts)
		{
			if (_queue.contains(host) || _in_check.contains(host))
				continue;

			if ((now - info.last) > _errors_recheck_interval)
			{
				info.last = now;
				_queue.insert(host);
				grew = true;
			}
		}
	}

	auto join_lines = [](const std::vector<std::string>& lines)
	{
		std::string out;
		for (const auto& line : lines)
		{
			if (!out.empty())
				out += '\n';
			out += line;
		}
		return out;
	};

	// UDP sends and wakeups happen outside the mutex: holding it during
	// hundreds of sendto calls serialized completions on all workers.
	if (send_snapshot)
	{
		_sendSnapshot("helper_seen", join_lines(seen));
		// Pool load snapshot (self-healing state, not edges): the checking
		// set is an instant sample and reads ~0 under millisecond checks,
		// while workers are actually buried in queue.
		_send(_makeStatsSignal(stat_queued, stat_in_check, stat_known), c_ipc_port);
		_sendSnapshot("helper_valid", join_lines(valid_lines));
		_sendSnapshot("helper_error", join_lines(error_lines));
		_sendSnapshot("helper_exhausted", join_lines(exhausted_lines));
		_sendSnapshot("helper_unjudged", join_lines(unjudged_lines));
	}

	// Wake workers only when new work actually arrived. An unconditional
	// notify_all here was a thundering herd (20 wakeups x 10Hz for nothing),
	// while the error branch above used to queue with no notify at all,
	// leaving workers asleep on a full queue.
	if (grew)
		_cv.notify_all();
}

void ZapretHelper::setVpnOnly(bool value)
{
	_vpn_only = value;
}

int ZapretHelper::run()
{
	// Cold start / PC reboot fallback: the file was flushed on unblock close,
	// so it holds the last applied values. Fresh values arrive via UDP
	// CONFIG: right after Unblock launches the helper (the on-disk file is
	// stale while unblock is running).
	applyConfig(HelperConfig::load());

	if (!_socket.create() || !_socket.bind(c_receive_port) || !_socket.nonBlocking())
		return 1;

	// Loopback storms of tiny datagrams (ERR spam, rebroadcasts) overflowed
	// the default kernel buffers and verdicts never reached unblock.
	_socket.setBufferSize(1 << 20);

	// Workers are NOT started here. They are spawned once on the first READY
	// (zapret heartbeat), so no probe can race the strategy startup. Starting
	// them here as well spawned a second pool (2x pool_size).
	if (_vpn_only)
		_log("vpn-only mode: watching the VPN tunnel, host pumping disabled");

	while (_running)
	{
		if (!_vpn_only)
			_checkZapretLiveness();

		_checkVpnPause();

		if (_zapret_ready.load())
			_idleStep();

		sockaddr_in from{};
		const int	n = _socket.recvFrom(_buffer.data(), static_cast<int>(_buffer.size()) - 1, from);
		if (n <= 0)
		{
			std::this_thread::sleep_for(c_sleep_short);
			continue;
		}

		_handleMessage(std::string_view{ _buffer.data(), static_cast<size_t>(n) });
	}

	_running = false;
	_cv.notify_all();

	return 0;
}
