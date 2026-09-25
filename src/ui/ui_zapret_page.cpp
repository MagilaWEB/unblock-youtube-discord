#include "ui_zapret_page.h"

#include "ui.h"
#include "../unblock/unblock.h"

#include <chrono>
#include <thread>
#include <unordered_set>

UiZapretPage::UiZapretPage(std::shared_ptr<Ui> ui) : _ui(std::move(ui))
{
	_file_service_list = std::make_shared<File>();
	_file_service_list->open({ Core::get().configsPath() / "service_setting" }, ".config", true);
	_ui->_unblock->serviceConfigFile(_file_service_list);

	// Initialize services list from config
	_file_service_list->forLineParametersSection(
		"LIST",
		[this](std::string key, std::string /*value*/)
		{
			_list_enable_services.emplace(key, std::make_shared<CheckBox>(std::string{ "_unblock_to_list_" } + key));
			return false;
		}
	);
}

std::string UiZapretPage::_technologyName(Technology technology)
{
	return Localization::Str{ technology == Technology::Zapret1 ? "str_h2_zapret1" : "str_h2_zapret2" }();
}

void UiZapretPage::initialize()
{
	_selectEngine();
	_initMainControls();
	_testingInit();
	_initFakeKey();
	_selectStrategyVersion();
	_selectConfig();
	_selectFakeBin();
	_initCustomLists();
	_listEnableServices();

	// Must run after _listEnableServices(): it restores the enabled services into
	// DomainTesting, and without them the domain list is empty, so the test would
	// report 0% with no hosts. The manual button works because services are set by then.
	if (_ui->getTestingDomainsStartup()->getState())
		_testingServiceDomains();

	updateState();
}

void UiZapretPage::_selectEngine()
{
	_select_engine->create("#zapret .common", "str_select_engine_title", Localization::Str{ "str_select_engine_description" });
	_select_engine->addTutorialStep("str_tour_engine_title", "str_tour_engine_description", 0);
	_select_engine->createOption(std::string{ toStringView(Technology::Zapret2) }, _technologyName(Technology::Zapret2));
	_select_engine->createOption(std::string{ toStringView(Technology::Zapret1) }, _technologyName(Technology::Zapret1));
	_select_engine->addEventChange(
		[this](JSArgs args)
		{
			_requestTechnologySwitch(technologyFromString(JSToCPP<std::string>(args[0])));
			return false;
		}
	);

	Technology saved = Technology::Zapret2;
	if (auto engine = _ui->userConfig()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "engine"))
		saved = technologyFromString(engine.value());

	_applyTechnology(saved);
}

void UiZapretPage::_applyTechnology(Technology technology)
{
	_technology = technology;
	const std::string engine_name{ toStringView(technology) };
	_ui->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "engine", engine_name);
	_select_engine->setSelectedOptionValue(engine_name);

	_selectStrategyVersionUpdate();
	_selectFakeBin();
	// NOTE: no updateState() here on purpose. During initialize() this runs
	// before the widgets exist, and the change-detection cache would swallow
	// the live state — the status would then stay frozen at its initial text
	// even with an engine running. The refresh comes from the updateState()
	// at the end of initialize() and from the Ui::update() ticks.
}

void UiZapretPage::_requestTechnologySwitch(Technology technology)
{
	if (technology == _technology)
	{
		_select_engine->setSelectedOptionValue(std::string{ toStringView(technology) });
		return;
	}

	const Technology other = technology == Technology::Zapret1 ? Technology::Zapret2 : Technology::Zapret1;

	// Switching the view is free, but the running bypass cannot stay up:
	// two engines never run at once, so confirm stopping it first.
	if (!_ui->_unblock->isRun(other))
	{
		_applyTechnology(technology);
		return;
	}

	_window_warning_technology_busy->setDescription(
		utils::format(Localization::Str{ "str_window_warning_technology_switch_description" }(), _technologyName(other))
	);
	_window_warning_technology_busy->show();
	_window_warning_technology_busy->addEventYesNo(
		[technology, this](JSArgs args)
		{
			_window_warning_technology_busy->hide();

			if (!JSToCPP<bool>(args[0]))
			{
				_select_engine->setSelectedOptionValue(std::string{ toStringView(_technology) });
				return true;
			}

			// Stopping services takes a while: show the wait window,
			// same as the stop button does.
			_ui->getUiUnblock()->getWindowWaitStopService()->show();
			Core::get().addTask(
				[technology, this]
				{
					_ui->_unblock->stopService();
					_applyTechnology(technology);
					_ui->getUiUnblock()->getWindowWaitStopService()->hide();
				}
			);
			return true;
		}
	);
}

