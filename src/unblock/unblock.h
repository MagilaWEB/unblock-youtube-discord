#pragma once
#include "zapret1_engine.h"
#include "zapret2_engine.h"
#include "domain_testing.h"
#include "dns_host.h"
#include "ipc_signals.h"

#include <cctype>
#include <charconv>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_set>

#include "../core/service.h"

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

/** Helper pool load snapshot ("queued:in_check:known"). Header-inline so
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
	size_t		pos = 0;
	size_t*		fields[3]{ &out.queued, &out.in_check, &out.known };
	for (int i = 0; i < 3; ++i)
	{
		const size_t end = (i < 2) ? text.find(':', pos) : std::string_view::npos;
		if (i < 2 && end == std::string_view::npos)
			return std::nullopt;
		unsigned long long num{};
		const auto		   part = text.substr(pos, (i < 2) ? end - pos : std::string_view::npos);
		const auto [ptr, ec]	= std::from_chars(part.data(), part.data() + part.size(), num);
		if (ec != std::errc{} || ptr != part.data() + part.size())
			return std::nullopt;
		*fields[i] = static_cast<size_t>(num);
		pos		   = end + 1;
	}
	return out;
}

class Unblock final : public std::enable_shared_from_this<Unblock>
{
public:
	struct DnsProxyUpstream
	{
		bool		enabled{ true };
		std::string name;
		std::string address;
		std::string bootstrap;
	};

	static std::vector<DnsProxyUpstream> defaultDnsProxyUpstreams();

private:
	Zapret1Engine _zapret1_engine;
	Zapret2Engine _zapret2_engine;

	Service _zapret_helper{ "zapret2_helper", "SvcHost.exe" };
	Service _tg_ws_proxy{ "TgWsProxy", "SvcHost.exe" };
	Service _dns_proxy{ "unblock_dns", "SvcHost.exe" };
	Service _win_divert{ "WinDivert" };

	std::vector<DnsProxyUpstream> _dns_proxy_upstreams;

	DomainTesting _domain_testing;
	DNSHost		  _dns_hosts;

	std::list<std::string> _section_opt_service_names{};

	std::string				   _tg_host{ "127.0.0.1" };
	std::string				   _tg_port{ "9101" };
	std::array<std::string, 4> _tg_dc_ip{ "149.154.175.50", "91.105.192.100", "149.154.175.100", "149.154.167.91" };
	std::string				   _tg_cfproxy_domain{ "unblock.kermanua1488.workers.dev" };

	// Fresh [HELPER] message (UDP CONFIG:...) from the UI. The on-disk
	// setting.config is stale while unblock runs (File::save on close),
	// so startService() pushes this instead of letting the helper read it.
	std::string _helper_config_message{};

	// Accessed only from the JS thread (via Ui::update)
	// The checking/error/valid/exhausted lists are mutually exclusive: one
	// host lives in exactly one of them (seen stays out of the sync). A
	// single timestamp tracks the last consumed helper signal; after
	// c_helper_signal_ttl of total silence every list is dropped, so the
	// UI never shows dead hosts.
	static constexpr auto						 c_helper_signal_ttl{ std::chrono::seconds(5) };
	std::chrono::steady_clock::time_point		 _helper_last_signal{};
	std::unordered_set<std::string>				 _helper_checking;
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

	// Drops every helper list if the helper stayed silent past the TTL.
	// Returns true when expired (all lists are empty afterwards).
	bool _dropExpiredHelperStates(std::chrono::steady_clock::time_point now);

	std::filesystem::path _dnsProxyConfigPath() const;
	std::filesystem::path _dnsProxyStatusPath() const;
	std::filesystem::path _dnsProxyBackupPath() const;
	std::filesystem::path _dnsProxyLogPath() const;
	void				  _dnsProxyWriteConfig(const std::vector<DnsProxyUpstream>& upstreams);
	/** Spawns unblock_dns.exe without a shell and waits up to timeout_ms.
	 *  The upstream value is passed via a file, never on the command line,
	 *  so '|' and quoting can't be mangled by cmd. */
	bool				  _dnsProxyRunHelper(const std::vector<std::string>& args, uint32_t timeout_ms);

public:
	Unblock();

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
	std::list<Service>&				getConflictingServices();

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

	void								 dnsProxy(bool state);
	bool								 dnsProxyIsRun();
	void								 setDnsProxyUpstreams(std::vector<DnsProxyUpstream> upstreams);
	const std::vector<DnsProxyUpstream>& dnsProxyUpstreams() const;
	/** Raw status file content (empty when the proxy never ran). */
	std::string							 dnsProxyStatus() const;
	/** Runs unblock_dns --test-upstream, output holds OK/FAIL text. */
	bool								 dnsProxyTestUpstream(const std::string& value, std::string& output);
	/** Restores adapters left on 127.0.0.1 by a killed proxy run. */
	void								 dnsProxyRepairBoot();

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
