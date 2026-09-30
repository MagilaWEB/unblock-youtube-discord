#pragma once
#include "zapret_engine.h"
#include "app_update.h"
#include "tg_proxy.h"
#include "dns_proxy.h"
#include "helper_state.h"

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

class Unblock final : public std::enable_shared_from_this<Unblock>
{
private:
	std::unique_ptr<Zapret1Engine> _zapret1_engine;
	std::unique_ptr<Zapret2Engine> _zapret2_engine;

	Service _zapret_helper{ "zapret2_helper", "SvcHost.exe" };
	Service _win_divert{ "WinDivert" };

	DnsProxy _dns_proxy;

	// Guards "we enabled TCP timestamps" across start/stop worker threads.
	std::mutex _tcp_timestamp_lock;
	bool	   _tcp_timestamps_owned{ false };

	// Self-update: version check + download orchestration.
	AppUpdater _updater;

	std::unique_ptr<DomainTesting> _domain_testing;
	std::unique_ptr<DNSHost>	   _dns_hosts;

	std::list<std::string> _section_opt_service_names{};

	TgProxy _tg_proxy;

	// Helper IPC aggregation: host verdict sets rebuilt from the snapshots
	// plus the CONFIG:/LIST: pushes to the helper process.
	HelperState _helper;

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
	void setHelperConfigMessage(std::string message) { _helper.setConfigMessage(std::move(message)); }
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

	/** DNS proxy subsystem (unblock_dns.exe). Consumers talk to it directly. */
	DnsProxy& dnsProxy() { return _dns_proxy; }

	/** Local Telegram WS proxy subsystem. Consumers talk to it directly. */
	TgProxy& tgProxy() { return _tg_proxy; }

	void testingDomain(std::function<void(std::string_view, bool)>&& callback = [](std::string_view, bool) {}, bool base_test = true);
	void testingDomainCancel();

	std::optional<std::string> checkUpdate() const;
	bool					   appUpdate();
	float					   appUpdateProgress() const;

	u32	 domainSuccessRate() const;
	bool validDomain() const;
};