void UiZapretPage::_listEnableServices()
{
	for (auto& [name, check_box] : _list_enable_services)
	{
		check_box->create(
			"#zapret .service",
			std::string{ "str_unblock_enable_" + name + "_title" },
			Localization::Str{ std::string{ "str_unblock_enable_" + name + "_description" } }
		);

		check_box->addEventClick(
			// NOLINTNEXTLINE(bugprone-exception-escape) - Ultralight callback contract
			[this, name](JSArgs args)
			{
				_ui->userConfig()->writeSectionParameter("UNBLOCK", std::string{ "enable_" } + name, JSToCPP(args[0]));

				_listEnableServicesUpdate();
				return false;
			}
		);
	}

	_listEnableServicesUpdate();
}

void UiZapretPage::_listEnableServicesUpdate()
{
	for (auto& [name, check_box] : _list_enable_services)
	{
		check_box->show();
		_applyServiceState(name, check_box, true);
	}
}

void UiZapretPage::updateServices()
{
	for (auto& [name, check_box] : _list_enable_services)
		if (check_box->isCreate())
			_applyServiceState(name, check_box, false);
}

void UiZapretPage::_applyServiceState(const std::string& name, const std::shared_ptr<CheckBox>& check_box, bool force)
{
	// Falls back to the widget state itself so an unknown service keeps
	// whatever is on screen (same as the old inline logic).
	bool want = check_box->getState();
	if (auto result = _ui->userConfig()->parameterSection<bool>("UNBLOCK", "enable_" + name))
		want = result.value();
	else if (auto state = _file_service_list->parameterSection<bool>("LIST", name))
		want = state.value();
	else if (force)
		Debug::warning(state.error());

	if (!force && check_box->getState() == want)
		return;

	check_box->setState(want);

	if (want)
		_ui->_unblock->addOptionalStrategies(name);
	else
		_ui->_unblock->removeOptionalStrategies(name);
}

std::span<const UiZapretPage::CustomListDef> UiZapretPage::customListDefs()
{
	static const CustomListDef defs[]{
		{			"custom_hosts",
		 "str_zapret_custom_hosts_title",		   "str_zapret_custom_hosts_description",
		 "str_input_zapret_custom_hosts_placeholder", &utils::isValidHostName,
		 &UiZapretPage::_list_custom_hosts			},
		{		   "custom_ip_set",
		 "str_zapret_custom_ip_set_title",		  "str_zapret_custom_ip_set_description",
		 "str_input_zapret_custom_ip_set_placeholder",  &utils::isValidNetwork,
		 &UiZapretPage::_list_custom_ip_set			},
		{ "custom_domains_exclude",
		 "str_zapret_custom_domains_exclude_title", "str_zapret_custom_domains_exclude_description",
		 "str_input_zapret_custom_domains_exclude_placeholder", &utils::isValidHostName,
		 &UiZapretPage::_list_custom_domains_exclude },
		{	   "custom_ip_exclude",
		 "str_zapret_custom_ip_exclude_title",	  "str_zapret_custom_ip_exclude_description",
		 "str_input_zapret_custom_ip_exclude_placeholder",  &utils::isValidNetwork,
		 &UiZapretPage::_list_custom_ip_exclude		},
	};

	return defs;
}

