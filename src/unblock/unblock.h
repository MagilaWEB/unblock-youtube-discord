#pragma once
#include "strategies_dpi.h"
#include "domain_testing.h"
#include "dns_host.h"
#include "ipc_signals.h"

#include <unordered_set>

#include "../core/service.h"

class UNBLOCK_API Unblock final : public std::enable_shared_from_this<Unblock>
{
    Service _zapret{"zapret2", "SvcHost.exe"};
	Service _zapret_helper{ "zapret2_helper", "SvcHost.exe" };
    Service _tg_ws_proxy{"TgWsProxy", "SvcHost.exe"};
    Service _win_divert{"WinDivert"};

    DomainTesting _domain_testing;
    StrategiesDPI _strategies_dpi;
    DNSHost _dns_hosts;

    std::list<std::string> _section_opt_service_names{};
    u32 _strategy{};
    std::atomic_bool _zapret_dbg_run_end;
    std::atomic_bool _zapret_dbg_run;

    // Accessed only from the JS thread (via Ui::jsUpdate)
    std::unordered_set<std::string> _helper_checking;
    std::unordered_set<std::string> _helper_seen;
    std::unordered_map<std::string, std::string> _helper_errors;
    std::unordered_map<std::string, std::string> _helper_valid;

public:
    Unblock();

    bool testUrl(std::string_view str_url);

    bool automaticallyStrategy();

    void serviceConfigFile(const std::shared_ptr<File>& config);

    void changeStrategy(std::string_view name_config);
    void changeDirVersionStrategy(std::string_view dir_version);

    void addOptionalStrategies(std::string_view name);
    void removeOptionalStrategies(std::string_view name);
    void clearOptionalStrategies();

    bool runTest();

    std::string getNameStrategies();
    const std::vector<std::string>& getStrategies();

    const std::vector<std::string>& getStrategiesList();
    std::list<Service>& getConflictingServices();

    void startService();
    void stopService();
    void removeService();
    bool activeService();

    std::vector<std::string> helperCheckingHosts();
    std::vector<std::string> helperSeenHosts();
    std::vector<std::pair<std::string, std::string>> helperErrorStrategies();
    std::vector<std::pair<std::string, std::string>> helperValidHosts();

    std::vector<std::string> listVersionStrategy();

    void dnsHosts(bool state);
    void dnsHostsUpdate();
    void dnsHostsCancelUpdate();
    float dnsHostsUpdateProgress() const;
    bool dnsHostsCheck() const;
    const std::list<std::string>& dnsHostsListName();

    void localProxyTg(bool run = true);
    bool localProxyTgIsRun();
    void localProxyTgLinkRun();

    void testingDomain(
        std::function<void(std::string_view, bool)>&& callback = [](std::string_view, bool)
        {
        }, bool base_test = true
    );
    void testingDomainCancel();

    std::optional<std::string> checkUpdate() const;
    bool appUpdate();
    float appUpdateProgress() const;

    u32 domainSuccessRate() const;
    bool validDomain() const;
};
