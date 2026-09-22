#include "zapret1_engine.h"

Zapret1Engine::Zapret1Engine()
{
	_service.open();
}

void Zapret1Engine::serviceConfigFile(const std::shared_ptr<File>& config)
{
	_strategies.serviceConfigFile(config);
}

void Zapret1Engine::changeStrategy(std::string_view file)
{
	_strategies.changeStrategy(file);
}

void Zapret1Engine::changeStrategy(u32 index)
{
	_strategies.changeStrategy(index);
}

void Zapret1Engine::changeDirVersion(std::string_view dir_version)
{
	_strategies.changeDirVersion(dir_version);
}

void Zapret1Engine::changeOptionalServices(std::list<std::string> list_service)
{
	_strategies.changeOptionalServices(std::move(list_service));
}

void Zapret1Engine::changeCustomLists(
	std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
)
{
	_strategies.changeCustomLists(std::move(hosts), std::move(ip_set), std::move(domains_exclude), std::move(ip_exclude));
}

const std::vector<std::string>& Zapret1Engine::strategiesList() const
{
	return _strategies.getStrategyList();
}

const std::vector<std::string>& Zapret1Engine::strategies()
{
	return _strategies.getStrategy();
}

std::string Zapret1Engine::strategyName() const
{
	return _strategies.getStrategyFileName();
}

size_t Zapret1Engine::strategiesSize() const
{
	return _strategies.getStrategySize();
}

std::vector<std::string> Zapret1Engine::listVersionStrategy()
{
	return StrategyConfigBase::listVersionDirs(Core::get().configsPath() / "strategy_zapret1");
}

bool Zapret1Engine::automaticallyStrategy()
{
	if (_strategy_index == _strategies.getStrategySize())
	{
		_strategy_index = 0;
		return false;
	}

	_strategies.changeStrategy(_strategy_index++);

	return true;
}

void Zapret1Engine::changeFakeKey(std::string_view key)
{
	_strategies.changeFakeKey(key);
}

std::vector<std::string> Zapret1Engine::fakeBinKeys() const
{
	std::vector<std::string> keys{};
	for (auto& [key, _] : _strategies.getFakeBinList())
		keys.push_back(key);
	return keys;
}

std::string Zapret1Engine::fakeBinKey() const
{
	return _strategies.getKeyFakeBin();
}

void Zapret1Engine::start()
{
	auto& list = _strategies.getStrategy();

	remove();

	if (list.empty())
		return;

	_service.setDescription(Localization::Str{ "str_service_zapret_description" }());

	std::vector<std::string> args{};
	args.reserve(list.size() + 1);
	args.push_back((Core::get().binariesPath() / "winws.exe").string());

	for (auto& line : list)
		args.push_back(line);

	_service.setArgs(std::move(args));
	_service.create();
	_service.start();
}

void Zapret1Engine::stop()
{
	_service.stop();
}

void Zapret1Engine::remove()
{
	_service.remove();
}

bool Zapret1Engine::isRun()
{
	return _service.isRun();
}
