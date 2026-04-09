#include "unblock.h"
#include "../engine/version.hpp"
#include <bit7z/bitfileextractor.hpp>

Unblock::Unblock()
{
	_zapret.open();
	_win_divert.open();
}

bool Unblock::testUrl(std::string_view str_url)
{
	HttpsLoad url{ str_url };

	url.run();

	return url.codeResult() == 200;
}

bool Unblock::automaticallyStrategy()
{
	if (_strategy == _strategies_dpi.getStrategySize())
	{
		_strategy = 0;
		return false;
	}

	_strategies_dpi.changeStrategy(_strategy++);

	return true;
}

void Unblock::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_strategies_dpi.serviceConfigFile(config);
}

void Unblock::changeStrategy(std::string_view name_config)
{
	_strategies_dpi.changeStrategy(name_config);
}

void Unblock::changeDirVersionStrategy(std::string_view dir_version)
{
	_strategies_dpi.changeDirVersion(dir_version);
}

void Unblock::addOptionalStrategies(std::string_view name)
{
	auto it = std::ranges::find(_section_opt_service_names, name);
	if (it != _section_opt_service_names.end())
		return;

	_section_opt_service_names.emplace_back(name);

	_strategies_dpi.changeOptionalServices(_section_opt_service_names);
	_domain_testing.changeOptionalServices(_section_opt_service_names);
}

void Unblock::removeOptionalStrategies(std::string_view name)
{
	std::erase(_section_opt_service_names, name);
	_strategies_dpi.changeOptionalServices(_section_opt_service_names);
	_domain_testing.changeOptionalServices(_section_opt_service_names);
}

void Unblock::clearOptionalStrategies()
{
	_section_opt_service_names.clear();

	_strategies_dpi.changeOptionalServices({});
	_domain_testing.changeOptionalServices({});
}

std::string Unblock::getNameStrategies()
{
	return _strategies_dpi.getStrategyFileName();
}

const std::vector<std::string>& Unblock::getStrategiesList()
{
	return _strategies_dpi.getStrategyList();
}

std::list<Service>& Unblock::getConflictingServices()
{
	static std::list<Service> conflicting_service;

	Service::allService(
		[&](std::string name_service)
		{
			if (name_service.empty())
				return;

			Service service{ name_service };
			service.open();

			auto& config = service.getConfig();

			constexpr static std::string_view services_conflict[]{ "winws.exe", "winws2.exe", "goodbyedpi.exe", "ciadpi.exe" };

			for (auto& name_prosses : services_conflict)
			{
				if (config.binary_path.contains(name_prosses))
				{
					if (std::regex_match(name_service, std::regex{ _zapret.getName() }))
						continue;

					if (std::regex_match(name_service, std::regex{ _win_divert.getName() }))
						continue;

					conflicting_service.emplace_back(Service{ name_service });
					conflicting_service.back().open();
				}
			}
		}
	);

	return conflicting_service;
}

bool Unblock::runTest(bool video)
{
	if (video)
		return _domain_testing_video.isTesting();

	return _domain_testing.isTesting();
}

void Unblock::testingDomain(std::function<void(std::string_view url, bool state)>&& callback, bool video, bool base_test)
{
	if (video)
	{
		_domain_testing_video.test(video, base_test, [callback](std::string_view url, bool state) { callback(url, state); });
		_domain_testing_video.printTestInfo();
		return;
	}

	_domain_testing.test(video, base_test, [callback](std::string_view url, bool state) { callback(url, state); });
	_domain_testing.printTestInfo();
}

void Unblock::testingDomainCancel(bool video)
{
	if (video)
	{
		_domain_testing_video.cancelTesting();
		return;
	}

	_domain_testing.cancelTesting();
}

