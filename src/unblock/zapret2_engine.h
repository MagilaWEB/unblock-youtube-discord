#pragma once
#include "zapret_engine.h"
#include "strategies_zapret2.h"
#include "../core/service.h"

// Zapret2 technology (winws2.exe, --lua-desync + zapret-helper): owns the
// strategy builder and the "zapret2" service. The helper itself is orchestrated
// by Unblock (it is shared plumbing, not part of the engine).
class Zapret2Engine final : public ZapretEngine
{
	Service			  _service{ "zapret2", "SvcHost.exe" };
	StrategiesZapret2 _strategies{};
	u32				  _strategy_index{ 0 };

public:
	Zapret2Engine();
	~Zapret2Engine() override = default;

	Technology	technology() const override { return Technology::Zapret2; }
	std::string serviceName() const override { return std::string{ toStringView(technology()) }; }

	void serviceConfigFile(const std::shared_ptr<File>& config) override;

	void changeStrategy(std::string_view file) override;
	void changeStrategy(u32 index) override;
	void changeDirVersion(std::string_view dir_version) override;
	void changeOptionalServices(std::list<std::string> list_service) override;
	void changeCustomLists(
		std::vector<std::string> hosts, std::vector<std::string> ip_set, std::vector<std::string> domains_exclude, std::vector<std::string> ip_exclude
	) override;

	const std::vector<std::string>& strategiesList() const override;
	const std::vector<std::string>& strategies() override;
	std::string						strategyName() const override;
	size_t							strategiesSize() const override;
	std::vector<std::string>		listVersionStrategy() override;
	bool							automaticallyStrategy() override;

	void start() override;
	void stop() override;
	void remove() override;
	bool isRun() override;
};
