#include "unblock.h"

Unblock::Unblock()
{
	_zapret->open();
	_unblock->open();
	_goodbay_dpi->open();
	_proxy_dpi->open();
	_win_divert->open();
}

template<typename Type>
bool Unblock::automaticallyStrategy()
{
	static_assert(
		std::is_same_v<Type, StrategiesDPI> || std::is_same_v<Type, ProxyStrategiesDPI>,
		"It can only be StrategiesDPI or ProxyStrategiesDPI!"
	);

	constexpr bool proxy = std::is_same_v<Type, ProxyStrategiesDPI>;

	Type* strategies_dpi = nullptr;

	if constexpr (proxy)
	{
		strategies_dpi = _proxy_strategies_dpi.get();

		if (_strategy.proxy_type == strategies_dpi->getStrategySize())
		{
			_strategy.proxy_type = 0;
			return false;
		}
	}
	else
	{
		strategies_dpi = _strategies_dpi.get();

		if (_strategy.type == strategies_dpi->getStrategySize())
		{
			_strategy.type	   = 0;
			_strategy.fake_bin = 0;
			return false;
		}
	}

	strategies_dpi->changeStrategy(proxy ? _strategy.proxy_type : _strategy.type);

	if constexpr (!proxy)
	{
		if (strategies_dpi->isFaked())
		{
			strategies_dpi->changeFakeKey(_strategy.fake_bin);
			const auto& fake_bin_list = strategies_dpi->getFakeBinList();
			if (_strategy.fake_bin == (fake_bin_list.size() - 1))
			{
				_strategy.fake_bin = 0;
				_strategy.type++;
			}
			else
				_strategy.fake_bin++;
		}
		else
		{
			strategies_dpi->changeFakeKey(1);
			_strategy.fake_bin = 0;
			_strategy.type++;
		}
	}
	else
		_strategy.proxy_type++;

	return true;
}
template UNBLOCK_API bool Unblock::automaticallyStrategy<StrategiesDPI>();
template UNBLOCK_API bool Unblock::automaticallyStrategy<ProxyStrategiesDPI>();

void Unblock::changeStrategy(pcstr name_config, pcstr name_fake_bin)
{
	_strategy.type	   = 0;
	_strategy.fake_bin = 0;

	_strategies_dpi->changeStrategy(name_config);
	_strategies_dpi->changeFakeKey(name_fake_bin);
}

void Unblock::changeProxyStrategy(pcstr name_config)
{
	_strategy.proxy_type = 0;

	_proxy_strategies_dpi->changeStrategy(name_config);
}

void Unblock::changeFilteringTopLevelDomains(bool state)
{
	_strategies_dpi->changeFilteringTopLevelDomains(state);
}

template<typename Type>
std::string Unblock::getNameStrategies()
{
	static_assert(
		std::is_same_v<Type, StrategiesDPI> || std::is_same_v<Type, ProxyStrategiesDPI>,
		"It can only be StrategiesDPI or ProxyStrategiesDPI!"
	);

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

template<typename Type>
const std::vector<std::string>& Unblock::getStrategiesList()
{
	static_assert(
		std::is_same_v<Type, StrategiesDPI> || std::is_same_v<Type, ProxyStrategiesDPI>,
		"It can only be StrategiesDPI or ProxyStrategiesDPI!"
	);

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

template<typename Type>
bool Unblock::runTest(bool video)
{
	static_assert(
		std::is_same_v<Type, StrategiesDPI> || std::is_same_v<Type, ProxyStrategiesDPI>,
		"It can only be StrategiesDPI or ProxyStrategiesDPI!"
	);

	CODE_TESTING_DOMAIN()

	if (video)
		return testing_video->isTesting();

	return testing->isTesting();
}
template UNBLOCK_API bool Unblock::runTest<StrategiesDPI>(bool);
template UNBLOCK_API bool Unblock::runTest<ProxyStrategiesDPI>(bool);

template<typename Type>
void Unblock::testingDomain(std::function<void(pcstr url, bool state)>&& callback, bool video, bool base_test)
{
	static_assert(
		std::is_same_v<Type, StrategiesDPI> || std::is_same_v<Type, ProxyStrategiesDPI>,
		"It can only be StrategiesDPI or ProxyStrategiesDPI!"
	);

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

template<typename Type>
void Unblock::testingDomainCancel(bool video)
{
	static_assert(
		std::is_same_v<Type, StrategiesDPI> || std::is_same_v<Type, ProxyStrategiesDPI>,
		"It can only be StrategiesDPI or ProxyStrategiesDPI!"
	);

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

	return _unblock->isRun() || _goodbay_dpi->isRun();
}

void Unblock::removeService(bool proxy)
{
	if (proxy)
	{
		_proxy_dpi->remove();
		return;
	}

	_unblock->remove();
	_goodbay_dpi->remove();
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
	_goodbay_dpi->stop();
}

void Unblock::startService(bool proxy)
{
	if (proxy)
	{
		const auto list = _proxy_strategies_dpi->getStrategy();
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

	_zapret->remove();
	_unblock->remove();
	_goodbay_dpi->remove();

	for (auto& [index, service] : indexStrategies)
	{
		const auto list = _strategies_dpi->getStrategy(index);
		if (!list.empty())
		{
			std::string str_service{ service };
			if (index == 0)
			{
				_unblock->setDescription("DPI программное обеспечение для обхода блокировки.");
				_unblock->setArgs(list);
				_unblock->create();

				_unblock->start();
			}

			if (index == 1)
			{
				_goodbay_dpi->setDescription("GoodbyeDPI программное обеспечение для обхода блокировки.");
				_goodbay_dpi->setArgs(list);
				_goodbay_dpi->create();

				_goodbay_dpi->start();
			}
		}
	}
}
