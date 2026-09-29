#include "unblock.h"
#include "../engine/version.hpp"
#include "domain_testing.h"
#include "ipc_signals.h"
#include <bit7z/bitfileextractor.hpp>
#include <curl/curl.h>

#include <shellapi.h>
#include <winreg.h>

#include <filesystem>
#include <string>

Unblock::Unblock()
{
	(void)IPCSignals::get();
	_zapret_helper.open();
	_win_divert.open();
	_tg_ws_proxy.open();
	_dns_proxy.open();
	dnsProxyRepairBoot();
}

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
		return _zapret1_engine;

	return _zapret2_engine;
}

bool Unblock::automaticallyStrategy(Technology technology)
{
	return engine(technology).automaticallyStrategy();
}

void Unblock::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_zapret1_engine.serviceConfigFile(config);
	_zapret2_engine.serviceConfigFile(config);
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

	_zapret1_engine.changeOptionalServices(_section_opt_service_names);
	_zapret2_engine.changeOptionalServices(_section_opt_service_names);
	_domain_testing.changeOptionalServices(_section_opt_service_names);
}

void Unblock::removeOptionalStrategies(std::string_view name)
{
	std::erase(_section_opt_service_names, name);
	_zapret1_engine.changeOptionalServices(_section_opt_service_names);
	_zapret2_engine.changeOptionalServices(_section_opt_service_names);
	_domain_testing.changeOptionalServices(_section_opt_service_names);
}

void Unblock::clearOptionalStrategies()
{
	_section_opt_service_names.clear();

	_zapret1_engine.changeOptionalServices({});
	_zapret2_engine.changeOptionalServices({});
	_domain_testing.changeOptionalServices({});
}

void Unblock::setCustomLists(
	std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
)
{
	_zapret1_engine.changeCustomLists(hosts, ip_set, domains_exclude, ip_exclude);
	_zapret2_engine.changeCustomLists(std::move(hosts), std::move(ip_set), std::move(domains_exclude), std::move(ip_exclude));
}

bool Unblock::runTest()
{
	return _domain_testing.isTesting();
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
					if (name_service == _zapret1_engine.serviceName())
						continue;

					if (name_service == _zapret2_engine.serviceName())
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
	_domain_testing.test(base_test, [callback](std::string_view url, bool state) { callback(url, state); }, _zapret2_engine.isRun());

	_domain_testing.printTestInfo();
}

void Unblock::testingDomainCancel()
{
	_domain_testing.cancelTesting();
}

