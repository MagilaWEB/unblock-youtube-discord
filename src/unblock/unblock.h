#pragma once
#include "zapret_engine.h"
#include "app_update.h"

#include <cctype>
#include <charconv>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../core/service.h"

class Zapret1Engine;
class Zapret2Engine;
class DomainTesting;
class DNSHost;

/** Passive autopick round (Zapret2): pure rules over helper verdict sets.
 *  No curl here — verdicts arrive from live traffic (helper probes every
 *  LIST host through the running desync, lua marks fully-tried hosts
 *  exhausted). Header-inline so unit tests link without the engine. */
inline bool autoRoundSettled(
	const std::unordered_set<std::string>& expected, const std::unordered_set<std::string>& valid, const std::unordered_set<std::string>& exhausted
)
{
	if (expected.empty())
		return false;
	for (const auto& h : expected)
		if (!valid.contains(h) && !exhausted.contains(h))
			return false;
	return true;
}

/** Win when at most ~10% of expected hosts are dead. Empty set never wins. */
inline bool judgeAutoRound(size_t dead, size_t total)
{
	return total > 0 && dead * 10 <= total;
}

/** Hostnames the helper can verdict (mirrors helper _isValidHost). */
inline bool isHelperHostName(std::string_view host)
{
	return std::ranges::any_of(host, [](char ch) { return std::isalpha(static_cast<unsigned char>(ch)); });
}

/** Helper pool load snapshot ("queued|in_check|known"). Header-inline so
 *  unit tests link without the engine. */
struct HelperStats
{
	size_t queued{ 0 };
	size_t in_check{ 0 };
	size_t known{ 0 };
};

inline std::optional<HelperStats> parseHelperStats(std::string_view text)
{
	HelperStats out{};
	size_t*		fields[3]{ &out.queued, &out.in_check, &out.known };
	size_t		index = 0;

	for (auto part_range : text | std::views::split('|'))
	{
		if (index >= 3)
			return std::nullopt;

		const std::string_view part{ std::ranges::data(part_range), std::ranges::size(part_range) };

		unsigned long long num{};
		const auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), num);
		if (ec != std::errc{} || ptr != part.data() + part.size())
			return std::nullopt;

		*fields[index++] = static_cast<size_t>(num);
	}

	if (index != 3)
		return std::nullopt;

	return out;
}

/** True when any strategy line toggles TCP timestamps: zapret1 `fooling=ts`
 *  or zapret2 `tcp_ts`/`tcp_ts_up`. Windows disables timestamps by default,
 *  so such strategies silently degrade unless the system enables them.
 *  Header-inline so unit tests link without the engine. */
inline bool strategyUsesTcpTimestamps(const std::vector<std::string>& strategies)
{
	for (const auto& line : strategies)
	{
		if (line.contains("tcp_ts"))
			return true;

		const auto pos = line.find("fooling=");
		if (pos == std::string::npos)
			continue;

		const auto value_begin = pos + 8;
		auto	   value_end   = line.find_first_of(" \t", value_begin);
		if (value_end == std::string::npos)
			value_end = line.size();

		const std::string_view value{ line.data() + value_begin, value_end - value_begin };

		if (std::ranges::any_of(
				value | std::views::split(','),
				[](auto token_range) { return std::string_view{ std::ranges::data(token_range), std::ranges::size(token_range) } == "ts"; }
			))
			return true;
	}

	return false;
}

/** In-flight helper checks with a short done grace. The helper reports a
 *  fast DONE edge while the terminal valid/error/exhausted verdict only
 *  arrives with the next snapshot, so removing a host on DONE alone makes
 *  the checking list blink. Recently finished hosts stay visible until a
 *  verdict arrives or the grace expires. Header-inline for unit tests. */
class HelperCheckingTracker
{
public:
	explicit HelperCheckingTracker(std::chrono::steady_clock::duration grace_ttl = std::chrono::seconds(1)) : _grace_ttl(grace_ttl) {}

	void checkingEdge(std::string host, std::chrono::steady_clock::time_point now)
	{
		(void)now;
		_active.insert(host);
		_grace.erase(host);
	}

	void doneEdge(std::string host, bool terminal, std::chrono::steady_clock::time_point now)
	{
		_active.erase(host);
		if (terminal)
		{
			_grace.erase(host);
			return;
		}

		_grace[std::move(host)] = now;
	}

	void verdict(std::string_view host)
	{
		const std::string name{ host };
		_active.erase(name);
		_grace.erase(name);
	}

	std::vector<std::string> visible(std::chrono::steady_clock::time_point now)
	{
		prune(now);

		std::vector<std::string> out;
		out.reserve(_active.size() + _grace.size());
		for (const auto& host : _active)
			out.emplace_back(host);
		for (const auto& [host, _] : _grace)
			if (!_active.contains(host))
				out.emplace_back(host);
		return out;
	}

