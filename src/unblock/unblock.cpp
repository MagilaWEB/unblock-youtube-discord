#include "unblock.h"

Unblock::Unblock()
{
	_unblock->open();
	_proxy_dpi->open();
	_win_divert->open();
}

template<ValidStrategies Type>
bool Unblock::automaticallyStrategy()
{
	if constexpr (std::is_same_v<Type, ProxyStrategiesDPI>)
	{
		if (_strategy.proxy_type == _proxy_strategies_dpi->getStrategySize())
		{
			_strategy.proxy_type = 0;
			return false;
		}

		_proxy_strategies_dpi->changeStrategy(_strategy.proxy_type);
		_strategy.proxy_type++;
	}
	else
	{
		if (_strategy.type == _strategies_dpi->getStrategySize())
		{
			_strategy.type	   = 0;
			_strategy.fake_bin = 0;
			return false;
		}

		_strategies_dpi->changeFakeKey(_strategy.fake_bin);
		_strategies_dpi->changeStrategy(_strategy.type);
		const auto& fake_bin_list = _strategies_dpi->getFakeBinList();
		if (_strategy.fake_bin == (fake_bin_list.size() - 1))
		{
			_strategy.fake_bin = 0;
			_strategy.type++;
		}
		else
			_strategy.fake_bin++;
	}

	return true;
}
template UNBLOCK_API bool Unblock::automaticallyStrategy<StrategiesDPI>();
template UNBLOCK_API bool Unblock::automaticallyStrategy<ProxyStrategiesDPI>();

void Unblock::changeStrategy(pcstr name_config, pcstr name_fake_bin)
{
	_strategy.type	   = 0;
	_strategy.fake_bin = 0;

	_strategies_dpi->changeFakeKey(name_fake_bin);
	_strategies_dpi->changeStrategy(name_config);
}

void Unblock::changeProxyStrategy(pcstr name_config)
{
	_strategy.proxy_type = 0;

	_proxy_strategies_dpi->changeStrategy(name_config);
}

void Unblock::changeProxyIP(std::string ip)
{
	_proxy_data.IP = ip;
	_updateProxyData();
}

void Unblock::changeProxyPort(u32 port)
{
	_proxy_data.PORT = port;
	_updateProxyData();
}

void Unblock::changeFilteringTopLevelDomains(bool state)
{
	_strategies_dpi->changeFilteringTopLevelDomains(state);
}

void Unblock::addOptionalStrategies(std::string name)
{
	_strategies_dpi->addOptionalStrategies(name);
	_domain_testing->addOptionalStrategies(name);
}

void Unblock::removeOptionalStrategies(std::string name)
{
	_strategies_dpi->removeOptionalStrategies(name);
	_domain_testing->removeOptionalStrategies(name);
}

void Unblock::clearOptionalStrategies()
{
	_strategies_dpi->clearOptionalStrategies();
	_domain_testing->clearOptionalStrategies();
}

template<ValidStrategies Type>
std::string Unblock::getNameStrategies()
{
	if constexpr (std::is_same_v<Type, StrategiesDPI>)
		return _strategies_dpi->getStrategyFileName();
	else
		return _proxy_strategies_dpi->getStrategyFileName();
}
template UNBLOCK_API std::string Unblock::getNameStrategies<StrategiesDPI>();
template UNBLOCK_API std::string Unblock::getNameStrategies<ProxyStrategiesDPI>();

std::string Unblock::getNameFakeBin()
{
	return _strategies_dpi->getKeyFakeBin();
}

template<ValidStrategies Type>
const std::vector<std::string>& Unblock::getStrategiesList()
{
	if constexpr (std::is_same_v<Type, StrategiesDPI>)
		return _strategies_dpi->getStrategyList();
	else
		return _proxy_strategies_dpi->getStrategyList();
}
template UNBLOCK_API const std::vector<std::string>& Unblock::getStrategiesList<StrategiesDPI>();
template UNBLOCK_API const std::vector<std::string>& Unblock::getStrategiesList<ProxyStrategiesDPI>();

const std::vector<StrategiesDPI::FakeBinParam>& Unblock::getFakeBinList()
{
	return _strategies_dpi->getFakeBinList();
}

std::list<Service>& Unblock::getConflictingServices()
{
#if __clang__
	[[clang::no_destroy]]
#endif
	static std::list<Service> conflicting_service;

	Service::allService(
		[&](std::string name_service)
		{
			if (name_service.empty())
				return;

			Service service{ name_service.c_str() };
			service.open();
			auto& config = service.getConfig();

			if (config.binary_path.contains("winws.exe") || config.binary_path.contains("goodbyedpi.exe")
				|| config.binary_path.contains("ciadpi.exe"))
			{
				if (std::regex_match(name_service, std::regex{ _unblock->getName() }))
					return;

				if (std::regex_match(name_service, std::regex{ _proxy_dpi->getName() }))
					return;

				if (std::regex_match(name_service, std::regex{ _win_divert->getName() }))
					return;

				conflicting_service.emplace_back(Service{ name_service.c_str() });
				conflicting_service.back().open();
			}
		}
	);

	return conflicting_service;
}

