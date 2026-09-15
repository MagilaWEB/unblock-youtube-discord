#include "zapret_helper.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <ranges>
#include <vector>

ZapretHelper::~ZapretHelper()
{
	_running = false;
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
	// Curl timeouts live in CurlClient; the merge base keeps current values
	// for keys the CONFIG: payload does not carry. Read them back is not
	// possible, so keep configured copies here is overkill: parsePayload()
	// merges over defaults, then applyConfig() overwrites everything.
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

	CurlClient::configure(norm.check_timeout_sec, norm.connect_timeout_sec, norm.max_redirects);

	if (!_pool.empty() && _pool.size() != _pool_size)
	{
		_stopWorkers();
		_startWorkers(_pool_size);
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

std::string ZapretHelper::_makeLog(std::string_view text)
{
	return std::format("LOG:INFO:helper:{}", text);
}

std::string ZapretHelper::_makeValidSignal(std::string_view host, std::string_view strategy)
{
	return std::format("STRING:helper_valid:{}:{}", host, strategy);
}

std::string ZapretHelper::_makeErrorSignal(std::string_view host, std::string_view strategy)
{
	return std::format("STRING:helper_error:{}:{}", host, strategy);
}

std::string ZapretHelper::_makeDoneSignal(std::string_view host)
{
	return std::format("STRING:helper_done:{}", host);
}

std::string ZapretHelper::_makeCheckingSignal(std::string_view host)
{
	return std::format("STRING:helper_checking:{}", host);
}

std::string ZapretHelper::_makeSeenSignal(std::string_view host)
{
	return std::format("STRING:helper_seen:{}", host);
}

std::string ZapretHelper::_makeOk(std::string_view host)
{
	return std::format("OK:{}", host);
}

std::string ZapretHelper::_makeFail(std::string_view host)
{
	return std::format("FAIL:{}", host);
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
		const auto		rest = message.substr(6);
		const auto		pos	 = rest.find(':');
		_addHost(rest.substr(0, pos));
		_cv.notify_one();	 // single host: one worker is enough
	}
	else if (message.starts_with("VALID:"))
	{
		std::lock_guard lock(_mutex);
		const auto		rest  = message.substr(6);
		const auto		pos	  = rest.find(':');
		const auto		host  = std::string{ rest.substr(0, pos) };
		const auto		strat = (pos != std::string_view::npos) ? rest.substr(pos + 1) : std::string_view{};
		if (_isValidHost(host) && !strat.empty())
		{
			_valid_hosts[host] = std::string{ strat };
			_error_hosts.erase(host);

			if (!_known_hosts.contains(host))
				_known_hosts.insert(host);

			for (auto& [host, strat] : _valid_hosts)
				_send(_makeValidSignal(host, strat), c_ipc_port);
		}
	}
	else if (message.starts_with("ERR:"))
	{
		std::lock_guard lock(_mutex);
		const auto		rest  = message.substr(4);
		const auto		pos	  = rest.find(':');
		const auto		host  = std::string{ rest.substr(0, pos) };
		const auto		strat = (pos != std::string_view::npos) ? rest.substr(pos + 1) : std::string_view{};
		if (_isValidHost(host))
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

		for (auto& [host, info] : _error_hosts)
			_send(_makeErrorSignal(host, info.strategy), c_ipc_port);
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

	if (result)
	{
		_log(std::format("ok {} http={}", host, result.value()));
		_send(_makeOk(host), c_receive_port);
	}
	else
	{
		_log(std::format("fail {} curl={}", host, result.error()));
		_send(_makeFail(host), c_receive_port);
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
		return host;
	}

	return std::nullopt;
}

void ZapretHelper::_workerRoutine()
{
	while (_running)
	{
		std::unique_lock lock(_mutex);
		_cv.wait(lock, [this] { return !_running || !_queue.empty(); });

		if (!_running)
			return;

		auto host = _popHost();
		if (!host)
			continue;

		lock.unlock();

		_send(_makeCheckingSignal(*host), c_ipc_port);
		_checkHost(*host);

		lock.lock();
		_in_check.erase(*host);
		_cv.notify_all();
		lock.unlock();
	}
}

void ZapretHelper::_startWorkers(u32 count)
{
	for (u32 i = 0; i < count; ++i)
		_pool.emplace_back([this] { _workerRoutine(); });
}

void ZapretHelper::_stopWorkers()
{
	_cv.notify_all();

	for (auto& worker : _pool)
		if (worker.joinable())
			worker.join();

	_pool.clear();
}

void ZapretHelper::_stopPool()
{
	_running = false;
	_cv.notify_all();
	_stopWorkers();
}

void ZapretHelper::_idleStep()
{
	const auto now = std::chrono::steady_clock::now();

	bool grew = false;
	bool send_seen = false;
	std::vector<std::string> seen;
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

		// Throttled broadcast: snapshot under lock, send after unlock.
		// The receiver keeps the last list, no need to resend the full
		// set every 100ms loop iteration.
		if ((now - _last_seen_send) >= c_seen_interval)
		{
			seen.assign(_known_hosts.begin(), _known_hosts.end());
			_last_seen_send = now;
			send_seen = true;
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
	}

	// UDP sends and wakeups happen outside the mutex: holding it during
	// hundreds of sendto calls serialized completions on all workers.
	if (send_seen)
		for (const auto& host : seen)
			_send(_makeSeenSignal(host), c_ipc_port);

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

	_startWorkers(_pool_size);

	while (_running)
	{
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
