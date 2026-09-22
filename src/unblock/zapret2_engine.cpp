#include "zapret2_engine.h"

Zapret2Engine::Zapret2Engine()
{
	_service.open();
}

void Zapret2Engine::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_strategies.serviceConfigFile(config);
}

void Zapret2Engine::changeStrategy(std::string_view file)
{
	_strategies.changeStrategy(file);
}

void Zapret2Engine::changeStrategy(u32 index)
{
	_strategies.changeStrategy(index);
}

void Zapret2Engine::changeDirVersion(std::string_view dir_version)
{
	_strategies.changeDirVersion(dir_version);
}

void Zapret2Engine::changeOptionalServices(std::list<std::string> list_service)
{
	_strategies.changeOptionalServices(std::move(list_service));
}

void Zapret2Engine::changeCustomLists(
	std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
)
{
	_strategies.changeCustomLists(std::move(hosts), std::move(ip_set), std::move(domains_exclude), std::move(ip_exclude));
}

const std::vector<std::string>& Zapret2Engine::strategiesList() const
{
	return _strategies.getStrategyList();
}

const std::vector<std::string>& Zapret2Engine::strategies()
{
	return _strategies.getStrategy();
}

std::string Zapret2Engine::strategyName() const
{
	return _strategies.getStrategyFileName();
}

size_t Zapret2Engine::strategiesSize() const
{
	return _strategies.getStrategySize();
}

std::vector<std::string> Zapret2Engine::listVersionStrategy()
{
	return StrategyConfigBase::listVersionDirs(Core::get().configsPath() / "strategy");
}

bool Zapret2Engine::automaticallyStrategy()
{
	if (_strategy_index == _strategies.getStrategySize())
	{
		_strategy_index = 0;
		return false;
	}

	_strategies.changeStrategy(_strategy_index++);

	return true;
}

void Zapret2Engine::start()
{
	auto& list = _strategies.getStrategy();

	remove();

	if (list.empty())
		return;

	_service.setDescription(Localization::Str{ "str_service_zapret_description" }());

	std::vector<std::string> args{};
	args.reserve(list.size() + 1);
	args.push_back((Core::get().binariesPath() / "winws2.exe").string());

	for (auto& line : list)
		args.push_back(line);

	_service.setArgs(std::move(args));
	_service.create();
	_service.start();
}

void Zapret2Engine::stop()
{
	_service.stop();
}

void Zapret2Engine::remove()
{
	_service.remove();
}

bool Zapret2Engine::isRun()
{
	return _service.isRun();
}