	void clear()
	{
		_active.clear();
		_grace.clear();
	}

private:
	void prune(std::chrono::steady_clock::time_point now)
	{
		for (auto it = _grace.begin(); it != _grace.end();)
			if (now - it->second > _grace_ttl)
				it = _grace.erase(it);
			else
				++it;
	}

	std::unordered_set<std::string>										   _active;
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> _grace;
	std::chrono::steady_clock::duration									   _grace_ttl;
};

class Unblock final : public std::enable_shared_from_this<Unblock>
{
public:
	static std::vector<std::string> defaultDnsProxyUpstreams();
	static std::vector<std::string> defaultDnsProxyBootstrap();
	static uint32_t					defaultDnsProxyTimeout();

private:
	std::unique_ptr<Zapret1Engine> _zapret1_engine;
	std::unique_ptr<Zapret2Engine> _zapret2_engine;

	Service _zapret_helper{ "zapret2_helper", "SvcHost.exe" };
	Service _tg_ws_proxy{ "TgWsProxy", "SvcHost.exe" };
	Service _dns_proxy{ "unblock_dns", "SvcHost.exe" };
	Service _win_divert{ "WinDivert" };

	std::vector<std::string> _dns_proxy_upstreams;
	std::vector<std::string> _dns_proxy_bootstrap;
	uint32_t				 _dns_proxy_timeout_ms{ 0 };

	// Guards "we enabled TCP timestamps" across start/stop worker threads.
	std::mutex _tcp_timestamp_lock;
	bool	   _tcp_timestamps_owned{ false };

	// Self-update: version check + download orchestration.
	AppUpdater _updater;

	std::unique_ptr<DomainTesting> _domain_testing;
	std::unique_ptr<DNSHost>	   _dns_hosts;

	std::list<std::string> _section_opt_service_names{};

	std::string				   _tg_host{ "127.0.0.1" };
	std::string				   _tg_port{ "9101" };
	std::array<std::string, 4> _tg_dc_ip{ "149.154.175.50", "91.105.192.100", "149.154.175.100", "149.154.167.91" };
	std::string				   _tg_cfproxy_domain{ "unblock.kermanua1488.workers.dev" };

	// Fresh [HELPER] message (UDP CONFIG:...) from the UI. The on-disk
	// setting.config is stale while unblock runs (File::save on close),
	// so startService() pushes this instead of letting the helper read it.
	std::string _helper_config_message{};

	// Rebuilt from helper IPC snapshots. The passive autopick worker and the
	// JS-thread Ui::update() both read/rebuild these, so all access is
	// serialized by _helper_state_lock.
	// The checking/error/valid/exhausted lists are mutually exclusive: one
	// host lives in exactly one of them (seen stays out of the sync). Checking
	// keeps a short done grace so a host does not blink out before its verdict
	// snapshot arrives. The helper_seen snapshot age drives the TTL: after
	// c_helper_signal_ttl of total silence every list is dropped, so the UI
	// never shows dead hosts.
	static constexpr auto						 c_helper_signal_ttl{ std::chrono::seconds(5) };
	HelperCheckingTracker						 _helper_checking;
	std::unordered_set<std::string>				 _helper_seen;
	std::unordered_map<std::string, std::string> _helper_errors;
	std::unordered_map<std::string, std::string> _helper_valid;
	// Fully-tried hosts relayed by the helper (lua wrapped a whole plan).
	// Terminal verdict like valid, but means "nothing works": autopick
	// fast-fails these without burning curl timeouts.
	std::unordered_map<std::string, std::string> _helper_exhausted;
	// Last pool load snapshot (queued/in-flight/known). Refreshed by the
	// helper_stats broadcast, zeroed with everything else on TTL expiry.
	HelperStats									 _helper_stats{};

	// Guards every helper-state container above (Ui::update() and the
	// passive autopick worker both touch them).
	std::mutex _helper_state_lock;

	// Drops every helper list when the newest helper_seen snapshot is older
	// than the TTL. Returns true when expired (all lists are empty after).
	bool _dropExpiredHelperStates();

	std::filesystem::path _dnsProxyConfigPath() const;
	std::filesystem::path _dnsProxyBackupPath() const;
	std::filesystem::path _dnsProxyLogPath() const;
	void _dnsProxyWriteConfig(const std::vector<std::string>& upstreams, const std::vector<std::string>& bootstrap, uint32_t timeout_ms);
	/** Spawns unblock_dns.exe without a shell and waits up to timeout_ms.
	 *  The upstream value is passed via a file, never on the command line. */
	bool _runHidden(const std::vector<std::string>& args, uint32_t timeout_ms);

	/** Enables TCP timestamps when the strategy needs ts/tcp_ts and they are
	 *  off, remembering that we changed the system. */
	void _tcpTimestampSync(Technology technology);
	/** Reverts TCP timestamps if this run enabled them. */
	void _tcpTimestampRestore();

public:
	Unblock();
	~Unblock();

