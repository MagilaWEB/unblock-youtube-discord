#pragma once
#include "strategies_dpi.h"
#include "domain_testing.h"
#include "dns_host.h"

#include "../core/service.h"

class UNBLOCK_API Unblock final
{
	Service _zapret{ "zapret2", "winws2.exe" };
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

	u32 _strategy{};

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

	bool runTest(bool video = false);

	std::string getNameStrategies();

	const std::vector<std::string>&							  getStrategiesList();
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

	void changeMaxConnectionAttempts(u32 count);

	std::optional<std::string> checkUpdate();
	bool					   appUpdate();
	float					   appUpdateProgress() const;

	bool validDomain();

private:
	void _updateProxyData();
};
