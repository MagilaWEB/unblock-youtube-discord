#include "zapret_helper.h"

#include <cctype>
#include <execution>
#include <format>
#include <ranges>
#include <vector>

ZapretHelper::~ZapretHelper()
{
	_running = false;
	if (_worker.joinable())
		_worker.join();
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
	_send(std::format("LOG:INFO:helper:{}", text), c_ipc_port);
}

void ZapretHelper::_addHost(std::string_view host)
{
	if (!_isValidHost(host))
		return;

	_known_hosts.insert(std::string{ host });
	if (std::ranges::find(_queue, host) == _queue.end())
		_queue.emplace_back(host);
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
		_addHost(message.substr(6));
	}
	else if (message.starts_with("STAT:"))
	{
		std::lock_guard lock(_mutex);
		const auto		rest  = message.substr(5);
		const auto		pos	  = rest.find(':');
		const auto		host  = rest.substr(0, pos);
		const auto		strat = (pos != std::string_view::npos) ? rest.substr(pos + 1) : std::string_view{};
		if (_isValidHost(host) && !strat.empty())
		{
			_valid[std::string{ host }] = std::string{ strat };
			_send(std::format("STRING:helper_valid:{}:{}", host, strat), c_ipc_port);

			if (_error_hosts.erase(std::string{ host }))
				_send(std::format("STRING:helper_error_clear:{}", host), c_ipc_port);
		}
	}
	else if (message.starts_with("ERR:"))
	{
		std::lock_guard lock(_mutex);
		const auto		rest  = message.substr(4);
		const auto		pos	  = rest.find(':');
		const auto		host  = rest.substr(0, pos);
		const auto		strat = (pos != std::string_view::npos) ? rest.substr(pos + 1) : std::string_view{};
		if (_isValidHost(host))
		{
			_error_hosts[std::string{ host }] = std::string{ strat };
			_send(std::format("STRING:helper_error:{}:{}", host, strat), c_ipc_port);
		}
	}
}

void ZapretHelper::_checkHost(std::string_view host) const
{
	_log(std::format("check {}", host));
	//_send(std::format("CHECK:{}", host), c_receive_port);

	const auto result = CurlClient::checkHost(std::string{ host });
	_send(std::format("STRING:helper_done:{}", host), c_ipc_port);

	if (result)
	{
		_log(std::format("ok {} http={}", host, result.value()));
		_send(std::format("OK:{}", host), c_receive_port);
	}
	else
	{
		_log(std::format("fail {} curl={}", host, result.error()));
		_send(std::format("FAIL:{}", host), c_receive_port);
	}
}

void ZapretHelper::_workerLoop()
{
	using namespace std::chrono;

	while (_running)
	{
		if (_queue.empty())
		{
			const auto now = steady_clock::now();
			if (now - _last_recheck > c_recheck_interval)
			{
				std::lock_guard lock(_mutex);
				for (auto& [host, strategy] : _error_hosts)
					if (!_known_hosts.contains(host))
						_known_hosts.insert(host);

				for (const auto& host : _known_hosts)
					if (std::ranges::find(_queue, host) == _queue.end())
						_queue.push_back(host);

				_last_recheck = now;
			}
			else
				for (const auto& host : _known_hosts)
					_send(std::format("STRING:helper_seen:{}", host), c_ipc_port);

			std::this_thread::sleep_for(c_sleep_short);
			continue;
		}

		for (auto& host : _queue)
			_send(std::format("STRING:helper_checking:{}", host), c_ipc_port);

		std::for_each(std::execution::par, _queue.begin(), _queue.end(), [this](std::string& host) { _checkHost(host); });

		_queue.clear();
	}
}

int ZapretHelper::run()
{
	if (!_socket.create() || !_socket.bind(c_receive_port) || !_socket.nonBlocking())
		return 1;

	_worker = std::thread{ [this] { _workerLoop(); } };

	while (_running)
	{
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
	if (_worker.joinable())
		_worker.join();

	return 0;
}
