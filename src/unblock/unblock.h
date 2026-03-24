#pragma once
#include "strategies_dpi.h"
#include "domain_testing.h"
#include "dns_host.h"

#include "../core/service.h"

class UNBLOCK_API Unblock final
{
	Service _zapret{ "zapret", "winws.exe" };
	Service _win_divert{ "WinDivert" };

	DomainTesting	   _domain_testing;
	DomainTesting	   _domain_testing_video;
	DomainTesting	   _domain_testing_proxy{ true };
	DomainTesting	   _domain_testing_proxy_video{ true };
	StrategiesDPI	   _strategies_dpi;
	DNSHost			   _dns_hosts;

	std::list<std::string> _section_opt_service_names{};
	std::atomic_bool	   _zapret_dbg_run_end;
	std::atomic_bool	   _zapret_dbg_run;

	struct StrategyType
	{
		u32 type{ 0 };
		u32 fake_bin{ 0 };
	};

	StrategyType _strategy{};

public:
	Unblock();

	bool automaticallyStrategy();

	void serviceConfigFile(const std::shared_ptr<File>& config);

	void changeStrategy(std::string_view name_config, std::string_view name_fake_bin);
	void changeDirVersionStrategy(std::string_view dir_version);

	void addOptionalStrategies(std::string_view name);
	void removeOptionalStrategies(std::string_view name);
	void clearOptionalStrategies();

	bool runTest(bool video = false);

	std::string getNameStrategies();
	std::string getNameFakeBin();

	const std::vector<std::string>&							  getStrategiesList();
	const std::map<std::string, StrategiesDPI::FakeBinParam>& getFakeBinList();
	std::list<Service>&										  getConflictingServices();

	void startService();
	void stopService();
	void removeService();
	bool activeService();
	void checkStateServices(const std::function<void(std::string_view, bool)>& callback);

	std::vector<std::string> listVersionStrategy();

	void						  dnsHosts(bool state);
	void						  dnsHostsUpdate();
	void						  dnsHostsCancelUpdate();
	float						  dnsHostsUpdateProgress() const;
	bool						  dnsHostsCheck() const;
	const std::list<std::string>& dnsHostsListName();

	void testingDomain(
		std::function<void(std::string_view, bool)>&& callback = [](std::string_view, bool) {}, bool video = false, bool base_test = true
	);
	void testingDomainCancel(bool video = false);

	void maxWaitTesting(u32 second);

	std::optional<std::string> checkUpdate();
	bool					   appUpdate();
	float					   appUpdateProgress() const;

	bool validDomain();

private:
	void _updateProxyData();
};
