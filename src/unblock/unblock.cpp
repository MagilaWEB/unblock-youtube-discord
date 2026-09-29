#include "unblock.h"
#include "domain_testing.h"
#include "dns_host.h"
#include "ipc_signals.h"
#include "zapret1_engine.h"
#include "zapret2_engine.h"
#include <curl/curl.h>

#include "../core/hidden_process.h"

#include <shellapi.h>
#include <winreg.h>

#include <filesystem>
#include <string>

Unblock::Unblock()
{
	_zapret1_engine = std::make_unique<Zapret1Engine>();
	_zapret2_engine = std::make_unique<Zapret2Engine>();
	_domain_testing = std::make_unique<DomainTesting>();
	_dns_hosts		= std::make_unique<DNSHost>();

	(void)IPCSignals::get();
	_zapret_helper.open();
	_win_divert.open();
	_dns_proxy.repairBoot();
}

Unblock::~Unblock() = default;

bool Unblock::testUrl(std::string_view str_url)
{
	DomainTesting::CurlDomain domain{ curl_easy_init(), std::string{ str_url } };
	const bool				  state_url = DomainTesting::isConnectionUrl(nullptr, domain);
	curl_easy_cleanup(domain.curl);
	return state_url;
}

ZapretEngine& Unblock::engine(Technology technology)
{
	if (technology == Technology::Zapret1)
		return *_zapret1_engine;

	return *_zapret2_engine;
}

bool Unblock::automaticallyStrategy(Technology technology)
{
	return engine(technology).automaticallyStrategy();
}

void Unblock::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_zapret1_engine->serviceConfigFile(config);
	_zapret2_engine->serviceConfigFile(config);
}

void Unblock::changeStrategy(Technology technology, std::string_view name_config)
{
	engine(technology).changeStrategy(name_config);
}

void Unblock::changeDirVersionStrategy(Technology technology, std::string_view dir_version)
{
	engine(technology).changeDirVersion(dir_version);
}

void Unblock::changeFakeKey(Technology technology, std::string_view key)
{
	engine(technology).changeFakeKey(key);
}

std::vector<std::string> Unblock::fakeBinKeys(Technology technology)
{
	return engine(technology).fakeBinKeys();
}

std::string Unblock::fakeBinKey(Technology technology)
{
	return engine(technology).fakeBinKey();
}

void Unblock::addOptionalStrategies(std::string_view name)
{
	auto it = std::ranges::find(_section_opt_service_names, name);
	if (it != _section_opt_service_names.end())
		return;

	_section_opt_service_names.emplace_back(name);

	_zapret1_engine->changeOptionalServices(_section_opt_service_names);
	_zapret2_engine->changeOptionalServices(_section_opt_service_names);
	_domain_testing->changeOptionalServices(_section_opt_service_names);
}

void Unblock::removeOptionalStrategies(std::string_view name)
{
	std::erase(_section_opt_service_names, name);
	_zapret1_engine->changeOptionalServices(_section_opt_service_names);
	_zapret2_engine->changeOptionalServices(_section_opt_service_names);
	_domain_testing->changeOptionalServices(_section_opt_service_names);
}

void Unblock::clearOptionalStrategies()
{
	_section_opt_service_names.clear();

	_zapret1_engine->changeOptionalServices({});
	_zapret2_engine->changeOptionalServices({});
	_domain_testing->changeOptionalServices({});
}

void Unblock::setCustomLists(
	std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
)
{
	_zapret1_engine->changeCustomLists(hosts, ip_set, domains_exclude, ip_exclude);
	_zapret2_engine->changeCustomLists(std::move(hosts), std::move(ip_set), std::move(domains_exclude), std::move(ip_exclude));
}

bool Unblock::runTest()
{
	return _domain_testing->isTesting();
}

std::string Unblock::getNameStrategies(Technology technology)
{
	return engine(technology).strategyName();
}

