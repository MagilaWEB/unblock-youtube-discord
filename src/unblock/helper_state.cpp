#include "helper_state.h"

#include "ipc_signals.h"

#include <format>
#include <ranges>
#include <thread>

namespace
{
	std::vector<std::string> splitSnapshotLines(std::string_view text)
	{
		std::vector<std::string> out;
		size_t					 pos = 0;
		while (pos <= text.size())
		{
			size_t end = text.find('\n', pos);
			if (end == std::string_view::npos)
				end = text.size();
			if (end > pos)
				out.emplace_back(text.substr(pos, end - pos));
			pos = end + 1;
		}

		return out;
	}

	void parseHostStrategySnapshot(std::string_view payload, std::unordered_map<std::string, std::string>& out)
	{
		out.clear();
		size_t pos = 0;
		while (pos <= payload.size())
		{
			size_t end = payload.find('\n', pos);
			if (end == std::string_view::npos)
				end = payload.size();

			const std::string_view line = payload.substr(pos, end - pos);
			pos							= end + 1;
			if (line.empty())
				continue;

			const size_t eq = line.find('=');
			if (eq == std::string_view::npos)
				continue;
			out[std::string(line.substr(0, eq))] = std::string(line.substr(eq + 1));
		}
	}

	void sendHelperUdp(const std::string& message, u32 retries = 5)
	{
		if (message.empty())
			return;

		for (u32 attempt = 0; attempt < retries; ++attempt)
		{
			auto sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock == INVALID_SOCKET)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				continue;
			}

			sockaddr_in addr{};
			addr.sin_family		 = AF_INET;
			addr.sin_port		 = htons(10'000);
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			sendto(sock, message.c_str(), static_cast<int>(message.size()), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
			closesocket(sock);

			// The helper may not have bound 10000 yet right after service
			// start; a short burst covers the bind race without blocking long.
			if (attempt + 1 < retries)
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
}	 // namespace

bool HelperState::_dropExpired()
{
	if (const auto age = IPCSignals::get().latestAge("helper_seen"); age && *age <= c_signal_ttl)
		return false;

	_checking.clear();
	_seen.clear();
	_unjudged.clear();
	_errors.clear();
	_valid.clear();
	_exhausted.clear();
	_stats = {};

	return true;
}

std::vector<std::string> HelperState::checkingHosts()
{
	std::lock_guard lock(_lock);
	auto&			ipc = IPCSignals::get();
	const auto		now = std::chrono::steady_clock::now();

	// CHECKING/DONE stay edges: the in-check set is an instant sample that a
	// 500ms snapshot would miss for millisecond checks. DONE moves a host to
	// a short grace instead of hiding it: the terminal verdict snapshot
	// arrives later, and removing the host immediately blinks the list.
	while (auto host = ipc.getString("helper_checking"))
	{
		// A fully-tried host is terminal: a recheck edge must not pull it
		// out of the exhausted list (that flicker broke autopick settle).
		if (_exhausted.contains(*host))
			continue;

		std::string name = std::move(*host);
		_errors.erase(name);
		_valid.erase(name);
		_unjudged.erase(name);
		_checking.checkingEdge(std::move(name), now);
	}

	while (auto host = ipc.getString("helper_done"))
		_checking.doneEdge(*host, _errors.contains(*host) || _valid.contains(*host) || _exhausted.contains(*host), now);

	if (_dropExpired())
		return {};

	return _checking.visible(now);
}

std::vector<std::string> HelperState::seenHosts()
{
	std::lock_guard lock(_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_seen"))
	{
		_seen.clear();
		for (auto& host : splitSnapshotLines(*payload))
			_seen.insert(std::move(host));
	}

	if (_dropExpired())
		return {};

	return { _seen.begin(), _seen.end() };
}

std::vector<std::string> HelperState::unjudgedHosts()
{
	std::lock_guard lock(_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_unjudged"))
	{
		// Snapshot lines are "host=no-verdict": parse the host=value map and
		// keep only the hostnames.
		std::unordered_map<std::string, std::string> parsed;
		parseHostStrategySnapshot(*payload, parsed);
		_unjudged.clear();
		for (auto& [host, _] : parsed)
			_unjudged.insert(std::move(host));
	}

	if (_dropExpired())
		return {};

	return { _unjudged.begin(), _unjudged.end() };
}

std::vector<std::pair<std::string, std::string>> HelperState::errorHosts()
{
	std::lock_guard lock(_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_error"))
	{
		parseHostStrategySnapshot(*payload, _errors);
		// Exhausted hosts are terminal; drop them from the error set instead
		// of erasing the exhausted mark (the single exit is validHosts).
		std::erase_if(_errors, [this](const auto& kv) { return _exhausted.contains(kv.first); });
		for (const auto& [host, _] : _errors)
		{
			_checking.verdict(host);
			_valid.erase(host);
		}
	}

	if (_dropExpired())
		return {};

	return { _errors.begin(), _errors.end() };
}

std::vector<std::pair<std::string, std::string>> HelperState::validHosts()
{
	std::lock_guard lock(_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_valid"))
	{
		parseHostStrategySnapshot(*payload, _valid);
		for (const auto& [host, _] : _valid)
		{
			_checking.verdict(host);
			_errors.erase(host);
			_exhausted.erase(host);
		}
	}

	if (_dropExpired())
		return {};

	return { _valid.begin(), _valid.end() };
}

std::vector<std::pair<std::string, std::string>> HelperState::exhaustedHosts()
{
	std::lock_guard lock(_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_exhausted"))
	{
		parseHostStrategySnapshot(*payload, _exhausted);
		for (const auto& [host, _] : _exhausted)
		{
			_checking.verdict(host);
			_errors.erase(host);
			_valid.erase(host);
		}
	}

	if (_dropExpired())
		return {};

	return { _exhausted.begin(), _exhausted.end() };
}

HelperStats HelperState::stats()
{
	std::lock_guard lock(_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_stats"))
		if (auto parsed = parseHelperStats(*payload))
			_stats = *parsed;

	if (_dropExpired())
		return {};

	return _stats;
}

void HelperState::clear()
{
	std::lock_guard lock(_lock);
	_checking.clear();
	_seen.clear();
	_unjudged.clear();
	_errors.clear();
	_valid.clear();
	_exhausted.clear();
}

void HelperState::pushConfig() const
{
	sendHelperUdp(_config_message, 3);
}

void HelperState::sendHostList(const std::vector<std::string>& hosts) const
{
	if (hosts.empty())
		return;

	sendHelperUdp(std::format("LIST:{}", hosts | std::views::join_with(':') | std::ranges::to<std::string>()));
}
