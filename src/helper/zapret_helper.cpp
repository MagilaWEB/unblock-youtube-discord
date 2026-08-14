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
	host = host.substr(0, host.find(':'));
	if (_isValidHost(host))
	{
		_known_hosts.insert(std::string{ host });
		_queue.insert(std::string{ host });
	}
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
			_strategy[std::string{ host }] = std::string{ strat };
			_send(std::format("STRING:helper_strategy:{}:{}", host, strat), c_ipc_port);
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
		std::vector<std::string> batch;
		batch.reserve(c_batch_size);

		{
			std::lock_guard lock(_mutex);
			while (!_queue.empty() && batch.size() < c_batch_size)
			{
				auto it_host = _queue.begin();
				if (!_active.contains(*it_host))
				{
					_active.insert(*it_host);
					batch.emplace_back(*it_host);
					_queue.erase(it_host);
				}
			}
		}

		if (_queue.empty())
		{
			const auto now = steady_clock::now();
			if (now - _last_recheck > c_recheck_interval)
			{
				std::lock_guard lock(_mutex);
				for (const auto& host : _known_hosts)
					if (!_active.contains(host))
						_queue.insert(host);

				_last_recheck = now;
			}
			else
				for (const auto& host : _known_hosts)
					_send(std::format("STRING:helper_seen:{}", host), c_ipc_port);
		}

		if (!_active.empty())
			for (auto& host : _active)
				_send(std::format("STRING:helper_checking:{}", host), c_ipc_port);

		if (batch.empty())
		{
			std::this_thread::sleep_for(c_sleep_short);
			continue;
		}

		std::for_each(
			std::execution::par,
			batch.begin(),
			batch.end(),
			[this](std::string& host)
			{
				_checkHost(host);
				std::lock_guard lock(_mutex);
				_active.erase(host);
			}
		);
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
