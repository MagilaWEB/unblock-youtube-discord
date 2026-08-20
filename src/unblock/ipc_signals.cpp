#include "ipc_signals.h"
#pragma comment(lib, "ws2_32.lib")

IPCSignals& IPCSignals::get()
{
	static IPCSignals instance;
	return instance;
}

IPCSignals::IPCSignals()
{
	auto sock = std::make_unique<Socket>();
	sock->fd  = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock->fd == INVALID_SOCKET)
		return;

	sockaddr_in addr{};
	addr.sin_family		 = AF_INET;
	addr.sin_port		 = htons(9'999);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sock->fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
	{
		Debug::warning("IPC: port 9999 is busy — IPC disabled");
		return;
	}

	u_long mode = 1;
	ioctlsocket(sock->fd, FIONBIO, &mode);
	_sock	  = std::move(sock);
	_listener = std::thread([this] { _listen(); });
}

IPCSignals::~IPCSignals()
{
	_stop = true;

	if (_sock)
	{
		// dummy packet to wake up recvfrom
		sockaddr_in loopback{};
		loopback.sin_family		 = AF_INET;
		loopback.sin_port		 = htons(9'999);
		loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sendto(_sock->fd, "", 1, 0, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback));
	}

	if (_listener.joinable())
		_listener.join();
}

bool IPCSignals::_isValidType(std::string_view t)
{
	static constexpr std::string_view valid[] = { "LOG", "BOOL", "STRING", "U32", "FLOAT" };
	return !t.empty() && t.size() <= 32 && std::ranges::find(valid, t) != std::end(valid);
}

bool IPCSignals::_isValidName(std::string_view n)
{
	return !n.empty() && n.size() <= 255
		&& std::ranges::all_of(
			n,
			[](char c)
			{
				auto uc = static_cast<unsigned char>(c);
				return std::isalnum(uc) || c == '.' || c == '-' || c == '_' || c == '/' || c == ':' || c == '@';
			}
		);
}

bool IPCSignals::_isValidValue(std::string_view type, std::string_view val)
{
	if (type == "BOOL")
		return val == "true" || val == "false" || val == "1" || val == "0";

	if (type == "U32")
	{
		if (val.empty())
			return false;
		unsigned long long num{};
		auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), num);
		return ec == std::errc{} && ptr == val.data() + val.size();
	}

	if (type == "FLOAT")
	{
		if (val.empty())
			return false;
		float num{};
		auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), num);
		return ec == std::errc{} && ptr == val.data() + val.size();
	}

	return true;	// STRING — any text
}

void IPCSignals::_cleanExpired()
{
	auto now = std::chrono::steady_clock::now();
	for (auto it = _data.begin(); it != _data.end();)
		if (now - it->second.created > _ttl)
			it = _data.erase(it);
		else
			++it;
}

std::optional<std::string> IPCSignals::_take(std::string_view name)
{
	std::lock_guard lock(_mutex);
	auto			it = _data.find(std::string(name));
	if (it == _data.end())
		return std::nullopt;

	if (it->second.values.empty())
		return std::nullopt;

	auto val = std::move(it->second.values.front());
	it->second.values.erase(it->second.values.begin());
	if (it->second.values.empty())
		_data.erase(it);

	return val;
}

bool IPCSignals::has(std::string_view name, std::string_view value)
{
	std::lock_guard lock(_mutex);
	auto			it = _data.find(std::string(name));
	if (it == _data.end())
		return false;
	auto& vec = it->second.values;
	for (auto jt = vec.begin(); jt != vec.end(); ++jt)
		if (*jt == value)
		{
			vec.erase(jt);
			if (vec.empty())
				_data.erase(it);
			return true;
		}
	return false;
}