#define CODE_TESTING_DOMAIN()                              \
	DomainTesting* testing		 = nullptr;                \
	DomainTesting* testing_video = nullptr;                \
	if constexpr (std::is_same_v<Type, StrategiesDPI>)     \
	{                                                      \
		testing		  = _domain_testing.get();             \
		testing_video = _domain_testing_video.get();       \
	}                                                      \
	else                                                   \
	{                                                      \
		testing		  = _domain_testing_proxy.get();       \
		testing_video = _domain_testing_proxy_video.get(); \
	}

template<ValidStrategies Type>
bool Unblock::runTest(bool video)
{
	CODE_TESTING_DOMAIN()

	if (video)
		return testing_video->isTesting();

	return testing->isTesting();
}
template UNBLOCK_API bool Unblock::runTest<StrategiesDPI>(bool);
template UNBLOCK_API bool Unblock::runTest<ProxyStrategiesDPI>(bool);

template<ValidStrategies Type>
void Unblock::testingDomain(std::function<void(pcstr url, bool state)>&& callback, bool video, bool base_test)
{
	CODE_TESTING_DOMAIN()

	if (video)
	{
		testing_video->test(video, base_test, [callback](pcstr url, bool state) { callback(url, state); });
		testing_video->printTestInfo();
		return;
	}

	testing->test(video, base_test, [callback](pcstr url, bool state) { callback(url, state); });
	testing->printTestInfo();
}
template UNBLOCK_API void Unblock::testingDomain<StrategiesDPI>(std::function<void(pcstr, bool)>&&, bool, bool);
template UNBLOCK_API void Unblock::testingDomain<ProxyStrategiesDPI>(std::function<void(pcstr, bool)>&&, bool, bool);

template<ValidStrategies Type>
void Unblock::testingDomainCancel(bool video)
{
	CODE_TESTING_DOMAIN()

	if (video)
	{
		testing_video->cancelTesting();
		return;
	}

	testing->cancelTesting();
}
template UNBLOCK_API void Unblock::testingDomainCancel<StrategiesDPI>(bool);
template UNBLOCK_API void Unblock::testingDomainCancel<ProxyStrategiesDPI>(bool);

void Unblock::accurateTesting(bool state)
{
	_domain_testing->changeAccurateTest(state);
	_domain_testing_video->changeAccurateTest(state);
	_domain_testing_proxy->changeAccurateTest(state);
	_domain_testing_proxy_video->changeAccurateTest(state);
}

void Unblock::maxWaitTesting(u32 second)
{
	_domain_testing->changeMaxWaitTesting(second);
	_domain_testing_video->changeMaxWaitTesting(second);
	_domain_testing_proxy->changeMaxWaitTesting(second);
	_domain_testing_proxy_video->changeMaxWaitTesting(second);
}

void Unblock::maxWaitAccurateTesting(u32 second)
{
	_domain_testing->changeMaxWaitAccurateTesting(second);
	_domain_testing_video->changeMaxWaitAccurateTesting(second);
	_domain_testing_proxy->changeMaxWaitAccurateTesting(second);
	_domain_testing_proxy_video->changeMaxWaitAccurateTesting(second);
}

bool Unblock::validDomain(bool proxy)
{
	if (proxy)
		return _domain_testing_proxy->successRate() >= MAX_SUCCESS_CONECTION;

	return _domain_testing->successRate() >= MAX_SUCCESS_CONECTION;
}

bool Unblock::activeService(bool proxy)
{
	if (proxy)
		return _proxy_dpi->isRun();

	return _unblock->isRun();
}

void Unblock::checkStateServices(const std::function<void(pcstr, bool)>& callback)
{
	callback("Unblock (winws.exe)", _unblock->isRun());
	callback("ProxyDPI (BayDPI)", _proxy_dpi->isRun());
	callback(_win_divert->getName().c_str(), _win_divert->isRun());
}

void Unblock::dnsHosts(bool state)
{
	state ? _dns_hosts->enable() : _dns_hosts->disable();
}

const std::list<std::string>& Unblock::dnsHostsListName()
{
	return _dns_hosts->listDnsFileName();
}

void Unblock::removeService(bool proxy)
{
	if (proxy)
	{
		_proxy_dpi->remove();
		return;
	}

	_unblock->remove();
	_win_divert->remove();
}

void Unblock::stopService(bool proxy)
{
	if (proxy)
	{
		_proxy_dpi->stop();
		return;
	}

	_unblock->stop();
}

void Unblock::startService(bool proxy)
{
	if (proxy)
	{
		auto & list = _proxy_strategies_dpi->getStrategy();
		if (!list.empty())
		{
			_proxy_dpi->remove();
			_proxy_dpi->setDescription("Proxy DPI программное обеспечение для обхода блокировки.");
			_proxy_dpi->setArgs(list);
			_proxy_dpi->create();

			_proxy_dpi->start();
		}
		return;
	}

	_unblock->remove();

	auto& list = _strategies_dpi->getStrategy();
	if (!list.empty())
	{
		_unblock->setDescription("DPI программное обеспечение для обхода блокировки.");
		_unblock->setArgs(list);
		_unblock->create();

		_unblock->start();
	}
}

void Unblock::_updateProxyData()
{
	_proxy_strategies_dpi->changeProxyData(_proxy_data);
	_domain_testing_proxy->changeProxy(_proxy_data.IP, _proxy_data.PORT);
	_domain_testing_proxy_video->changeProxy(_proxy_data.IP, _proxy_data.PORT);
}
