#include "zapret_helper.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <ranges>
#include <vector>

namespace
{
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
	const uint64_t seq	 = _snap_seq.fetch_add(1) + 1;
	const size_t   total = payload.empty() ? 1 : (payload.size() + c_snapshot_chunk - 1) / c_snapshot_chunk;

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

std::string ZapretHelper::_makeSnapshotChunk(std::string_view key, uint64_t seq, size_t idx, size_t total, std::string_view data)
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
		const auto		host = splitOnce(message.substr(6), ':').first;
		// Do not re-enqueue a host already being checked: the pump must not
		// drive duplicates.
		if (!_in_check.contains(std::string{ host }))
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

void ZapretHelper::_checkHost(std::string_view host)
{
	const bool voice = _isVoiceMediaHost(host);

	_log(std::format("{} {}", voice ? "voice check" : "check", host));

	const auto result = voice ? CurlClient::checkVoiceHost(std::string{ host }) : CurlClient::checkHost(std::string{ host });
	_send(_makeDoneSignal(host), c_ipc_port);

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
		_cv.wait(lock, [this, epoch] { return !_running || epoch != _pool_epoch.load() || !_queue.empty(); });

		if (!_running || epoch != _pool_epoch.load())
			return;

		auto host = _popHost();
		if (!host)
			continue;

		lock.unlock();

		_checkHost(*host);

		lock.lock();
		_in_check.erase(*host);
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

void ZapretHelper::_refreshUnjudged()
{
	// Caller holds _mutex. A probe that finished but never got a lua verdict
	// (no VALID/ERR/EXHAUSTED) is promoted into the unjudged bucket once the
	// stale-error interval passed. Sticky: it is only removed by a verdict.
	const auto now = std::chrono::steady_clock::now();
	for (const auto& [host, probed] : _probed_at)
	{
		if (_valid_hosts.contains(host) || _error_hosts.contains(host) || _exhausted_hosts.contains(host))
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

	while (_running)
	{
		_checkZapretLiveness();

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