const std::vector<std::string>& Unblock::getStrategies(Technology technology)
{
	return engine(technology).strategies();
}

const std::vector<std::string>& Unblock::getStrategiesList(Technology technology)
{
	return engine(technology).strategiesList();
}

std::list<Service> Unblock::getConflictingServices()
{
	std::list<Service> conflicting_service;

	Service::allService(
		[&](std::string name_service) -> void
		{
			if (name_service.empty())
				return;

			Service service{ name_service };
			service.open();

			auto& config = service.getConfig();

			constexpr static std::string_view services_conflict[]{ "winws.exe", "winws2.exe", "goodbyedpi.exe", "ciadpi.exe" };

			for (auto& name_prosses : services_conflict)
			{
				if (config.binary_path.contains(name_prosses))
				{
					if (name_service == _zapret1_engine->serviceName())
						continue;

					if (name_service == _zapret2_engine->serviceName())
						continue;

					if (name_service == _win_divert.getName())
						continue;

					conflicting_service.emplace_back(name_service);
					conflicting_service.back().open();
				}
			}
		}
	);

	return conflicting_service;
}

void Unblock::testingDomain(std::function<void(std::string_view url, bool state)>&& callback, bool base_test)
{
	// The retry/exhausted coordination over UDP 9999 (zcheck) exists only in
	// Zapret2. With Zapret1 running the test must behave exactly like with
	// everything stopped: a plain single-attempt curl per host.
	_domain_testing->test(base_test, [callback](std::string_view url, bool state) { callback(url, state); }, _zapret2_engine->isRun());

	_domain_testing->printTestInfo();
}

void Unblock::testingDomainCancel()
{
	_domain_testing->cancelTesting();
}

std::optional<std::string> Unblock::checkUpdate() const
{
	return _updater.check();
}

bool Unblock::appUpdate()
{
	return _updater.run();
}

float Unblock::appUpdateProgress() const
{
	return _updater.progress();
}

u32 Unblock::domainSuccessRate() const
{
	return _domain_testing->successRate();
}

bool Unblock::validDomain() const
{
	return domainSuccessRate() >= MAX_SUCCESS_CONECTION;
}

bool Unblock::activeService()
{
	return runningTechnology().has_value();
}

bool Unblock::isRun(Technology technology)
{
	return engine(technology).isRun();
}

std::optional<Technology> Unblock::runningTechnology()
{
	if (_zapret1_engine->isRun())
		return Technology::Zapret1;

	if (_zapret2_engine->isRun())
		return Technology::Zapret2;

	return std::nullopt;
}

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
}

bool Unblock::_dropExpiredHelperStates()
{
	// The helper re-broadcasts the full host snapshot every tick, always
	// including helper_seen, so its age is the liveness signal. No fresh
	// snapshot past the TTL means every list is stale.
	const auto age = IPCSignals::get().latestAge("helper_seen");
	if (age && *age <= c_helper_signal_ttl)
		return false;

	_helper_checking.clear();
	_helper_seen.clear();
	_helper_errors.clear();
	_helper_valid.clear();
	_helper_exhausted.clear();
	_helper_stats = {};
	return true;
}

std::vector<std::string> Unblock::helperCheckingHosts()
{
	std::lock_guard lock(_helper_state_lock);
	auto&			ipc = IPCSignals::get();
	const auto		now = std::chrono::steady_clock::now();

	// CHECKING/DONE stay edges: the in-check set is an instant sample that a
	// 500ms snapshot would miss for millisecond checks. DONE moves a host to
	// a short grace instead of hiding it: the terminal verdict snapshot
	// arrives later, and removing the host immediately blinks the list.
	while (auto host = ipc.getString("helper_checking"))
	{
		std::string name = std::move(*host);
		// A fully-tried host is terminal: a recheck edge must not pull it
		// out of the exhausted list (that flicker broke autopick settle).
		if (_helper_exhausted.contains(name))
			continue;
		_helper_errors.erase(name);
		_helper_valid.erase(name);
		_helper_checking.checkingEdge(std::move(name), now);
	}

	while (auto host = ipc.getString("helper_done"))
	{
		const bool terminal = _helper_errors.contains(*host) || _helper_valid.contains(*host) || _helper_exhausted.contains(*host);
		_helper_checking.doneEdge(*host, terminal, now);
	}

	if (_dropExpiredHelperStates())
		return {};

	return _helper_checking.visible(now);
}