void UiZapretPage::_initCustomLists()
{
	auto on_change = [this](JSArgs)
	{
		_saveCustomLists();
		return false;
	};

	for (const auto& def : customListDefs())
	{
		auto& widget = this->*def.widget;
		widget->create(
			"#zapret .common",
			Localization::Str{ def.title },
			Localization::Str{ def.description }(),
			Localization::Str{ def.placeholder }()
		);
		widget->setValidator(def.validate);
		widget->addEventChange(on_change);

		if (auto cfg = _ui->userConfig()->parameterSectionVector("ZAPRET", std::string{ def.config_key }))
			widget->setItems(std::move(cfg.value()));
	}

	_saveCustomLists();
}

void UiZapretPage::_saveCustomLists()
{
	for (const auto& def : customListDefs())
		_ui->userConfig()->writeSectionParameterVector("ZAPRET", std::string{ def.config_key }, (this->*def.widget)->items());

	_ui->_unblock->setCustomLists(
		_list_custom_hosts->items(),
		_list_custom_ip_set->items(),
		_list_custom_domains_exclude->items(),
		_list_custom_ip_exclude->items()
	);
}

void UiZapretPage::_selectStrategyVersion()
{
	_select_version_strategy
		->create("#zapret .common", "str_select_version_strategy_title", Localization::Str{ "str_select_version_strategy_description" });
	_select_version_strategy->addTutorialStep("str_tour_version_strategy_title", "str_tour_version_strategy_description", 3);
	_select_version_strategy->addEventChange(
		[this](JSArgs args)
		{
			_ui->userConfig()->writeSectionParameter(_rememberSection(), "version_strategy", JSToCPP(args[0]));
			_selectStrategyVersionUpdate();
			return false;
		}
	);

	_selectStrategyVersionUpdate();
}

void UiZapretPage::_selectStrategyVersionUpdate()
{
	if (!_select_version_strategy->isCreate())
		return;

	_select_version_strategy->clear();

	_select_version_strategy->show();

	auto strategy_dirs = _ui->_unblock->listVersionStrategy(_technology);
	if (strategy_dirs.empty())
		return;

	for (const auto& strategy_dir : strategy_dirs)
		_select_version_strategy->createOption(strategy_dir, strategy_dir);

	// Default to the first (newest) version when the config has no valid value.
	// Without this the config selector below stays empty (changeDirVersion("")
	// yields no strategies at all).
	std::string active_version = strategy_dirs.front();
	if (auto strategy_version = _ui->userConfig()->parameterSection<std::string>(_rememberSection(), "version_strategy"))
		if (std::ranges::find(strategy_dirs, strategy_version.value()) != strategy_dirs.end())
			active_version = strategy_version.value();

	_select_version_strategy->setSelectedOptionValue(active_version);
	_ui->userConfig()->writeSectionParameter(_rememberSection(), "version_strategy", active_version);
	_ui->_unblock->changeDirVersionStrategy(_technology, active_version);

	_selectConfigUpdate();
}

void UiZapretPage::_selectConfig()
{
	_select_config->create("#zapret .common", "str_select_config_title", Localization::Str{ "str_select_config_description" });
	_select_config->addTutorialStep("str_tour_config_title", "str_tour_config_description", 4);
	_select_config->addEventChange(
		[this](JSArgs args)
		{
			_ui->userConfig()->writeSectionParameter(_rememberSection(), "config", JSToCPP(args[0]));
			return false;
		}
	);

	_selectConfigUpdate();
}

void UiZapretPage::_selectConfigUpdate()
{
	if (!_select_config->isCreate())
		return;

	_select_config->clear();

	auto& strategies_list = _ui->_unblock->getStrategiesList(_technology);

	if (strategies_list.empty())
		return;

	_select_config->show();

	for (const auto& strategy : strategies_list)
		_select_config->createOption(strategy, strategy);

	// Default to the first config when the config has no valid value.
	// An empty selection would hit the assert in changeStrategy() below.
	std::string active_config = strategies_list[0];
	if (auto config = _ui->userConfig()->parameterSection<std::string>(_rememberSection(), "config"))
		if (std::ranges::find(strategies_list, config.value()) != strategies_list.end())
			active_config = config.value();

	_ui->userConfig()->writeSectionParameter(_rememberSection(), "config", active_config);
	_select_config->setSelectedOptionValue(active_config);

	_buttonUpdate();
	_ui->_unblock->changeStrategy(_technology, JSToCPP(_select_config->getSelectedOptionValue()));
}