void IPCSignals::_listen()
{
	auto		buf = std::array<char, 1'024>{};
	sockaddr_in from{};
	int			fromlen	   = sizeof(from);
	auto		last_clean = std::chrono::steady_clock::now();

	while (!_stop.load())
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_sock->fd, &fds);
		timeval			tv{};
		struct timeval* ptv = &tv;
		int				sel = select(0, &fds, nullptr, nullptr, ptv);

		if (sel <= 0)
		{
			if (!_stop.load())
			{
				auto now = std::chrono::steady_clock::now();
				if (now - last_clean > std::chrono::minutes(1))
				{
					std::lock_guard lock(_mutex);
					_cleanExpired();
					last_clean = now;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			continue;
		}

		int n = recvfrom(_sock->fd, buf.data(), static_cast<int>(buf.size()) - 1, 0, (sockaddr*)&from, &fromlen);

		if (n <= 0)
			continue;

		buf[n] = 0;
		std::string msg(buf.data());

		auto pos1 = msg.find(':');
		auto pos2 = (pos1 != std::string::npos) ? msg.find(':', pos1 + 1) : std::string::npos;

		auto type = msg.substr(0, pos1);
		auto key =
			(pos1 != std::string::npos) ? msg.substr(pos1 + 1, (pos2 != std::string::npos) ? pos2 - pos1 - 1 : std::string::npos) : std::string{};
		auto val = (pos2 != std::string::npos) ? msg.substr(pos2 + 1) : std::string{};

		if (!_isValidType(type))
		{
			Debug::warning("IPC: invalid type [{}]", type);
			continue;
		}

		// LOG signals — print and done
		if (type == "LOG")
		{
			if (key == "OK")
			{
				Debug::ok("IPC: {}", val);
				continue;
			}
			if (key == "WARNING")
			{
				Debug::warning("IPC: {}", val);
				continue;
			}
			if (key == "INFO")
			{
				Debug::info("IPC: {}", val);
				continue;
			}
			Debug::warning("IPC: unknown log level [{}]", key);
			continue;
		}

		// Data signals: need valid name and value
		if (!_isValidName(key))
		{
			Debug::warning("IPC: invalid name [{}] for {}", key, type);
			continue;
		}
		if (!_isValidValue(type, val))
		{
			Debug::warning("IPC: invalid value [{}] for {}", val, type);
			continue;
		}

		{
			std::lock_guard lock(_mutex);
			_data[std::string(key)].values.emplace_back(val);
			_data[std::string(key)].created = std::chrono::steady_clock::now();
		}

#ifdef DEBUG
		Debug::info("IPC: {} {}={}", type, key, val);
#endif
	}
}

std::optional<std::string> IPCSignals::getString(std::string_view name)
{
	return _take(name);
}

std::optional<bool> IPCSignals::getBool(std::string_view name)
{
	auto raw = _take(name);
	if (!raw)
		return std::nullopt;
	if (*raw == "true" || *raw == "1")
		return true;
	if (*raw == "false" || *raw == "0")
		return false;
	return std::nullopt;
}

std::optional<float> IPCSignals::getFloat(std::string_view name)
{
	auto raw = _take(name);
	if (!raw)
		return std::nullopt;
	float val{};
	auto [ptr, ec] = std::from_chars(raw->data(), raw->data() + raw->size(), val);
	if (ec != std::errc{} || ptr != raw->data() + raw->size())
		return std::nullopt;
	return val;
}

std::optional<uint32_t> IPCSignals::getU32(std::string_view name)
{
	auto raw = _take(name);
	if (!raw)
		return std::nullopt;

	unsigned long long val{};
	auto [ptr, ec] = std::from_chars(raw->data(), raw->data() + raw->size(), val);
	if (ec != std::errc{} || ptr != raw->data() + raw->size())
		return std::nullopt;

	if (val > UINT32_MAX)
	{
		Debug::warning("IPC: value [{}] exceeds u32, truncated", *raw);
		return static_cast<uint32_t>(val);
	}
	return static_cast<uint32_t>(val);
}

void IPCSignals::clear(std::string_view name)
{
	std::lock_guard lock(_mutex);
	_data.erase(std::string(name));
}

void IPCSignals::clearAll()
{
	std::lock_guard lock(_mutex);
	_data.clear();
}
