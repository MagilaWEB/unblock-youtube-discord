#include "zapret_helper.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <ranges>
#include <vector>

ZapretHelper::~ZapretHelper()
{
	_stopPool();
}

bool ZapretHelper::_isValidHost(std::string_view host)
{
	return !host.empty() && std::ranges::any_of(host, [](char ch) { return std::isalpha(static_cast<unsigned char>(ch)); });
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
	_cv.notify_one();
}

void ZapretHelper::_handleMessage(std::string_view message)
{
	if (message.starts_with("LIST:"))
	{
		std::lock_guard lock(_mutex);
		for (const auto part : message.substr(5) | std::views::split(':'))
			_addHost(std::string_view{ part });

		_log(std::format("list added {} hosts", _queue.size()));
	}
	else if (message.starts_with("CHECK:"))
	{
		std::lock_guard lock(_mutex);
		const auto		rest = message.substr(6);
		const auto		pos	 = rest.find(':');
		_addHost(rest.substr(0, pos));
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
			const auto now	= std::chrono::steady_clock::now();
			auto&	   info = _error_hosts[host];
			if (info.first == std::chrono::steady_clock::time_point{})
				info.first = now;

			info.last	  = now;
			info.strategy = std::string{ strat };
			_valid_hosts.erase(host);
			_cv.notify_all();
		}

		for (auto& [host, info] : _error_hosts)
			_send(_makeErrorSignal(host, info.strategy), c_ipc_port);
	}
}

void ZapretHelper::_checkHost(std::string_view host)
{
	_log(std::format("check {}", host));

	const auto result = CurlClient::checkHost(std::string{ host });
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

		std::this_thread::sleep_for(c_sleep_short);
	}
}

void ZapretHelper::_stopPool()
{
	_running = false;
	_cv.notify_all();

	for (auto& worker : _pool)
		if (worker.joinable())
			worker.join();

	_pool.clear();
}

void ZapretHelper::_idleStep()
{
	const auto now = std::chrono::steady_clock::now();

	if ((now - _last_recheck) > c_recheck_interval)
	{
		std::lock_guard lock(_mutex);
		for (const auto& host : _known_hosts)
			if (!_queue.contains(host) && !_in_check.contains(host))
				_queue.insert(host);

		_last_recheck = now;
		_cv.notify_all();
	}

	std::lock_guard lock(_mutex);

	for (const auto& host : _known_hosts)
		_send(_makeSeenSignal(host), c_ipc_port);

	for (auto& [host, info] : _error_hosts)
	{
		if (!_known_hosts.contains(host))
			_known_hosts.insert(host);

		// Host is currently being checked - do not re-enqueue it.
		if (_queue.contains(host) || _in_check.contains(host))
			continue;

		if ((now - info.first) < c_errors_progress_recheck_interval)
		{
			_queue.insert(host);
		}
		else if ((now - info.last) > c_errors_recheck_interval)
		{
			info.last = now;
			_queue.insert(host);
		}
	}
}

int ZapretHelper::run()
{
	if (!_socket.create() || !_socket.bind(c_receive_port) || !_socket.nonBlocking())
		return 1;

	for (u32 i = 0; i < c_pool_size; ++i)
		_pool.emplace_back([this] { _workerRoutine(); });

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