void UiZapretPage::_initFakeKey()
{
	if (_technology != Technology::Zapret1)
		return;

	// The fake profile must be selected before any changeStrategy() call:
	// %FAKE_*% placeholders expand at upload time.
	auto keys = _ui->_unblock->fakeBinKeys(_technology);
	if (keys.empty())
		return;

	std::string active = keys.front();
	if (auto saved = _ui->userConfig()->parameterSection<std::string>(_rememberSection(), "fake_bin"))
		if (std::ranges::find(keys, saved.value()) != keys.end())
			active = saved.value();

	_ui->userConfig()->writeSectionParameter(_rememberSection(), "fake_bin", active);
	_ui->_unblock->changeFakeKey(_technology, active);
}

void UiZapretPage::_selectFakeBin()
{
	// The engine selector initializes first and already triggers an update
	// while the config selector does not exist yet; the initialize() call
	// below runs right after it, keeping the UI order: version, config, fake.
	if (!_select_config->isCreate())
		return;

	if (!_select_fake_bin->isCreate())
	{
		_select_fake_bin->create("#zapret .common", "str_select_fake_bin_title", Localization::Str{ "str_select_fake_bin_description" });
		_select_fake_bin->addEventChange(
			[this](JSArgs args)
			{
				_ui->userConfig()->writeSectionParameter(_rememberSection(), "fake_bin", JSToCPP(args[0]));
				_selectFakeBinUpdate();
				return false;
			}
		);
	}

	if (_technology == Technology::Zapret1)
	{
		_select_fake_bin->show();
		_selectFakeBinUpdate();
	}
	else
		_select_fake_bin->hide();
}

void UiZapretPage::_selectFakeBinUpdate()
{
	if (_technology != Technology::Zapret1 || !_select_fake_bin->isCreate())
		return;

	_select_fake_bin->clear();

	auto keys = _ui->_unblock->fakeBinKeys(_technology);
	if (keys.empty())
		return;

	_select_fake_bin->show();

	for (const auto& key : keys)
		_select_fake_bin->createOption(key, key);

	std::string active = keys.front();
	if (auto saved = _ui->userConfig()->parameterSection<std::string>(_rememberSection(), "fake_bin"))
		if (std::ranges::find(keys, saved.value()) != keys.end())
			active = saved.value();

	_select_fake_bin->setSelectedOptionValue(active);
	_ui->userConfig()->writeSectionParameter(_rememberSection(), "fake_bin", active);

	// The key is already applied during init (_initFakeKey); re-upload the
	// current config only when the user actually picked another profile,
	// otherwise every startup logs the strategy selection twice.
	if (_ui->_unblock->fakeBinKey(_technology) == active)
		return;

	_ui->_unblock->changeFakeKey(_technology, active);

	// Re-upload the current config so the profile takes effect immediately.
	if (auto config = _ui->userConfig()->parameterSection<std::string>(_rememberSection(), "config"))
	{
		auto& strategies_list = _ui->_unblock->getStrategiesList(_technology);
		if (std::ranges::find(strategies_list, config.value()) != strategies_list.end())
			_ui->_unblock->changeStrategy(_technology, config.value());
	}
}

void UiZapretPage::_syncFakeProfile()
{
	if (_technology != Technology::Zapret1 || !_select_fake_bin->isCreate())
		return;

	const auto active = _ui->_unblock->fakeBinKey(_technology);
	if (active.empty())
		return;

	// setSelectedOptionValue only mirrors the state, no change events.
	_select_fake_bin->setSelectedOptionValue(active);
	_ui->userConfig()->writeSectionParameter(_rememberSection(), "fake_bin", active);
}