	bool testUrl(std::string_view str_url);

	ZapretEngine& engine(Technology technology);

	bool automaticallyStrategy(Technology technology);

	void serviceConfigFile(const std::shared_ptr<File>& config);

	void changeStrategy(Technology technology, std::string_view name_config);
	void changeDirVersionStrategy(Technology technology, std::string_view dir_version);

	void					 changeFakeKey(Technology technology, std::string_view key);
	std::vector<std::string> fakeBinKeys(Technology technology);
	std::string				 fakeBinKey(Technology technology);

	void addOptionalStrategies(std::string_view name);
	void removeOptionalStrategies(std::string_view name);
	void clearOptionalStrategies();

	/** True when at least one service is enabled (something to bypass). */
	bool hasOptionalStrategies() const { return !_section_opt_service_names.empty(); }

	void setCustomLists(
		std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
	);

	bool runTest();

	std::string						getNameStrategies(Technology technology);
	const std::vector<std::string>& getStrategies(Technology technology);

	const std::vector<std::string>& getStrategiesList(Technology technology);
	std::list<Service>				getConflictingServices();

	void					  startService(Technology technology);
	void					  stopService();
	void					  removeService();
	bool					  activeService();
	bool					  isRun(Technology technology);
	std::optional<Technology> runningTechnology();

	/** Fresh [HELPER] payload from the UI (in-memory userConfig, not the
	 *  stale on-disk file). Sent as UDP CONFIG: right after the helper is
	 *  launched and on Apply while it is running. */
	void setHelperConfigMessage(std::string message) { _helper_config_message = std::move(message); }
	void pushHelperConfig() const;

	std::vector<std::string>						 helperCheckingHosts();
	std::vector<std::string>						 helperSeenHosts();
	std::vector<std::pair<std::string, std::string>> helperErrorHosts();
	std::vector<std::pair<std::string, std::string>> helperValidHosts();
	/** Fully-tried hosts: every strategy failed, nothing left to attempt. */
	std::vector<std::pair<std::string, std::string>> helperExhaustedHosts();

	/** Last helper pool load snapshot (zeros when the helper is silent). */
	HelperStats helperStats();

	/** Bare hostnames of the current domain_test lists (what LIST: carries).
	 *  Filtered to helper-verdictable names. */
	std::vector<std::string> testHostNames();

	std::vector<std::string> listVersionStrategy(Technology technology);

	void						  dnsHosts(bool state);
	void						  dnsHostsUpdate();
	void						  dnsHostsCancelUpdate();
	float						  dnsHostsDownloadProgress() const;
	bool						  dnsHostsCheck() const;
	const std::list<std::string>& dnsHostsListName();
	void						  setDnsHostsRegion(std::string_view region);
	const std::string&			  dnsHostsRegion() const;
	void						  setDnsHostsBaseUrl(std::string_view url);
	const std::string&			  dnsHostsBaseUrl() const;
	bool						  dnsHostsRegionAvailable(std::string_view region) const;

	void							dnsProxy(bool state);
	bool							dnsProxyIsRun();
	void							setDnsProxyUpstreams(std::vector<std::string> upstreams);
	const std::vector<std::string>& dnsProxyUpstreams() const;
	void							setDnsProxyBootstrap(std::vector<std::string> bootstrap);
	const std::vector<std::string>& dnsProxyBootstrap() const;
	void							setDnsProxyTimeout(uint32_t timeout_ms);
	uint32_t						dnsProxyTimeout() const;
	/** Status counters pushed by the proxy over IPC (zeros when unknown). */
	std::string						dnsProxyStatus() const;
	/** Runs unblock_dns --test-upstream, output holds OK/FAIL text. */
	bool							dnsProxyTestUpstream(const std::string& value, std::string& output);
	/** Restores adapters left on 127.0.0.1 by a killed proxy run. */
	void							dnsProxyRepairBoot();

	void localProxyTg(bool run = true);
	bool localProxyTgIsRun();
	void localProxyTgLinkRun();

	void setTgProxyParams(std::string_view host, std::string_view port, std::array<std::string, 4> dc_ip, std::string_view cfproxy_worker_domain);
	const std::string&				  tgProxyHost() const { return _tg_host; }
	const std::string&				  tgProxyPort() const { return _tg_port; }
	const std::array<std::string, 4>& tgProxyDcIp() const { return _tg_dc_ip; }
	const std::string&				  tgProxyCfproxyDomain() const { return _tg_cfproxy_domain; }

	void testingDomain(std::function<void(std::string_view, bool)>&& callback = [](std::string_view, bool) {}, bool base_test = true);
	void testingDomainCancel();

	std::optional<std::string> checkUpdate() const;
	bool					   appUpdate();
	float					   appUpdateProgress() const;

	u32	 domainSuccessRate() const;
	bool validDomain() const;
};
