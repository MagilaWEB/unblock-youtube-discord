#pragma once
#include "strategies_dpi.h"
#include "proxy_strategies_dpi.h"
#include "domain_testing.h"
#include "dns_host.h"

#include "../core/service.h"

template<typename T>
concept ValidStrategies = std::same_as<T, StrategiesDPI> || std::same_as<T, ProxyStrategiesDPI>;

class UNBLOCK_API Unblock final
{
	Ptr<Service> _zapret{ "zapret", "winws.exe" };
	Ptr<Service> _proxy_dpi{ "proxy_dpi", "ciadpi.exe" };
	Ptr<Service> _win_divert{ "WinDivert" };

	Ptr<DomainTesting>		_domain_testing;
	Ptr<DomainTesting>		_domain_testing_video;
	Ptr<DomainTesting>		_domain_testing_proxy{ true };
	Ptr<DomainTesting>		_domain_testing_proxy_video{ true };
	Ptr<StrategiesDPI>		_strategies_dpi;
	Ptr<ProxyStrategiesDPI> _proxy_strategies_dpi;
	Ptr<DNSHost>			_dns_hosts;

#ifdef DEBUG
	std::atomic_bool _zapret_dbg_run_end;
	std::atomic_bool _zapret_dbg_run;
#endif	  // DEBUG

	ProxyData _proxy_data{};

	struct StrategyType
	{
		u32 proxy_type{ 0 };
		u32 type{ 0 };
		u32 fake_bin{ 0 };
	};

	StrategyType _strategy{};

public:
	Unblock();

	template<ValidStrategies Type>
	bool automaticallyStrategy();

	void changeStrategy(pcstr name_config, pcstr name_fake_bin);
	void changeProxyStrategy(pcstr name_config);
	void changeFilteringTopLevelDomains(bool state = false);

	void changeProxyIP(std::string ip);
	void changeProxyPort(u32 port);

	void changeDirVersionStrategy(std::string dir_version);

	void addOptionalStrategies(std::string name);
	void removeOptionalStrategies(std::string name);
	void clearOptionalStrategies();

	template<ValidStrategies Type>
	bool runTest(bool video = false);

	template<ValidStrategies Type>
	std::string getNameStrategies();
	std::string getNameFakeBin();

	template<ValidStrategies Type>
	const std::vector<std::string>&							  getStrategiesList();
	const std::map<std::string, StrategiesDPI::FakeBinParam>& getFakeBinList();
	std::list<Service>&										  getConflictingServices();

	void startService(bool proxy = false);
	void stopService(bool proxy = false);
	void removeService(bool proxy = false);
	bool activeService(bool proxy = false);
	void checkStateServices(const std::function<void(pcstr, bool)>& callback);

	void						  dnsHosts(bool state);
	void						  dnsHostsUpdate();
	void						  dnsHostsCancelUpdate();
	float						  dnsHostsUpdateProgress() const;
	bool						  dnsHostsCheck() const;
	const std::list<std::string>& dnsHostsListName();

	template<ValidStrategies Type>
	void testingDomain(std::function<void(pcstr, bool)>&& callback = [](pcstr, bool) {}, bool video = false, bool base_test = true);
	template<ValidStrategies Type>
	void testingDomainCancel(bool video = false);

	void accurateTesting(bool state);
	void maxWaitTesting(u32 second);
	void maxWaitAccurateTesting(u32 second);

	bool validDomain(bool proxy = false);

private:
	void _updateProxyData();
};