void UiZapretPage::_initMainControls()
{
	_status_engine->create("#zapret .common");
	_status_engine->setInactive(Localization::Str{ "str_status_engine_stopped" }());

	_start_button->create("#zapret .common", "str_b_start_zapret");
	_start_button->addTutorialStep("str_tour_start_button_title", "str_tour_start_button_description", 1);

	_start_button->addEventClick(
		[this](JSArgs)
		{
			_clickStartService();
			return false;
		}
	);

	_window_auto_start_wait->create(Localization::Str{ "str_please_wait" }, "str_window_auto_start_wait_description");
	_window_auto_start_wait->setType(SecondaryWindow::Type::Wait);
	_window_auto_start_wait->addEventCancel(
		[this](JSArgs)
		{
			_automatically_strategy_cancel.store(true);
			_ui->_unblock->testingDomainCancel();
			return false;
		}
	);

	_window_continue_select_strategy->create(Localization::Str{ "str_window_continue_select_strategy_title" }, "");
	_window_continue_select_strategy->setType(SecondaryWindow::Type::YesNo);
	_window_continue_select_strategy->addEventYesNo(
		[this](JSArgs args)
		{
			_window_continue_select_strategy->hide();

			if (!args[0].ToBoolean())
				_autoStart();

			return false;
		}
	);

	_window_configuration_selection_error->create(Localization::Str{ "str_error" }, "str_window_configuration_selection_error");
	_window_configuration_selection_error->setType(SecondaryWindow::Type::OK);

	_window_no_bypass_targets->create(Localization::Str{ "str_error" }, "str_window_no_bypass_targets");
	_window_no_bypass_targets->setType(SecondaryWindow::Type::OK);
	_window_no_bypass_targets->addEventOk(
		[this](JSArgs)
		{
			_window_no_bypass_targets->hide();
			return true;
		}
	);

	_window_warning_technology_busy->create(Localization::Str{ "str_warning" }, "str_window_warning_technology_busy_description");
	_window_warning_technology_busy->setType(SecondaryWindow::Type::YesNo);

	_stop_zapret->create("#zapret .common", "str_b_stop_zapret");
	_stop_zapret->addEventClick(
		[this](JSArgs)
		{
			_ui->getUiUnblock()->getWindowWaitStopService()->show();
			Core::get().addTask(
				[this]
				{
					_ui->_unblock->removeService();
					_ui->getUiUnblock()->getWindowWaitStopService()->hide();
					_buttonUpdate();
				}
			);
			return false;
		}
	);

	_run_auto_config_zapret->create("#zapret .common", "str_b_run_auto_config_zapret");
	_run_auto_config_zapret->addTutorialStep("str_tour_auto_config_title", "str_tour_auto_config_description", 2);

	_run_auto_config_zapret->addEventClick(
		[this](JSArgs)
		{
			_autoStart();
			return false;
		}
	);
}

void UiZapretPage::_testingInit()
{
	_start_testing_zapret->create("#zapret .common", "str_b_start_testing_zapret");
	_start_testing_zapret->addTutorialStep("str_tour_testing_title", "str_tour_testing_description", 6);
	_start_testing_zapret->addEventClick(
		[this](JSArgs)
		{
			if (_ui->_unblock->runTest())
				return false;

			_testingServiceDomains();
			return false;
		}
	);

	_initTestingWindow();
}

void UiZapretPage::_buttonUpdate()
{
	if (_ui->_unblock->isRun(_technology))
		_start_button->setTitle("str_b_restart_unblock");
	else
		_start_button->setTitle("str_b_start_zapret");
}

void UiZapretPage::_updateStatus(std::optional<Technology> active)
{
	if (!_status_engine->isCreate())
		return;

	if (!active.has_value())
		_status_engine->setInactive(Localization::Str{ "str_status_engine_stopped" }());
	else
		_status_engine->setActive(utils::format(Localization::Str{ "str_status_engine_running" }(), _technologyName(active.value())));
}

void UiZapretPage::updateState()
{
	const bool running = _ui->_unblock->isRun(_technology);
	const auto active  = _ui->_unblock->runningTechnology();
	if (running == _last_running && active == _last_active)
		return;

	_last_running = running;
	_last_active  = active;
	_buttonUpdate();
	_updateStatus(active);
}

