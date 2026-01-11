#include "unblock.h"
#include "../engine/version.hpp"
#include <bit7z/bitfileextractor.hpp>

Unblock::Unblock()
{
	_zapret->open();
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
	_strategies_dpi->changeFakeKey(name_fake_bin);
	_strategies_dpi->changeStrategy(name_config);
}

void Unblock::changeProxyStrategy(pcstr name_config)
{
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

void Unblock::changeDirVersionStrategy(std::string dir_version)
{
	_strategies_dpi->changeDirVersion(dir_version);
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

const std::map<std::string, StrategiesDPI::FakeBinParam>& Unblock::getFakeBinList()
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
				if (std::regex_match(name_service, std::regex{ _zapret->getName() }))
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

void Unblock::maxWaitTesting(u32 second)
{
	_domain_testing->changeMaxWaitTesting(second);
	_domain_testing_video->changeMaxWaitTesting(second);
	_domain_testing_proxy->changeMaxWaitTesting(second);
	_domain_testing_proxy_video->changeMaxWaitTesting(second);
}

std::optional<std::string> Unblock::checkUpdate()
{
	HttpsLoad version{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest" };

	auto lines = version.run();
	if (version.codeResult() == 200)
	{
		for (auto& line : lines)
		{
			static std::string version_mask{ "/MagilaWEB/unblock-youtube-discord/tree/v" };
			size_t			   pos = line.find(version_mask);
			if (pos != std::string::npos)
			{
				static std::string mask_end{ "\" data-tab-item=\"i0code-tab\"" };
				size_t			   pos_end = line.find(mask_end);
				if (pos_end != std::string::npos)
				{
					auto start_str = pos + version_mask.length();
					auto str	   = line.substr(start_str, pos_end - start_str);
					if (Core::get().isVersionNewer(str, VERSION_STR))
						return str;

					return {};
				}
			}
		}
	}

	return {};
}

constexpr static pcstr setup_update_script{ R"(
ECHO off
SET CURRENT_DIR=%~dp0

goto wait_loop

:wait_loop
tasklist /fi "imagename eq engine.exe" /v | find /i "Unblock Version:" >nul
if %errorlevel% == 0 (
    timeout /t 1 /nobreak >nul
    goto wait_loop
) else (
   goto close_unblock
)

:close_unblock

RD %CURRENT_DIR%\bin /S /Q
RD %CURRENT_DIR%\binaries /S /Q
RD %CURRENT_DIR%\configs /S /Q
RD %CURRENT_DIR%\ui /S /Q

ROBOCOPY %CURRENT_DIR%update\unblock %CURRENT_DIR% /E /IS /IT /COPYALL /R:0 /W:0 /NP /NJH /NJS

RD %CURRENT_DIR%\update /S /Q

start %CURRENT_DIR%bin\engine.exe
start cmd /c del "%CURRENT_DIR%setup_update.bat"&exit
exit
)" };

void Unblock::appUpdate()
{
	auto path = Core::get().currentPath() / "update" / "new_unblock.7z";
	HttpsLoad{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest/download/unblock.7z" }.run_to_file(path);

	try
	{
		static bit7z::Bit7zLibrary	   lib{ "7za.dll" };
		static bit7z::BitFileExtractor extractor{ lib, bit7z::BitFormat::SevenZip };

		extractor.extract(path.string(), path.parent_path().string());
	}
	catch (const bit7z::BitException& ex)
	{
		Debug::warning("%s", ex.what());
	}

	std::string setup_bat_path{ (Core::get().currentPath() / "setup_update").string() + ".bat" };
	std::string run_bat{ "start " + setup_bat_path };

	std::fstream bat;
	bat.open(setup_bat_path.c_str(), std::ios::out | std::ios::binary);
	bat.clear();
	bat << setup_update_script;
	bat.close();

	while (!bat.is_open())
		bat.open(setup_bat_path.c_str(), std::ios::in);
	bat.close();

	system(run_bat.c_str());
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

	return _zapret->isRun();
}

void Unblock::checkStateServices(const std::function<void(pcstr, bool)>& callback)
{
	callback("Zapret (winws.exe)", _zapret->isRun());
	callback("ProxyDPI (BayDPI)", _proxy_dpi->isRun());
	callback(_win_divert->getName().c_str(), _win_divert->isRun());
}

void Unblock::dnsHosts(bool state)
{
	state ? _dns_hosts->enable() : _dns_hosts->disable();
}

void Unblock::dnsHostsUpdate()
{
	_dns_hosts->update();
}

void Unblock::dnsHostsCancelUpdate()
{
	_dns_hosts->cancel();
}

float Unblock::dnsHostsUpdateProgress() const
{
	return _dns_hosts->percentageCompletion();
}

bool Unblock::dnsHostsCheck() const
{
	return _dns_hosts->isHostsUser();
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

#ifdef DEBUG
	if (_zapret_dbg_run.load())
	{
		_zapret_dbg_run_end.store(true);

		while (_zapret_dbg_run.load())
			std::this_thread::yield();
	}
#endif

	_zapret->remove();
	_win_divert->remove();
}

void Unblock::stopService(bool proxy)
{
	if (proxy)
	{
		_proxy_dpi->stop();
		return;
	}

#ifdef DEBUG
	if (_zapret_dbg_run.load())
	{
		_zapret_dbg_run_end.store(true);

		while (_zapret_dbg_run.load())
			std::this_thread::yield();
	}
#endif

	_zapret->stop();
}

void Unblock::startService(bool proxy)
{
	if (proxy)
	{
		auto& list = _proxy_strategies_dpi->getStrategy();
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

	auto& list = _strategies_dpi->getStrategy();
	if (!list.empty())
	{
		_zapret->setDescription("DPI программное обеспечение для обхода блокировки.");
		_zapret->setArgs(list);
		_zapret->create();

#ifdef DEBUG
		auto&		service_config = _zapret->getConfig();
		auto&		path		   = service_config.binary_path;
		std::string command		   = path;

		command = std::regex_replace(command, std::regex{ "\"" }, "");
		command = std::regex_replace(command, std::regex{ "--wf-tcp" }, "--debug --wf-tcp");

		if (_zapret_dbg_run.load())
		{
			_zapret_dbg_run_end.store(true);

			while (_zapret_dbg_run.load())
				std::this_thread::yield();
		}

		Core::get().exec_parallel(
			command,
			[this](std::string)
			{
				if (_zapret_dbg_run_end.load())
				{
					_zapret_dbg_run_end.store(false);
					_zapret_dbg_run.store(false);
					return true;
				}

				if (!_zapret_dbg_run.load())
					_zapret_dbg_run.store(true);

				return false;
			}
		);
#else
		_zapret->start();
#endif
	}
}

void Unblock::_updateProxyData()
{
	_proxy_strategies_dpi->changeProxyData(_proxy_data);
	_domain_testing_proxy->changeProxy(_proxy_data.IP, _proxy_data.PORT);
	_domain_testing_proxy_video->changeProxy(_proxy_data.IP, _proxy_data.PORT);
}
