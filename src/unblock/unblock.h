#pragma once
#include "strategies_dpi.h"
#include "proxy_strategies_dpi.h"
#include "domain_testing.h"

#include "../core/service.h"

class UNBLOCK_API Unblock final
{
	Ptr<Service> _zapret{ "zapret" };
	Ptr<Service> _unblock{ "unblock", "winws.exe" };
	Ptr<Service> _goodbay_dpi{ "GoodByeDpi", "goodbyedpi.exe" };
	Ptr<Service> _proxy_dpi{ "proxy_dpi", "ciadpi.exe" };
	Ptr<Service> _win_divert{ "WinDivert" };

	Ptr<DomainTesting>		_domain_testing;
	Ptr<DomainTesting>		_domain_testing_video;
	Ptr<DomainTesting>		_domain_testing_proxy{ true };
	Ptr<DomainTesting>		_domain_testing_proxy_video{ true };
	Ptr<StrategiesDPI>		_strategies_dpi;
	Ptr<ProxyStrategiesDPI> _proxy_strategies_dpi;

	struct StrategyType
	{
		u32 proxy_type{ 0 };
		u32 type{ 0 };
		u32 fake_bin{ 0 };
	};

	StrategyType _strategy{};

public:
	Unblock();

	template<typename Type>
	bool automaticallyStrategy();

	void changeStrategy(pcstr name_config, pcstr name_fake_bin);
	void changeProxyStrategy(pcstr name_config);
	void changeFilteringTopLevelDomains(bool state = false);

	template<typename Type>
	bool runTest(bool video = false);

	template<typename Type>
	std::string getNameStrategies();
	std::string getNameFakeBin();

	template<typename Type>
	const std::vector<std::string>&					getStrategiesList();
	const std::vector<StrategiesDPI::FakeBinParam>& getFakeBinList();

	void startService(bool proxy = false);
	void stopService(bool proxy = false);
	void removeService(bool proxy = false);
	bool activeService(bool proxy = false);

	template<typename Type>
	void testingDomain(std::function<void(pcstr url, bool state)>&& callback = [](pcstr, bool) {}, bool video = false, bool base_test = true);
	template<typename Type>
	void testingDomainCancel(bool video = false);

	void accurateTesting(bool state);
	void maxWaitTesting(u32 second);
	void maxWaitAccurateTesting(u32 second);

	bool validDomain(bool proxy = false);
};
