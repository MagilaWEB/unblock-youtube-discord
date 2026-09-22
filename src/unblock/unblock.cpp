#include "unblock.h"
#include "../engine/version.hpp"
#include "domain_testing.h"
#include "ipc_signals.h"
#include <bit7z/bitfileextractor.hpp>
#include <curl/curl.h>

#include <shellapi.h>

#include <filesystem>
#include <string>

Unblock::Unblock()
{
	(void)IPCSignals::get();
	_zapret_helper.open();
	_win_divert.open();
	_tg_ws_proxy.open();
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

std::list<Service>& Unblock::getConflictingServices()
{
	static std::list<Service> conflicting_service;

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

static HttpsLoad& getLoad7z()
{
	static HttpsLoad load{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest/download/unblock.7z" };
	return load;
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

	if (!getLoad7z().run_to_file(archive))
		return false;

	const u32 code = getLoad7z().codeResult();
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
	return getLoad7z().progress();
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

bool Unblock::_dropExpiredHelperStates(std::chrono::steady_clock::time_point now)
{
	if ((now - _helper_last_signal) > c_helper_signal_ttl)
	{
		_helper_checking.clear();
		_helper_seen.clear();
		_helper_errors.clear();
		_helper_valid.clear();
		return true;
	}
	return false;
}

std::vector<std::string> Unblock::helperCheckingHosts()
{
	auto&	   ipc = IPCSignals::get();
	const auto now = std::chrono::steady_clock::now();

	while (auto host = ipc.getString("helper_checking"))
	{
		_helper_last_signal = now;
		std::string name	= std::move(*host);
		// Sync: one host lives in a single list (seen stays out of the sync).
		_helper_errors.erase(name);
		_helper_valid.erase(name);
		_helper_checking.insert(std::move(name));
	}

	while (auto host = ipc.getString("helper_done"))
	{
		_helper_last_signal = now;
		_helper_checking.erase(*host);
	}

	if (_dropExpiredHelperStates(now))
		return {};

	return { _helper_checking.begin(), _helper_checking.end() };
}

std::vector<std::string> Unblock::helperSeenHosts()
{
	auto&	   ipc = IPCSignals::get();
	const auto now = std::chrono::steady_clock::now();

	while (auto host = ipc.getString("helper_seen"))
	{
		_helper_last_signal = now;
		_helper_seen.insert(std::move(*host));
	}

	if (_dropExpiredHelperStates(now))
		return {};

	return { _helper_seen.begin(), _helper_seen.end() };
}

std::vector<std::pair<std::string, std::string>> Unblock::helperErrorHosts()
{
	auto&	   ipc = IPCSignals::get();
	const auto now = std::chrono::steady_clock::now();

	auto entry = ipc.getString("helper_error");

	if (entry)
	{
		_helper_last_signal = now;
		_helper_errors.clear();

		do
		{
			const auto pos = entry->rfind(':');
			if (pos != std::string::npos)
			{
				const auto host = entry->substr(0, pos);
				// Sync: evict from the sibling lists (seen stays out of the sync).
				_helper_checking.erase(host);
				_helper_valid.erase(host);
				_helper_errors[host] = entry->substr(pos + 1);
			}
			_helper_last_signal = now;
		} while ((entry = ipc.getString("helper_error")));
	}

	if (_dropExpiredHelperStates(now))
		return {};

	std::vector<std::pair<std::string, std::string>> result;
	result.reserve(_helper_errors.size());
	for (const auto& [host, strategy] : _helper_errors)
		result.emplace_back(host, strategy);

	return result;
}

std::vector<std::pair<std::string, std::string>> Unblock::helperValidHosts()
{
	auto&	   ipc = IPCSignals::get();
	const auto now = std::chrono::steady_clock::now();

	auto entry = ipc.getString("helper_valid");

	if (entry)
	{
		_helper_last_signal = now;
		_helper_valid.clear();

		do
		{
			const auto pos = entry->rfind(':');
			if (pos != std::string::npos)
			{
				const auto host = entry->substr(0, pos);
				// Sync: evict from the sibling lists (seen stays out of the sync).
				_helper_checking.erase(host);
				_helper_errors.erase(host);
				_helper_valid[host] = entry->substr(pos + 1);
			}
			_helper_last_signal = now;
		} while ((entry = ipc.getString("helper_valid")));
	}

	if (_dropExpiredHelperStates(now))
		return {};

	std::vector<std::pair<std::string, std::string>> result;
	result.reserve(_helper_valid.size());
	for (const auto& [host, strategy] : _helper_valid)
		result.emplace_back(host, strategy);

	return result;
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

float Unblock::dnsHostsUpdateProgress() const
{
	return _dns_hosts.percentageCompletion();
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
	_helper_seen.clear();
	_helper_checking.clear();
	_helper_errors.clear();
	_helper_valid.clear();
	_zapret1_engine.remove();
	_zapret2_engine.remove();
	_zapret_helper.remove();
	_win_divert.remove();
}

void Unblock::stopService()
{
	_helper_seen.clear();
	_helper_checking.clear();
	_helper_errors.clear();
	_helper_valid.clear();
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
		auto		list_host = _domain_testing.listHost();
		std::string list	  = "LIST:";

		if (!list_host.empty())
		{
			for (auto& line : list_host)
			{
				auto host = [&]() -> std::string
				{
					std::smatch m;
					return std::regex_search(line, m, std::regex{ R"(://([^/?#]+))" }) && m.size() > 1 ? m[1].str() : "";
				}();

				list += host;
				list += ':';
			}

			list.pop_back();
			sendHelperUdp(list);
		}
	}
}