bool UiZapretPage::_hasBypassTargets() const
{
	return _ui->_unblock->hasOptionalStrategies() || !_list_custom_hosts->items().empty() || !_list_custom_ip_set->items().empty();
}

bool UiZapretPage::_requireBypassTargets()
{
	if (_hasBypassTargets())
		return true;

	_window_no_bypass_targets->show();
	return false;
}

void UiZapretPage::_startWithTechnologyCheck(std::function<void()>&& proceed)
{
	const Technology other = _technology == Technology::Zapret1 ? Technology::Zapret2 : Technology::Zapret1;

	if (!_ui->_unblock->isRun(other))
	{
		proceed();
		return;
	}

	_window_warning_technology_busy->setDescription(
		utils::format(Localization::Str{ "str_window_warning_technology_busy_description" }(), _technologyName(other), _technologyName(_technology))
	);
	_window_warning_technology_busy->show();
	_window_warning_technology_busy->addEventYesNo(
		[proceed = std::move(proceed), this](JSArgs args)
		{
			_window_warning_technology_busy->hide();

			if (JSToCPP<bool>(args[0]))
				proceed();

			return true;
		}
	);
}

void UiZapretPage::_clickStartService()
{
	if (!_requireBypassTargets())
		return;

	if (auto config = _ui->userConfig()->parameterSection<std::string>(_rememberSection(), "config"))
	{
		auto& strategy_list = _ui->_unblock->getStrategiesList(_technology);
		if (std::ranges::find(strategy_list, config.value()) == strategy_list.end())
		{
			Debug::warning("config[{}] The specified strategy does not exist from the user's settings!", config.value());

			_ui->userConfig()->writeSectionParameter(_rememberSection(), "config", "");

			_select_config->setSelectedOptionValue(strategy_list[0]);
		}
	}

	_startWithTechnologyCheck([this] { _startServiceFromConfig(); });
}

