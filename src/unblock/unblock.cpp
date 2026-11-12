module;

#include "../core/macros.hpp"

module unblock;

import ext.windows;
import core;
import core.debug;
import core.file_system;
import core.hidden_process;
import core.localization;
import core.service;
import core.service_diagnostics;
import ext.curl;
import unblock.constants;
import unblock.dns_host;
import unblock.dns_proxy;
import unblock.domain_testing;
import unblock.helper_state;
import unblock.ipc_signals;
import unblock.zapret1_engine;
import unblock.zapret2_engine;
import unblock.zapret_engine;

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
	if (std::ranges::find(_section_opt_service_names, name) != _section_opt_service_names.end())
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
	// The retry/exhausted coordination over UDP 9999 (helper snapshots)
	// exists only in Zapret2. With Zapret1 running the test must behave
	// exactly like with everything stopped: a plain single-attempt curl per
	// host.
	_domain_testing->test(base_test, [callback](std::string_view url, bool state) { callback(url, state); }, _zapret2_engine->isRun());

	_domain_testing->printTestInfo();
}

void Unblock::testingDomainCancel()
{
	_domain_testing->cancelTesting();
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

bool Unblock::vpnPaused()
{
	// The helper re-asserts this key every couple of seconds, so its freshness
	// is the liveness proof. Asking the SCM instead made the pause flicker off:
	// Service::isRun() falls back to a cached "not running" while its lock is
	// busy, and one such read hid a real pause from the UI.
	auto& ipc = IPCSignals::get();

	auto age = ipc.latestAge("zapret.vpn_paused");
	if (!age || *age > std::chrono::seconds{ 30 })
		return false;

	return ipc.getLatestU32("zapret.vpn_paused").value_or(0) != 0;
}

std::vector<std::string> Unblock::helperCheckingHosts()
{
	return _helper.checkingHosts();
}

std::vector<std::string> Unblock::helperSeenHosts()
{
	return _helper.seenHosts();
}

std::vector<std::string> Unblock::helperUnjudgedHosts()
{
	return _helper.unjudgedHosts();
}

std::vector<std::pair<std::string, std::string>> Unblock::helperErrorHosts()
{
	return _helper.errorHosts();
}

std::vector<std::pair<std::string, std::string>> Unblock::helperValidHosts()
{
	return _helper.validHosts();
}

std::vector<std::pair<std::string, std::string>> Unblock::helperExhaustedHosts()
{
	return _helper.exhaustedHosts();
}

HelperStats Unblock::helperStats()
{
	return _helper.stats();
}

namespace
{
	float parseRate(std::string_view text)
	{
		float value{};
		const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (ec != std::errc{} || ptr != text.data() + text.size())
			return 0.F;

		return value;
	}

	// A diagnostic field holds paragraphs separated by newlines; every line is
	// logged on its own so the log does not end up as one giant entry.
	std::vector<std::string_view> splitLines(std::string_view text)
	{
		std::vector<std::string_view> lines;
		for (size_t start = 0; start <= text.size();)
		{
			const size_t		   end	= text.find('\n', start);
			const std::string_view line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			if (!line.empty())
				lines.push_back(line);

			if (end == std::string_view::npos)
				break;

			start = end + 1;
		}
		return lines;
	}
}	 // namespace

ZapretRate Unblock::zapretRateBps()
{
	auto raw = IPCSignals::get().getLatest("zapret_rate");
	if (!raw)
		return {};

	const auto sep = raw->find('|');
	if (sep == std::string::npos)
		return {};

	const std::string_view text{ *raw };
	ZapretRate			   out{};
	out.down = parseRate(text.substr(0, sep));
	out.up	 = parseRate(text.substr(sep + 1));
	return out;
}

std::vector<std::string> Unblock::testHostNames()
{
	std::vector<std::string> hosts;
	for (auto& line : _domain_testing->listHost())
	{
		std::smatch m;
		if (!std::regex_search(line, m, std::regex{ R"(://([^/?#]+))" }) || m.size() <= 1)
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

DNSHost& Unblock::dnsHosts()
{
	return *_dns_hosts;
}

void Unblock::setDnsEnabled(bool state)
{
	if (state)
	{
		_dns_hosts->update();
		_dns_hosts->enable();
		runDnsProxy(true);
		return;
	}

	runDnsProxy(false);
	_dns_hosts->disable();
}

void Unblock::runDnsProxy(bool state)
{
	_dns_proxy.run(state);
	if (state)
		_buildServiceReport({ &_dns_proxy.service() });
}

void Unblock::runTgProxy(bool state)
{
	_tg_proxy.run(state);
	if (state)
		_buildServiceReport({ &_tg_proxy.service() });
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

void Unblock::removeService()
{
	_tcpTimestampRestore();
	_helper.clear();
	_zapret1_engine->remove();
	_zapret2_engine->remove();
	_zapret_helper.remove();
	_win_divert.remove();
}

void Unblock::stopService()
{
	_tcpTimestampRestore();
	_helper.clear();
	_zapret1_engine->stop();
	_zapret2_engine->stop();
	_zapret_helper.stop();
}

void Unblock::pushHelperConfig() const
{
	_helper.pushConfig();
}

void Unblock::startService(Technology technology)
{
	_helper.clear();

	// Exactly one technology runs at a time.
	if (technology == Technology::Zapret1)
		_zapret2_engine->stop();
	else
		_zapret1_engine->stop();

	_zapret_helper.remove();

	auto& eng = engine(technology);
	if (eng.strategies().empty())
		return;

	// The helper understands the zapret2 protocol (Lua auto_strategy over
	// --lua-desync). Under Zapret1 there is nothing to pump, so it is started
	// with --vpn-only: it only watches for a VPN tunnel and pauses the engines
	// while one is up, then brings back exactly those it found running.
	const std::string		 helper_exe = (Core::get().binPath() / "zapret_helper.exe").string();
	std::vector<std::string> helper_args{ helper_exe };
	if (technology == Technology::Zapret1)
		helper_args.emplace_back("--vpn-only");

	_zapret_helper.setDescription(Localization::Str{ "str_service_zapret_description" }());
	_zapret_helper.setArgs(std::move(helper_args));
	_zapret_helper.create();
	_zapret_helper.start();

	// ts/tcp_ts fooling needs OS TCP timestamps, which Windows ships off.
	// Enable them only when the effective strategy uses it, and remember it
	// so stopService/removeService can put the system back.
	_tcpTimestampSync(technology);

	eng.start();

	_refreshServiceReport(technology);

	if (technology != Technology::Zapret2)
		return;

	// Fresh settings first: the on-disk setting.config is stale while
	// unblock runs (File::save on close), so the helper cannot rely on
	// reading the file at startup. Then the domain list, both with retries
	// for the helper bind race.
	pushHelperConfig();

	// send domain list to zapret-helper
	_helper.sendHostList(testHostNames());
}

void Unblock::_buildServiceReport(std::initializer_list<Service*> services)
{
	std::vector<ServiceFailure> failures;

	// Only the services this attempt actually started are judged: a service
	// that is running started cleanly, and a service that never started keeps
	// an older failure snapshot (even a speculative Service::open() writes
	// one). Reporting that would show a failure from a previous run under a
	// successful start. The driver state still reaches the report as evidence
	// inside diagnose().
	for (Service* service : services)
		if (!service->isRun())
			if (const auto& failure = service->getLastFailure(); failure.valid())
				failures.push_back(failure);

	// A clean start drops the previous report only when it belongs to one of
	// the services of this attempt. Starting the DNS proxy (or the Telegram
	// one) cleanly must not erase a still valid engine failure and silently
	// turn the engine's status line green; a report about other services is
	// left untouched.
	if (failures.empty())
	{
		CRITICAL_SECTION_RAII(_service_report_lock);
		if (_reportCoversAnyLocked(services))
			_resetServiceReportLocked();

		return;
	}

	DiagnosticReport report = ServiceDiagnostics::diagnose(failures, true);
	if (report.empty())
	{
		// Same rule as above: nothing to show about these services does not
		// mean the report about the others became stale.
		CRITICAL_SECTION_RAII(_service_report_lock);
		if (_reportCoversAnyLocked(services))
			_resetServiceReportLocked();

		return;
	}

	// The reason has to survive into the log tail that goes into a bug report.
	// One entry per line, so the log does not become a single giant record:
	// verdict, summary and recommendations at warning level (error would
	// append a stack trace to every line), raw data at info.
	Debug::warning("service diagnostics: verdict: {}", report.verdict);
	for (const auto& line : splitLines(report.summary))
		Debug::warning("service diagnostics: summary: {}", line);
	for (const auto& recommendation : report.recommendations)
		Debug::warning("service diagnostics: recommendation: {}", recommendation);
	for (const auto& line : report.raw)
		Debug::info("service diagnostics: raw: {}", line);

	// The full report goes to the attached file: verdict, summary, checklist,
	// recommendations and the raw evidence. The issue body carries only the
	// minimum (verdict + summary) and asks the user to attach this file.
	// Written next to the log and overwritten by every new report, like
	// logs/strategy.args.txt.
	File report_file{ false };
	report_file.open(Core::get().currentPath() / "logs" / "service-report", ".txt", true);
	report_file.clear();
	report_file.writeText(report.fullText());
	report_file.close();
	Debug::info(Localization::Str{ "str_service_report_dump" }(), report_file.getPath().string());

	CRITICAL_SECTION_RAII(_service_report_lock);
	_service_report = std::move(report);
	_service_report_names.clear();
	for (Service* service : services)
		_service_report_names.emplace_back(service->getName());
	++_service_report_id;
}

void Unblock::_refreshServiceReport(Technology technology)
{
	if (technology == Technology::Zapret2)
	{
		_buildServiceReport({ &engine(technology).service(), &_zapret_helper });
		return;
	}

	_buildServiceReport({ &engine(technology).service() });
}

bool Unblock::hasServiceReport() const
{
	CRITICAL_SECTION_RAII(_service_report_lock);
	return _service_report.has_value();
}

DiagnosticReport Unblock::lastServiceReport() const
{
	CRITICAL_SECTION_RAII(_service_report_lock);
	return _service_report.value_or(DiagnosticReport{});
}

u64 Unblock::lastServiceReportId() const
{
	CRITICAL_SECTION_RAII(_service_report_lock);
	return _service_report_id;
}

void Unblock::clearServiceReport()
{
	CRITICAL_SECTION_RAII(_service_report_lock);
	_resetServiceReportLocked();
}

bool Unblock::_reportCoversAnyLocked(std::initializer_list<Service*> services) const
{
	// _service_report_lock is held by the caller.
	return std::ranges::any_of(
		services,
		[this](const Service* service) { return std::ranges::find(_service_report_names, service->getName()) != _service_report_names.end(); }
	);
}

void Unblock::_resetServiceReportLocked()
{
	_service_report.reset();
	_service_report_names.clear();
	_service_report_id = 0;
}

bool Unblock::serviceReportCovers(std::string_view service_name) const
{
	CRITICAL_SECTION_RAII(_service_report_lock);
	return std::ranges::any_of(_service_report_names, [service_name](const std::string& name) { return name == service_name; });
}
