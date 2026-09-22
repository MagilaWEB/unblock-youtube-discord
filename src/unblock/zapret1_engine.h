#pragma once
#include "zapret_engine.h"
#include "strategies_zapret1.h"
#include "../core/service.h"

// Zapret1 technology (classic winws.exe, static --dpi-desync configs): owns
// the strategy builder (with selectable fake profiles) and the "zapret1"
// service. No helper integration.
class Zapret1Engine final : public ZapretEngine
{
	Service			  _service{ "zapret1", "SvcHost.exe" };
	StrategiesZapret1 _strategies{};
	u32				  _strategy_index{ 0 };

public:
	Zapret1Engine();
	~Zapret1Engine() override = default;

	Technology	technology() const override { return Technology::Zapret1; }
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

	// Advances to the next fake profile, wrapping to the first one.
	// Returns false when the list wrapped (all profiles tried).
	bool _advanceFakeKey();

	// Fake profile selection (classic fake-bin concept). No-op concepts on
	// Zapret2: the base defaults cover it, the UI only calls these for Zapret1.
	void					 changeFakeKey(std::string_view key) override;
	std::vector<std::string> fakeBinKeys() const override;
	std::string				 fakeBinKey() const override;

	void start() override;
	void stop() override;
	void remove() override;
	bool isRun() override;
};