void UiZapretPage::_autoStart()
{
	if (!_requireBypassTargets())
		return;

	_startWithTechnologyCheck(
		[this]
		{
			_tcpGlobalChange(true);

			Core::get().addTask(
				[this]
				{
					InputConsole::textOk(Localization::Str{ "str_beginning_auto_selection" }());

					_window_auto_start_wait->setDescription(_autoStartWaitDescription());
					_window_auto_start_wait->show();

					while (_autoStartTryNext())
					{
						// The engine may have switched to another fake
						// profile (Zapret1): persist it and reflect it in
						// the selector before testing the combination.
						_syncFakeProfile();

						if (_automatically_strategy_cancel)
						{
							_ui->_unblock->stopService();
							break;
						}

						_ui->_unblock->startService(_technology);

						auto strategy_name = _ui->_unblock->getNameStrategies(_technology);
						auto version_str   = JSToCPP<std::string>(_select_version_strategy->getSelectedOptionValue());

						// On Zapret1 the same config is retried with every
						// fake profile, so the description names the active
						// one — otherwise iterations look like a stuck loop.
						std::string text_desc;
						if (_technology == Technology::Zapret1)
							text_desc = utils::format(
								Localization::Str{ "str_window_auto_start_wait_name_strategy_description_zapret1" }(),
								strategy_name,
								version_str,
								_ui->_unblock->fakeBinKey(_technology)
							);
						else
							text_desc = utils::format(
								Localization::Str{ "str_window_auto_start_wait_name_strategy_description" }(),
								strategy_name,
								version_str
							);

						text_desc.insert(0, "\n");
						text_desc.insert(0, Localization::Str{ _autoStartWaitDescription() }());

						_window_auto_start_wait->setDescription(text_desc);

					if (_technology == Technology::Zapret2)
					{
						// Passive round: no curl testing here at all. The helper
						// probes every LIST host through the running desync,
						// lua marks fully-tried hosts exhausted. The round ends
						// when every expected host is valid or exhausted —
						// event-driven, cancel is the escape hatch.
						auto expected_hosts = _ui->_unblock->testHostNames();
						const std::unordered_set<std::string> expected(expected_hosts.begin(), expected_hosts.end());

						// Drop stale datagrams from previous rounds.
						_ui->_unblock->helperCheckingHosts();
						_ui->_unblock->helperSeenHosts();
						_ui->_unblock->helperValidHosts();
						_ui->_unblock->helperErrorHosts();
						_ui->_unblock->helperExhaustedHosts();

						bool settled = expected.empty();
						std::string last_live;
						auto		  last_live_at = std::chrono::steady_clock::now() - std::chrono::seconds(10);
						while (!settled)
						{
							if (_automatically_strategy_cancel)
							{
								_ui->_unblock->stopService();
								break;
							}
							if (!_ui->_unblock->isRun(_technology))
								break; // engine died: nothing will verdict

							std::unordered_set<std::string> valid_set;
							for (auto& [host, _] : _ui->_unblock->helperValidHosts())
								valid_set.insert(host);
							std::unordered_set<std::string> exhausted_set;
							for (auto& [host, _] : _ui->_unblock->helperExhaustedHosts())
								exhausted_set.insert(host);
							auto checking = _ui->_unblock->helperCheckingHosts();
							auto errors	 = _ui->_unblock->helperErrorHosts();

							// DOM bridge from the worker: at most every 2s and
							// only on change, never a blind 2Hz hammer.
							const auto now = std::chrono::steady_clock::now();
							auto live		 = text_desc + "\n"
									  + utils::format(
										  Localization::Str{ "str_window_auto_start_wait_live" }(), valid_set.size(), checking.size(),
										  errors.size(), exhausted_set.size()
									  );
							if (live != last_live && now - last_live_at >= std::chrono::seconds(2))
							{
								last_live	 = std::move(live);
								last_live_at = now;
								_window_auto_start_wait->setDescription(last_live);
							}

							settled = autoRoundSettled(expected, valid_set, exhausted_set);
							if (!settled)
								std::this_thread::sleep_for(std::chrono::milliseconds(500));
						}

						size_t dead = 0;
						if (!_automatically_strategy_cancel && _ui->_unblock->isRun(_technology))
						{
							for (auto& [host, _] : _ui->_unblock->helperExhaustedHosts())
								if (expected.contains(host))
									++dead;
						}

						if (!_automatically_strategy_cancel && _ui->_unblock->isRun(_technology)
							&& judgeAutoRound(dead, expected.size()))
						{
							_ui->userConfig()->writeSectionParameter(_rememberSection(), "config", strategy_name);

							_window_continue_select_strategy->setDescription(
								utils::format(
									Localization::Str{ "str_window_continue_select_strategy_description" }(),
									strategy_name,
									version_str
								)
							);
							_window_continue_select_strategy->show();
							break;
						}
						// Otherwise the next strategy/version is tried.
					}
					else
					{
						_ui->_unblock->testingDomain();

						if (!_automatically_strategy_cancel && _ui->_unblock->validDomain())
						{
							_ui->userConfig()->writeSectionParameter(_rememberSection(), "config", strategy_name);

							// The winning combination includes the fake
							// profile (already synced above): name it so the
							// user knows what exactly worked.
							_window_continue_select_strategy->setDescription(
								utils::format(
									Localization::Str{ "str_window_auto_start_wait_name_strategy_description_zapret1" }(),
									strategy_name,
									version_str,
									_ui->_unblock->fakeBinKey(_technology)
								)
							);
							_window_continue_select_strategy->show();
							break;
						}
					}
				}

					_buttonUpdate();

					_automatically_strategy_cancel = false;
					_window_auto_start_wait->hide();
				}
			);
		}
	);
}