std::optional<std::string> Unblock::checkUpdate()
{
	HttpsLoad version{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest" };

	auto lines = version.run();

	if (version.codeResult() != 200)
		return {};

	for (auto& line : lines)
	{
		constexpr static std::string_view version_mask{ "/MagilaWEB/unblock-youtube-discord/tree/v" };
		size_t							  pos = line.find(version_mask);
		if (pos != std::string::npos)
		{
			constexpr static std::string_view mask_end{ "\" data-tab-item=\"i0code-tab\"" };
			size_t							  pos_end = line.find(mask_end);
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

	return {};
}

constexpr static std::string_view setup_update_script{ R"(
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
start cmd /c del "%CURRENT_DIR%setup_update.bat"
exit
)" };

static HttpsLoad load_7z{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest/download/unblock.7z" };

bool Unblock::appUpdate()
{
	auto path = Core::get().currentPath() / "update" / "new_unblock.7z";

	load_7z.run_to_file(path);

	u32 code = load_7z.codeResult();
	if (code != 200)
		return false;

	try
	{
		static bit7z::Bit7zLibrary	   lib{ "7za.dll" };
		static bit7z::BitFileExtractor extractor{ lib, bit7z::BitFormat::SevenZip };

		extractor.extract(path.string(), path.parent_path().string());
	}
	catch (const bit7z::BitException& ex)
	{
		Debug::warning("{}", ex.what());
		return false;
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
	return true;
}

float Unblock::appUpdateProgress() const
{
	return load_7z.progress();
}

bool Unblock::validDomain()
{
	return _domain_testing.successRate() >= MAX_SUCCESS_CONECTION;
}

bool Unblock::activeService()
{
	return _zapret.isRun();
}

void Unblock::checkStateServices(const std::function<void(std::string_view, bool)>& callback)
{
	callback("Zapret2 (winws2.exe)", _zapret.isRun());
	callback(_win_divert.getName(), _win_divert.isRun());
}

std::vector<std::string> Unblock::listVersionStrategy()
{
	std::vector<std::string> strategy_dirs{};

	auto patch_dir = Core::get().configsPath() / "strategy";
	for (auto& entry : std::filesystem::directory_iterator(patch_dir))
		strategy_dirs.push_back(entry.path().filename().string());

	std::ranges::sort(
		strategy_dirs,
		[](const std::string& left, const std::string& right)
		{
			static std::regex reg{ "\\." };
			return std::stoul(std::regex_replace(left, reg, "")) > std::stoul(std::regex_replace(right, reg, ""));
		}
	);

	return strategy_dirs;
}

void Unblock::dnsHosts(bool state)
{
	state ? _dns_hosts.enable() : _dns_hosts.disable();
}

void Unblock::dnsHostsUpdate()
{
	_dns_hosts.update();
}

void Unblock::dnsHostsCancelUpdate()
{
	_dns_hosts.cancel();
}

float Unblock::dnsHostsUpdateProgress() const
{
	return _dns_hosts.percentageCompletion();
}

bool Unblock::dnsHostsCheck() const
{
	return _dns_hosts.isHostsUser();
}

const std::list<std::string>& Unblock::dnsHostsListName()
{
	return _dns_hosts.listDnsFileName();
}

void Unblock::removeService()
{
#ifdef DEBUG_RUN_ZAPRET	   // #ifdef DEBUG
	if (_zapret_dbg_run.load())
	{
		_zapret_dbg_run_end.store(true);

		while (_zapret_dbg_run.load())
			std::this_thread::yield();
	}
#endif

	_zapret.remove();
	_win_divert.remove();
}

void Unblock::stopService()
{
#ifdef DEBUG_RUN_ZAPRET	   // #ifdef DEBUG
	if (_zapret_dbg_run.load())
	{
		_zapret_dbg_run_end.store(true);

		while (_zapret_dbg_run.load())
			std::this_thread::yield();
	}
#endif

	_zapret.stop();
}

void Unblock::startService()
{
	_zapret.remove();

	auto& list = _strategies_dpi.getStrategy();
	if (!list.empty())
	{
		_zapret.setDescription(Localization::Str{ "str_service_zapret_description" }());
		_zapret.setArgs(list);
		_zapret.create();

#ifdef DEBUG_RUN_ZAPRET	   // #ifdef DEBUG
		auto&		service_config = _zapret.getConfig();
		auto&		path		   = service_config.binary_path;
		std::string command		   = path;

		command = std::regex_replace(command, std::regex{ "\"" }, "");
		command = std::regex_replace(command, std::regex{ "--wf-tcp-in" }, "--debug --wf-tcp-in");

		if (_zapret_dbg_run.load())
		{
			_zapret_dbg_run_end.store(true);

			while (_zapret_dbg_run.load())
				std::this_thread::yield();
		}

		Core::get().exec_parallel(
			command,
			[this](std::string str)
			{
				if (_zapret_dbg_run_end.load())
				{
					_zapret_dbg_run_end.store(false);
					_zapret_dbg_run.store(false);
					return true;
				}

				if (!_zapret_dbg_run.load())
					_zapret_dbg_run.store(true);

				if (str == "EXIT")
				{
					_zapret_dbg_run.store(false);
					_zapret_dbg_run_end.store(false);
				}

				return false;
			}
		);
#else
		_zapret.start();
#endif
	}
}