std::vector<std::string> Unblock::helperSeenHosts()
{
	std::lock_guard lock(_helper_state_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_seen"))
	{
		_helper_seen.clear();
		for (auto& host : splitSnapshotLines(*payload))
			_helper_seen.insert(std::move(host));
	}

	if (_dropExpiredHelperStates())
		return {};

	return { _helper_seen.begin(), _helper_seen.end() };
}

std::vector<std::pair<std::string, std::string>> Unblock::helperErrorHosts()
{
	std::lock_guard lock(_helper_state_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_error"))
	{
		parseHostStrategySnapshot(*payload, _helper_errors);
		// Exhausted hosts are terminal; drop them from the error set instead
		// of erasing the exhausted mark (the single exit is helperValidHosts).
		std::erase_if(_helper_errors, [this](const auto& kv) { return _helper_exhausted.contains(kv.first); });
		for (const auto& [host, _] : _helper_errors)
		{
			_helper_checking.verdict(host);
			_helper_valid.erase(host);
		}
	}

	if (_dropExpiredHelperStates())
		return {};

	std::vector<std::pair<std::string, std::string>> result;
	result.reserve(_helper_errors.size());
	for (const auto& [host, strategy] : _helper_errors)
		result.emplace_back(host, strategy);

	return result;
}

std::vector<std::pair<std::string, std::string>> Unblock::helperValidHosts()
{
	std::lock_guard lock(_helper_state_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_valid"))
	{
		parseHostStrategySnapshot(*payload, _helper_valid);
		for (const auto& [host, _] : _helper_valid)
		{
			_helper_checking.verdict(host);
			_helper_errors.erase(host);
			_helper_exhausted.erase(host);
		}
	}

	if (_dropExpiredHelperStates())
		return {};

	std::vector<std::pair<std::string, std::string>> result;
	result.reserve(_helper_valid.size());
	for (const auto& [host, strategy] : _helper_valid)
		result.emplace_back(host, strategy);

	return result;
}

std::vector<std::pair<std::string, std::string>> Unblock::helperExhaustedHosts()
{
	std::lock_guard lock(_helper_state_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_exhausted"))
	{
		parseHostStrategySnapshot(*payload, _helper_exhausted);
		for (const auto& [host, _] : _helper_exhausted)
		{
			_helper_checking.verdict(host);
			_helper_errors.erase(host);
			_helper_valid.erase(host);
		}
	}

	if (_dropExpiredHelperStates())
		return {};

	std::vector<std::pair<std::string, std::string>> result;
	result.reserve(_helper_exhausted.size());
	for (const auto& [host, strategy] : _helper_exhausted)
		result.emplace_back(host, strategy);

	return result;
}

HelperStats Unblock::helperStats()
{
	std::lock_guard lock(_helper_state_lock);
	if (auto payload = IPCSignals::get().getLatest("helper_stats"))
		if (auto parsed = parseHelperStats(*payload))
			_helper_stats = *parsed;

	if (_dropExpiredHelperStates())
		return {};

	return _helper_stats;
}

std::vector<std::string> Unblock::testHostNames()
{
	std::vector<std::string> hosts;
	for (auto& line : _domain_testing->listHost())
	{
		std::smatch m;
		if (!(std::regex_search(line, m, std::regex{ R"(://([^/?#]+))" }) && m.size() > 1))
			continue;

		std::string host = m[1].str();
		if (isHelperHostName(host))
			hosts.emplace_back(std::move(host));
	}
	return hosts;
}

std::vector<std::string> Unblock::listVersionStrategy(Technology technology)
{
	return engine(technology).listVersionStrategy();
}

void Unblock::dnsHosts(bool state)
{
	state ? _dns_hosts->enable() : _dns_hosts->disable();
}

void Unblock::dnsHostsUpdate()
{
	_dns_hosts->update();
}

void Unblock::dnsHostsCancelUpdate()
{
	_dns_hosts->cancel();
}

float Unblock::dnsHostsDownloadProgress() const
{
	return _dns_hosts->downloadProgress();
}

bool Unblock::dnsHostsCheck() const
{
	return _dns_hosts->isHostsUser();
}

const std::list<std::string>& Unblock::dnsHostsListName()
{
	return _dns_hosts->listDnsFileName();
}

void Unblock::setDnsHostsRegion(std::string_view region)
{
	_dns_hosts->setRegion(region);
}

const std::string& Unblock::dnsHostsRegion() const
{
	return _dns_hosts->region();
}

void Unblock::setDnsHostsBaseUrl(std::string_view url)
{
	_dns_hosts->setBaseUrl(url);
}

const std::string& Unblock::dnsHostsBaseUrl() const
{
	return _dns_hosts->baseUrl();
}

bool Unblock::dnsHostsRegionAvailable(std::string_view region) const
{
	return _dns_hosts->regionAvailable(region);
}

std::vector<std::string> Unblock::defaultDnsProxyUpstreams()
{
	return DnsProxy::defaultUpstreams();
}

std::vector<std::string> Unblock::defaultDnsProxyBootstrap()
{
	return DnsProxy::defaultBootstrap();
}

uint32_t Unblock::defaultDnsProxyTimeout()
{
	return DnsProxy::defaultTimeout();
}

namespace
{
	// RFC 1323 timestamps live in Tcp1323Opts bit 0x2. The value is absent on
	// a stock Windows install (timestamps disabled), so a missing key means
	// "off" — no need to parse localized netsh output.
	bool tcpTimestampsEnabled()
	{
		DWORD		  value	 = 0;
		DWORD		  size	 = sizeof(value);
		const LSTATUS status = RegGetValueW(
			HKEY_LOCAL_MACHINE,
			L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
			L"Tcp1323Opts",
			RRF_RT_REG_DWORD,
			nullptr,
			&value,
			&size
		);

		return status == ERROR_SUCCESS && (value & 0x2) != 0;
	}
}	 // namespace

void Unblock::_tcpTimestampSync(Technology technology)
{
	if (!strategyUsesTcpTimestamps(getStrategies(technology)))
		return;

	std::scoped_lock lock(_tcp_timestamp_lock);
	if (_tcp_timestamps_owned || tcpTimestampsEnabled())
		return;

	if (runHiddenProcess({ "netsh", "interface", "tcp", "set", "global", "timestamps=enabled" }, 10'000))
		_tcp_timestamps_owned = true;
}

void Unblock::_tcpTimestampRestore()
{
	std::scoped_lock lock(_tcp_timestamp_lock);
	if (!_tcp_timestamps_owned)
		return;

	if (runHiddenProcess({ "netsh", "interface", "tcp", "set", "global", "timestamps=disabled" }, 10'000))
		_tcp_timestamps_owned = false;
}

void Unblock::dnsProxy(bool state)
{
	_dns_proxy.run(state);
}

bool Unblock::dnsProxyIsRun()
{
	return _dns_proxy.isRun();
}

void Unblock::setDnsProxyUpstreams(std::vector<std::string> upstreams)
{
	_dns_proxy.setUpstreams(std::move(upstreams));
}

const std::vector<std::string>& Unblock::dnsProxyUpstreams() const
{
	return _dns_proxy.upstreams();
}

void Unblock::setDnsProxyBootstrap(std::vector<std::string> bootstrap)
{
	_dns_proxy.setBootstrap(std::move(bootstrap));
}

const std::vector<std::string>& Unblock::dnsProxyBootstrap() const
{
	return _dns_proxy.bootstrap();
}

void Unblock::setDnsProxyTimeout(uint32_t timeout_ms)
{
	_dns_proxy.setTimeout(timeout_ms);
}

uint32_t Unblock::dnsProxyTimeout() const
{
	return _dns_proxy.timeout();
}

std::string Unblock::dnsProxyStatus() const
{
	return _dns_proxy.status();
}

bool Unblock::dnsProxyTestUpstream(const std::string& value, std::string& output)
{
	return _dns_proxy.testUpstream(value, output);
}

void Unblock::dnsProxyRepairBoot()
{
	_dns_proxy.repairBoot();
}

void Unblock::localProxyTg(bool run)
{
	_tg_proxy.run(run);
}

void Unblock::setTgProxyParams(std::string_view host, std::string_view port, std::array<std::string, 4> dc_ip, std::string_view cfproxy_worker_domain)
{
	_tg_proxy.setParams(host, port, std::move(dc_ip), cfproxy_worker_domain);
}

bool Unblock::localProxyTgIsRun()
{
	return _tg_proxy.isRun();
}

void Unblock::localProxyTgLinkRun()
{
	_tg_proxy.linkRun();
}

void Unblock::removeService()
{
	_tcpTimestampRestore();
	_helper_seen.clear();
	_helper_checking.clear();
	_helper_errors.clear();
	_helper_valid.clear();
	_helper_exhausted.clear();
	_zapret1_engine->remove();
	_zapret2_engine->remove();
	_zapret_helper.remove();
	_win_divert.remove();
}

void Unblock::stopService()
{
	_tcpTimestampRestore();
	_helper_seen.clear();
	_helper_checking.clear();
	_helper_errors.clear();
	_helper_valid.clear();
	_helper_exhausted.clear();
	_zapret1_engine->stop();
	_zapret2_engine->stop();
	_zapret_helper.stop();
}

namespace
{
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

void Unblock::pushHelperConfig() const
{
	sendHelperUdp(_helper_config_message, 3);
}

void Unblock::startService(Technology technology)
{
	_helper_seen.clear();
	_helper_checking.clear();
	_helper_errors.clear();
	_helper_valid.clear();
	_helper_exhausted.clear();

	// Exactly one technology runs at a time.
	if (technology == Technology::Zapret1)
		_zapret2_engine->stop();
	else
		_zapret1_engine->stop();

	_zapret_helper.remove();

	auto& eng  = engine(technology);
	auto& list = eng.strategies();
	if (list.empty())
		return;

	// The helper only understands the zapret2 protocol (zcheck over
	// --lua-desync); Zapret1 runs standalone like in 1.4.19.
	if (technology == Technology::Zapret2)
	{
		_zapret_helper.setDescription(Localization::Str{ "str_service_zapret_description" }());
		_zapret_helper.setArgs({ (Core::get().binPath() / "zapret_helper.exe").string() });
		_zapret_helper.create();
		_zapret_helper.start();
	}

	// ts/tcp_ts fooling needs OS TCP timestamps, which Windows ships off.
	// Enable them only when the effective strategy uses it, and remember it
	// so stopService/removeService can put the system back.
	_tcpTimestampSync(technology);

	eng.start();

	if (technology != Technology::Zapret2)
		return;

	// Fresh settings first: the on-disk setting.config is stale while
	// unblock runs (File::save on close), so the helper cannot rely on
	// reading the file at startup. Then the domain list, both with retries
	// for the helper bind race.
	pushHelperConfig();

	// send domain list to zapret-helper
	{
		auto hosts = testHostNames();
		if (!hosts.empty())
		{
			std::string list = std::format("LIST:{}", hosts | std::views::join_with(':') | std::ranges::to<std::string>());

			sendHelperUdp(list);
		}
	}
}