bool UiZapretPage::_autoStartTryNext() const
{
	if (_ui->_unblock->automaticallyStrategy(_technology))
		return true;

	auto strategy_dirs = _ui->_unblock->listVersionStrategy(_technology);
	if (strategy_dirs.empty())
		return false;

	auto it = std::ranges::find(strategy_dirs, JSToCPP<std::string>(_select_version_strategy->getSelectedOptionValue()));

	auto save_version = [this](std::string version)
	{
		_select_version_strategy->setSelectedOptionValue(version);
		_ui->_unblock->changeDirVersionStrategy(_technology, version);
		_ui->userConfig()->writeSectionParameter(_rememberSection(), "version_strategy", version);
	};

	if (it != strategy_dirs.end())
	{
		if (++it != strategy_dirs.end())
		{
			save_version(*it);
			return true;
		}
	}

	save_version(strategy_dirs.front());

	_window_configuration_selection_error->show();
	_window_configuration_selection_error->addEventOk(
		[this](JSArgs)
		{
			_window_configuration_selection_error->hide();

			_ui->getUiUnblock()->getWindowWaitStopService()->show();
			Core::get().addTask(
				[this]
				{
					_ui->_unblock->removeService();
					_ui->getUiUnblock()->getWindowWaitStopService()->hide();
				}
			);
			return true;
		}
	);

	return false;
}

void UiZapretPage::_startServiceFromConfig()
{
	Core::get().addTask(
		[this]
		{
			_ui->getWindowWaitStartService()->show();

			_tcpGlobalChange(true);

			_ui->_unblock->changeStrategy(_technology, JSToCPP(_select_config->getSelectedOptionValue()));

			_ui->_unblock->startService(_technology);
			_buttonUpdate();
			_ui->getWindowWaitStartService()->hide();
		}
	);
}

void UiZapretPage::_tcpGlobalChange(bool state) const
{
	if (!state)
	{
		system("netsh interface tcp set global timestamps=disabled");
		_ui->userConfig()->writeSectionParameter("SYSTEM", "enable_tcp_global", "false");
		return;
	}

	auto tcp_set_global = _ui->userConfig()->parameterSection<bool>("SYSTEM", "enable_tcp_global");
	if ((!tcp_set_global) || (!tcp_set_global.value()))
	{
		system("netsh interface tcp set global timestamps=enabled");
		_ui->userConfig()->writeSectionParameter("SYSTEM", "enable_tcp_global", "true");
	}
}

void UiZapretPage::_initTestingWindow()
{
	_window_wait_testing->create(Localization::Str{ "str_please_wait" }, "str_secondary_window_description_wait_domain");
	_window_wait_testing->setType(SecondaryWindow::Type::Wait);

	_list_host->create("#" + std::string{ _window_wait_testing->name() } + " .description", "str_h2_verified_domains");

	_window_wait_testing->addEventCancel(
		[this](JSArgs)
		{
			_ui->_unblock->testingDomainCancel();
			_domain_testing_cancel.store(true);
			return false;
		}
	);

	_window_info_testing->create(Localization::Str{ "str_window_info_testing" }, "str_secondary_window_description_info_domain");
	_window_info_testing->setType(SecondaryWindow::Type::OK);

	_window_info_testing->addEventOk(
		[this](JSArgs)
		{
			_window_info_testing->hide();
			_list_host_info->clear();
			return false;
		}
	);

	_list_host_info->create("#" + std::string{ _window_info_testing->name() } + " .description", "");
}

void UiZapretPage::_testingServiceDomains()
{
	_window_wait_testing->show();

	const Core::TaskId testTask = Core::get().addTask(
		[this]
		{
			_ui->_unblock->testingDomain(
				[this](std::string_view url, bool state)
				{
					_list_host->createLiSuccess(url, state);
					_list_host_info->createLiSuccess(url, state);
				},
				false
			);
		}
	);

	Core::get().taskComplete(
		testTask,
		[this]
		{
			_window_wait_testing->hide();
			_list_host->clear();

			if (!_domain_testing_cancel.load())
			{
				_list_host_info->setTitle(utils::format(Localization::Str{ "str_window_title_info_result" }(), _ui->_unblock->domainSuccessRate()));

				_window_info_testing->show();
				return;
			}

			_list_host_info->clear();

			_domain_testing_cancel.store(false);
		}
	);
}