std::optional<std::string> Unblock::checkUpdate() const
{
	HttpsLoad version{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest" };

	auto lines = version.run();

	if (version.codeResult() != 200)
		return {};

	for (auto& line : lines)
	{
		constexpr static std::string_view version_mask{ "/MagilaWEB/unblock-youtube-discord/tree/v" };
		size_t							  pos = line.find(version_mask);
		if (pos != std::string::npos)
		{
			constexpr static std::string_view mask_end{ "\" data-tab-item=\"i0code-tab\"" };
			size_t							  pos_end = line.find(mask_end);
			if (pos_end != std::string::npos)
			{
				auto start_str = pos + version_mask.length();
				auto str	   = line.substr(start_str, pos_end - start_str);
				if (Core::get().isVersionNewer(str, VERSION_STR))
					return str;

				return {};
			}
		}
	}

	return {};
}

bool Unblock::appUpdate()
{
	// The update is delegated to the standalone unblock_update.exe (see src/unblock_update);
	// all temporary files live under %TEMP%\unblock — no .bat scripts are dropped into
	// the application root.
	const auto temp_root = Core::get().tempPath() / "unblock";
	const auto bin_path	 = Core::get().binPath();

	std::error_code ec;
	std::filesystem::remove_all(temp_root, ec);
	std::filesystem::create_directories(temp_root, ec);

	const auto archive = temp_root / "new_unblock.7z";

	auto load = std::make_shared<HttpsLoad>("https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest/download/unblock.7z");
	{
		std::scoped_lock lock(_update_load_mutex);
		_update_load = load;
	}

	if (!load->runToFile(archive))
		return false;

	const u32 code = load->codeResult();
	if (code != 200)
		return false;

	try
	{
		// Absolute path: the process working directory is not guaranteed
		// to be bin/ (shortcuts, service starts), so a relative "7za.dll"
		// silently fails to load on the first update attempt.
		static bit7z::Bit7zLibrary	   lib{ (Core::get().binPath() / "7za.dll").string() };
		static bit7z::BitFileExtractor extractor{ lib, bit7z::BitFormat::SevenZip };

		extractor.extract(archive.string(), temp_root.string());
	}
	catch (const bit7z::BitException& ex)
	{
		Debug::warning("{}", ex.what());
		return false;
	}

	// The archive must contain the wrapped payload with the new engine —
	// otherwise the helper would wipe temp and restart the old build,
	// looking like "the update did nothing".
	const auto payload_dir = temp_root / "unblock";
	if (!std::filesystem::exists(payload_dir / "bin" / "engine.exe", ec) || ec)
	{
		Debug::warning("Update payload is missing unblock/bin/engine.exe");
		return false;
	}

	// Stage the helper into %TEMP% and run it from there (same as the
	// remove flow): a helper running from bin/ locks its own image, so the
	// payload copy of bin/ either fails mid-way (partial update that looks
	// like "nothing happened") or can never refresh the helper itself.
	// From %TEMP% the helper can wipe + copy managed dirs freely.
	const auto staged_updater = temp_root / "unblock_update.exe";
	{
		const auto	payload_updater = payload_dir / "bin" / "unblock_update.exe";
		const auto& source			= std::filesystem::exists(payload_updater) ? payload_updater : bin_path / "unblock_update.exe";

		std::error_code copy_ec;
		std::filesystem::copy_file(source, staged_updater, std::filesystem::copy_options::overwrite_existing, copy_ec);
		if (copy_ec)
		{
			Debug::error("Failed to stage unblock_update: {}", copy_ec.message());
			return false;
		}
	}

	std::wstring cmd_line = L"\"" + staged_updater.wstring() + L"\" \"" + Core::get().currentPath().wstring() + L"\" "
						  + std::to_wstring(GetCurrentProcessId()) + L" update \"" + temp_root.wstring() + L"\"";

	STARTUPINFOW		startup{};
	PROCESS_INFORMATION process{};
	startup.cb = sizeof(startup);

	if (!CreateProcessW(nullptr, cmd_line.data(), nullptr, nullptr, FALSE, 0, nullptr, temp_root.c_str(), &startup, &process))
	{
		Debug::error("Failed to start unblock_update: {}", static_cast<u32>(GetLastError()));
		return false;
	}

	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

float Unblock::appUpdateProgress() const
{
	std::shared_ptr<HttpsLoad> load;
	{
		std::scoped_lock lock(_update_load_mutex);
		load = _update_load;
	}

	return load ? load->progress() : 0.F;
}

u32 Unblock::domainSuccessRate() const
{
	return _domain_testing.successRate();
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
	if (_zapret1_engine.isRun())
		return Technology::Zapret1;

	if (_zapret2_engine.isRun())
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
	for (auto& line : _domain_testing.listHost())
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
	state ? _dns_hosts.enable() : _dns_hosts.disable();
}

void Unblock::dnsHostsUpdate()
{
	_dns_hosts.update();
}

void Unblock::dnsHostsCancelUpdate()
{
	_dns_hosts.cancel();
}

float Unblock::dnsHostsDownloadProgress() const
{
	return _dns_hosts.downloadProgress();
}

bool Unblock::dnsHostsCheck() const
{
	return _dns_hosts.isHostsUser();
}

const std::list<std::string>& Unblock::dnsHostsListName()
{
	return _dns_hosts.listDnsFileName();
}

void Unblock::setDnsHostsRegion(std::string_view region)
{
	_dns_hosts.setRegion(region);
}

const std::string& Unblock::dnsHostsRegion() const
{
	return _dns_hosts.region();
}

void Unblock::setDnsHostsBaseUrl(std::string_view url)
{
	_dns_hosts.setBaseUrl(url);
}

const std::string& Unblock::dnsHostsBaseUrl() const
{
	return _dns_hosts.baseUrl();
}

bool Unblock::dnsHostsRegionAvailable(std::string_view region) const
{
	return _dns_hosts.regionAvailable(region);
}

std::vector<std::string> Unblock::defaultDnsProxyUpstreams()
{
	// Order is the priority: the first entry is the primary resolver, the
	// rest are only fallbacks (see unblock_dns buildSettings). GeoHide leads
	// (queries must reach it for region-specific answers) across its DoH/DoT
	// endpoints and ports, then Comss.one (encrypted, unfiltered), then Xbox
	// DNS (free Smart DNS), then plain DNS as the last resort.
	return {
		"https://dns.geohide.ru:8443/dns-query",
		"https://dns.geohide.ru:853/dns-query",
		"https://dns.geohide.ru:443/dns-query",
		"tls://dns.geohide.ru:8443",
		"tls://dns.geohide.ru:853",
		"tls://dns.geohide.ru:443",
		"https://dns.comss.one/dns-query",
		"tls://dns.comss.one",
		"111.88.96.54",
		"111.88.96.55",
		"1.1.1.1",
		"1.0.0.1",
	};
}

std::vector<std::string> Unblock::defaultDnsProxyBootstrap()
{
	// Plain DNS used only to resolve hostname upstreams. Editable in the UI;
	// if the ISP poisons these, the user can point them at a reachable
	// resolver (e.g. GeoHide's own IPs).
	return { "1.1.1.1", "8.8.8.8", "9.9.9.9", "77.88.8.8" };
}

uint32_t Unblock::defaultDnsProxyTimeout()
{
	// A private resolver (GeoHide) stalls from time to time; a generous
	// default avoids spurious SERVFAILs. Editable in the UI.
	return 15'000;
}

std::filesystem::path Unblock::_dnsProxyConfigPath() const
{
	return Core::get().userPath() / "dns_proxy.conf";
}

std::filesystem::path Unblock::_dnsProxyBackupPath() const
{
	return Core::get().userPath() / "dns_proxy.adapters";
}

std::filesystem::path Unblock::_dnsProxyLogPath() const
{
	return Core::get().userPath() / "dns_proxy.log";
}

void Unblock::_dnsProxyWriteConfig(const std::vector<std::string>& upstreams, const std::vector<std::string>& bootstrap, uint32_t timeout_ms)
{
	File conf{ false };
	conf.open(_dnsProxyConfigPath(), "", true);
	conf.clear();

	conf.writeText("listen=127.0.0.1");
	conf.writeText("port=53");
	conf.writeText("timeout=" + std::to_string(timeout_ms));
	// Explicit paths: the wrapper reads these verbatim, so engine and wrapper
	// can never disagree on where backup/log live. Status is pushed over IPC
	// (UDP 9999), not written to disk.
	conf.writeText("backup=" + _dnsProxyBackupPath().string());
	conf.writeText("log=" + _dnsProxyLogPath().string());

	std::string bootstrap_line;
	for (const auto& b : bootstrap)
		bootstrap_line += (bootstrap_line.empty() ? "" : ",") + b;
	conf.writeText("bootstrap=" + bootstrap_line);

	for (const auto& upstream : upstreams)
		if (!upstream.empty())
			conf.writeText("upstream=" + upstream);

	conf.close();
}

bool Unblock::_runHidden(const std::vector<std::string>& args, uint32_t timeout_ms)
{
	if (args.empty())
		return false;

	auto quote = [](const std::string& value)
	{
		std::string out{ "\"" };
		for (char ch : value)
		{
			if (ch == '"')
				out += '\\';
			out += ch;
		}
		out += '"';
		return out;
	};

	std::string cmdline;
	for (auto& arg : args)
	{
		if (!cmdline.empty())
			cmdline += ' ';
		cmdline += quote(arg);
	}

	auto wide_cmd = utils::utf8ToUtf16(cmdline);
	if (wide_cmd.empty())
		return false;

	STARTUPINFOW		si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);

	if (!CreateProcessW(nullptr, wide_cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		return false;

	const DWORD wait = WaitForSingleObject(pi.hProcess, timeout_ms);
	if (wait == WAIT_TIMEOUT)
		TerminateProcess(pi.hProcess, 1);

	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return code == 0;
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

	if (_runHidden({ "netsh", "interface", "tcp", "set", "global", "timestamps=enabled" }, 10'000))
		_tcp_timestamps_owned = true;
}

void Unblock::_tcpTimestampRestore()
{
	std::scoped_lock lock(_tcp_timestamp_lock);
	if (!_tcp_timestamps_owned)
		return;

	if (_runHidden({ "netsh", "interface", "tcp", "set", "global", "timestamps=disabled" }, 10'000))
		_tcp_timestamps_owned = false;
}

void Unblock::dnsProxy(bool state)
{
	if (!state)
	{
		_dns_proxy.remove();
		// SvcHost stops its child with TerminateProcess (see MagilaWEB/svc_host),
		// so the wrapper never gets to run its own OS-DNS restore on stop. Undo
		// the adapter switch from here, using the backup written when it happened.
		dnsProxyRepairBoot();
		IPCSignals::get().clear("dns.queries");
		IPCSignals::get().clear("dns.cache_hits");
		IPCSignals::get().clear("dns.errors");
		return;
	}

	const auto& upstreams = _dns_proxy_upstreams.empty() ? defaultDnsProxyUpstreams() : _dns_proxy_upstreams;
	const auto& bootstrap = _dns_proxy_bootstrap.empty() ? defaultDnsProxyBootstrap() : _dns_proxy_bootstrap;
	const auto	timeout	  = _dns_proxy_timeout_ms != 0 ? _dns_proxy_timeout_ms : defaultDnsProxyTimeout();

	_dnsProxyWriteConfig(upstreams, bootstrap, timeout);

	// Reset the IPC counters so a stale run's numbers don't show on start.
	IPCSignals::get().clear("dns.queries");
	IPCSignals::get().clear("dns.cache_hits");
	IPCSignals::get().clear("dns.errors");

	_dns_proxy.remove();
	_dns_proxy.setDescription("Unblock DNS proxy (AdGuard DnsLibs).");
	_dns_proxy.setArgs({ (Core::get().binPath() / "unblock_dns.exe").string(), "--config", "\"" + _dnsProxyConfigPath().string() + "\"" });
	_dns_proxy.create();
	_dns_proxy.start();
}

bool Unblock::dnsProxyIsRun()
{
	return _dns_proxy.isRun();
}

void Unblock::setDnsProxyUpstreams(std::vector<std::string> upstreams)
{
	_dns_proxy_upstreams = std::move(upstreams);
}

const std::vector<std::string>& Unblock::dnsProxyUpstreams() const
{
	return _dns_proxy_upstreams;
}

void Unblock::setDnsProxyBootstrap(std::vector<std::string> bootstrap)
{
	_dns_proxy_bootstrap = std::move(bootstrap);
}

const std::vector<std::string>& Unblock::dnsProxyBootstrap() const
{
	return _dns_proxy_bootstrap;
}

void Unblock::setDnsProxyTimeout(uint32_t timeout_ms)
{
	_dns_proxy_timeout_ms = timeout_ms;
}

uint32_t Unblock::dnsProxyTimeout() const
{
	return _dns_proxy_timeout_ms;
}

std::string Unblock::dnsProxyStatus() const
{
	auto&			   ipc = IPCSignals::get();
	std::ostringstream ss;
	ss << "queries=" << ipc.getLatestU32("dns.queries").value_or(0) << "\n";
	ss << "cache_hits=" << ipc.getLatestU32("dns.cache_hits").value_or(0) << "\n";
	ss << "errors=" << ipc.getLatestU32("dns.errors").value_or(0) << "\n";
	return ss.str();
}

bool Unblock::dnsProxyTestUpstream(const std::string& value, std::string& output)
{
	const auto in_path	= Core::get().tempPath() / "unblock_dns_test.in";
	const auto out_path = Core::get().tempPath() / "unblock_dns_test.out";

	{
		File in{ false };
		in.open(in_path, "", true);
		in.clear();
		in.writeText(value);
		in.close();
	}

	std::error_code ec;
	std::filesystem::remove(out_path, ec);

	std::string bootstrap_csv;
	const auto& bootstrap = _dns_proxy_bootstrap.empty() ? defaultDnsProxyBootstrap() : _dns_proxy_bootstrap;
	for (const auto& b : bootstrap)
		bootstrap_csv += (bootstrap_csv.empty() ? "" : ",") + b;

	const bool ok = _runHidden(
		{ (Core::get().binPath() / "unblock_dns.exe").string(),
		  "--test-upstream-file",
		  in_path.string(),
		  "--bootstrap",
		  bootstrap_csv,
		  "--result",
		  out_path.string() },
		15'000
	);

	std::ifstream result{ out_path, std::ios::binary };
	if (result)
	{
		std::ostringstream ss;
		ss << result.rdbuf();
		output = ss.str();
	}
	else
	{
		output = ok ? "OK" : "FAIL: unblock_dns produced no result";
	}

	return output.starts_with("OK");
}

void Unblock::dnsProxyRepairBoot()
{
	if (_dns_proxy.isRun())
		return;

	const auto backup_path = _dnsProxyBackupPath();
	if (!std::filesystem::exists(backup_path))
		return;

	// All adapter work lives in the wrapper (DLL API), the engine never
	// touches the registry itself.
	if (_runHidden({ (Core::get().binPath() / "unblock_dns.exe").string(), "--repair", "--backup", backup_path.string() }, 15'000))
		Debug::warning("DNS proxy was killed without restore, adapters repaired.");
	else
		Debug::warning("DNS proxy adapter restore failed, adapters may still point at the local proxy.");
}

constexpr static std::string_view proxy_secret{ "dd92bc05d4dc4f4bef9cb4b7bf5628c5" };

void Unblock::localProxyTg(bool run)
{
	if (run)
	{
		_tg_ws_proxy.remove();
		_tg_ws_proxy.setDescription("Local proxy telegram.");
		_tg_ws_proxy.setArgs(
			{ (Core::get().binariesPath() / "tg-ws-proxy.exe").string(),
			  std::string{ "--secret " } + proxy_secret.data(),
			  "--dc-ip 1:" + _tg_dc_ip[0] + " --dc-ip 2:" + _tg_dc_ip[1] + " --dc-ip 3:" + _tg_dc_ip[2] + " --dc-ip 4:" + _tg_dc_ip[3],
			  //		  "--cfproxy-worker-domain " + _tg_cfproxy_domain,
			  "--host " + _tg_host,
			  "--port " + _tg_port }
		);
		_tg_ws_proxy.create();
		_tg_ws_proxy.start();
		return;
	}

	_tg_ws_proxy.remove();
}

void Unblock::setTgProxyParams(std::string_view host, std::string_view port, std::array<std::string, 4> dc_ip, std::string_view cfproxy_worker_domain)
{
	_tg_host		   = host;
	_tg_port		   = port;
	_tg_dc_ip		   = std::move(dc_ip);
	_tg_cfproxy_domain = cfproxy_worker_domain;
}

bool Unblock::localProxyTgIsRun()
{
	return _tg_ws_proxy.isRun();
}

void Unblock::localProxyTgLinkRun()
{
	Core::get().addTask(
		[this]
		{
			std::string tg{ "tg://proxy?server=" };
			tg.append(_tg_host);
			tg.append("&port=");
			tg.append(_tg_port);
			tg.append("&secret=");
			tg.append(proxy_secret);

			// ShellExecuteA forwards '&' in the URL verbatim, whereas
			// system("start ...") routes the link through cmd.exe, which
			// interprets '&' as a command separator and truncates the URL.
			ShellExecuteA(nullptr, "open", tg.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
	);
}

void Unblock::removeService()
{
	_tcpTimestampRestore();
	_helper_seen.clear();
	_helper_checking.clear();
	_helper_errors.clear();
	_helper_valid.clear();
	_helper_exhausted.clear();
	_zapret1_engine.remove();
	_zapret2_engine.remove();
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
	_zapret1_engine.stop();
	_zapret2_engine.stop();
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
		_zapret2_engine.stop();
	else
		_zapret1_engine.stop();

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
			std::string list = "LIST:";
			for (auto& host : hosts)
			{
				list += host;
				list += ':';
			}

			list.pop_back();
			sendHelperUdp(list);
		}
	}
}
