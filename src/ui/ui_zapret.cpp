#include "ui_zapret.h"

#include "ui.h"
#include "ui_base.h"
#include "../unblock/unblock.h"

UiZapret2::UiZapret2(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock)
    : _ui(std::move(ui)), _unblock(std::move(unblock))
{
    _file_service_list = std::make_shared<File>();
    _file_service_list->open({Core::get().configsPath() / "service_setting"}, ".config", true);
    _unblock->serviceConfigFile(_file_service_list);

    // Initialize services list from config
    _file_service_list->forLineParametersSection(
        "LIST",
        [this](std::string key, std::string /*value*/)
        {
            _list_enable_services.emplace(key, std::make_shared<CheckBox>(std::string{"_unblock_to_list_"} + key));
            return false;
        }
    );
}

void UiZapret2::initialize()
{
    _initMainControls();
    _testingInit();
    _selectStrategyVersion();
    _selectConfig();
    _listEnableServices();
}

void UiZapret2::_listEnableServices()
{
    for (auto& [name, check_box] : _list_enable_services)
    {
        check_box->create(
            "#zapret .service",
            std::string{"str_unblock_enable_" + name + "_title"},
            Localization::Str{std::string{"str_unblock_enable_" + name + "_description"}}
        );

        check_box->addEventClick(
            [this, name](JSArgs args)
            {
                _ui->uiBase()->userConfig()->writeSectionParameter("UNBLOCK", (std::string{"enable_"} + name),
                                                                   JSToCPP(args[0]));

                _listEnableServicesUpdate();
                return false;
            }
        );
    }

    _listEnableServicesUpdate();
}

void UiZapret2::_listEnableServicesUpdate()
{
    for (auto& [name, check_box] : _list_enable_services)
    {
        check_box->show();

        std::string setting_name{"enable_" + name};

        if (auto result = _ui->uiBase()->userConfig()->parameterSection<bool>("UNBLOCK", setting_name))
        {
            if (result.value())
                _unblock->addOptionalStrategies(name);

            check_box->setState(result.value());
        }
        else if (auto state = _file_service_list->parameterSection<bool>("LIST", name))
        {
            if (state.value())
                _unblock->addOptionalStrategies(name);

            check_box->setState(state.value());
        }
        else
            Debug::warning(state.error());

        if (check_box->getState())
            _unblock->addOptionalStrategies(name);
        else
            _unblock->removeOptionalStrategies(name);
    }
}

void UiZapret2::_selectStrategyVersion()
{
    _select_version_strategy
        ->create("#zapret .common", "str_select_version_strategy_title",
                 Localization::Str{"str_select_version_strategy_description"});
    _select_version_strategy->addEventChange(
        [this](JSArgs args)
        {
            _ui->uiBase()->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "version_strategy",
                                                               JSToCPP(args[1]));
            _selectStrategyVersionUpdate();
            return false;
        }
    );

    _selectStrategyVersionUpdate();
}

void UiZapret2::_selectStrategyVersionUpdate()
{
    if (!_select_version_strategy->isCreate())
        return;

    _select_version_strategy->clear();

    _select_version_strategy->show();

    auto strategy_dirs = _unblock->listVersionStrategy();

    for (u32 i = 0; i < strategy_dirs.size(); i++)
        _select_version_strategy->createOption(i, strategy_dirs[i]);

    if (auto strategy_version = _ui->uiBase()->userConfig()->parameterSection<std::string>(
        "REMEMBER_CONFIGURATION", "version_strategy"))
        _select_version_strategy->setSelectedOptionValue(strategy_version.value());

    _unblock->changeDirVersionStrategy(JSToCPP<std::string>(_select_version_strategy->getSelectedOptionValue()));

    _selectConfigUpdate();
}

void UiZapret2::_selectConfig()
{
    _select_config->create("#zapret .common", "str_select_config_title",
                           Localization::Str{"str_select_config_description"});
    _select_config->addEventChange(
        [this](JSArgs args)
        {
            _ui->uiBase()->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "config", JSToCPP(args[1]));
            return false;
        }
    );

    _selectConfigUpdate();
}

void UiZapret2::_selectConfigUpdate()
{
    if (!_select_config->isCreate())
        return;

    _select_config->clear();

    auto& strategies_list = _unblock->getStrategiesList();

    if (strategies_list.empty())
        return;

    _select_config->show();

    for (u32 i = 0; i < strategies_list.size(); i++)
        _select_config->createOption(i, strategies_list[i]);

    if (auto config = _ui->uiBase()->userConfig()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
    {
        if (std::ranges::find(strategies_list, config.value()) != strategies_list.end())
        {
            _select_config->setSelectedOptionValue(config.value());
            _ui->uiBase()->userConfig()
               ->writeSectionParameter("REMEMBER_CONFIGURATION", "config",
                                       JSToCPP(_select_config->getSelectedOptionValue()));
        }
        else
        {
            _ui->uiBase()->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "config", strategies_list[0]);
            _select_config->setSelectedOptionValue(strategies_list[0]);
        }
    }

    _buttonUpdate();
}

void UiZapret2::_initMainControls()
{
    // Initialize start button
    _start_button->create("#zapret .common", "str_b_start_zapret");

    _start_button->addEventClick(
        [this](JSArgs)
        {
            _clickStartService();
            return false;
        }
    );

    // Initialize windows
    _window_config_not_found->create(Localization::Str{"str_window_config_not_found_title"},
                                     "str_window_config_not_found_description");
    _window_config_not_found->setType(SecondaryWindow::Type::OK);
    _window_config_not_found->addEventOk(
        [this](JSArgs)
        {
            _autoStart();
            return false;
        }
    );

    _window_config_found->create(Localization::Str{"str_window_config_found_title"}, "");
    _window_config_found->setType(SecondaryWindow::Type::YesNo);
    _window_config_found->addEventYesNo(
        [this](JSArgs args)
        {
            if (args[0].ToBoolean())
                _autoStart();
            else
                _startServiceFromConfig();

            return false;
        }
    );

    _window_auto_start_wait->create(Localization::Str{"str_please_wait"}, "str_window_auto_start_wait_description");
    _window_auto_start_wait->setType(SecondaryWindow::Type::Wait);
    _window_auto_start_wait->addEventCancel(
        [this](JSArgs)
        {
            _automatically_strategy_cancel.store(true);
            _unblock->testingDomainCancel();
            return false;
        }
    );

    _window_continue_select_strategy->create(Localization::Str{"str_window_continue_select_strategy_title"}, "");
    _window_continue_select_strategy->setType(SecondaryWindow::Type::YesNo);
    _window_continue_select_strategy->addEventYesNo(
        [this](JSArgs args)
        {
            if (args[0].ToBoolean())
                _autoStart();

            return false;
        }
    );

    _window_configuration_selection_error->create(Localization::Str{"str_error"},
                                                  "str_window_configuration_selection_error");
    _window_configuration_selection_error->setType(SecondaryWindow::Type::OK);

    _stop_zapret->create("#zapret .common", "str_b_stop_zapret");
    _stop_zapret->addEventClick(
        [this](JSArgs)
        {
            _ui->getWindowWaitStopService()->show();
            Core::get().addTask(
                [this]
                {
                    _unblock->removeService();
                    _ui->getWindowWaitStopService()->hide();
                }
            );
            return false;
        }
    );

    _run_auto_config_zapret->create("#zapret .common", "str_b_run_auto_config_zapret");

    _run_auto_config_zapret->addEventClick(
        [this](JSArgs)
        {
            _autoStart();
            return false;
        }
    );
}

void UiZapret2::_testingInit()
{
    _start_testing_zapret->create("#zapret .common", "str_b_start_testing_zapret");
    _start_testing_zapret->addEventClick(
        [this](JSArgs)
        {
            if (_unblock->runTest())
                return false;

            _testingServiceDomains();
            return false;
        }
    );

    _initTestingWindow();
}

void UiZapret2::_buttonUpdate()
{
    if (auto config = _ui->uiBase()->userConfig()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
        if (_unblock->activeService())
            getStartButton()->setTitle("str_b_restart_unblock");
        else
            getStartButton()->setTitle("str_b_start_zapret");
    else
        getStartButton()->setTitle("str_b_start_find_config");
}

void UiZapret2::_clickStartService()
{
    if (auto config = _ui->uiBase()->userConfig()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
    {
        _startServiceFromConfig();

        /*auto& strategy_list = _unblock.getStrategiesList();
        if (std::ranges::find(strategy_list, config.value()) == strategy_list.end())
        {
            Debug::warning("config[{}] The specified strategy does not exist from the user's settings!", config.value());

            _ui->uiBase()->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "config", "");

            if (_unblock_select_config->isShow())
                _unblock_select_config->setSelectedOptionValue(strategy_list[0]);

            _window_config_not_found->show();
            return;
        }

        _window_config_found->setDescription(utils::format(
            Localization::Str{ "str_window_config_found_description" }(),
            config.value(),
            JSToCPP(_unblock_select_version_strategy->getSelectedOptionValue())
        ));

        _window_config_found->show();*/
        return;
    }

    _window_config_not_found->show();
}


void UiZapret2::_autoStart()
{
    _tcpGlobalChange(true);

    Core::get().addTask(
        [this]
        {
            InputConsole::textOk(Localization::Str{"str_beginning_auto_selection"}());

            _window_auto_start_wait->setDescription("str_window_auto_start_wait_description");
            _window_auto_start_wait->show();

            while (_autoStartTryNext())
            {
                if (_automatically_strategy_cancel)
                {
                    _unblock->stopService();
                    break;
                }

                _unblock->startService();

                auto strategy_name = _unblock->getNameStrategies();
                auto version_str = JSToCPP(_select_version_strategy->getSelectedOptionValue());

                auto text_desc =
                    utils::format(Localization::Str{"str_window_auto_start_wait_name_strategy_description"}(),
                                  strategy_name, version_str);

                text_desc.insert(0, "\n");
                text_desc.insert(0, Localization::Str{"str_window_auto_start_wait_description"}());

                _window_auto_start_wait->setDescription(text_desc);

                _unblock->testingDomain();

                if (!_automatically_strategy_cancel && _unblock->validDomain())
                {
                    _ui->uiBase()->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "config",
                                                                       strategy_name);

                    _window_continue_select_strategy->setDescription(
                        utils::format(Localization::Str{"str_window_continue_select_strategy_description"}(),
                                      strategy_name, version_str));
                    _window_continue_select_strategy->show();
                    break;
                }
            }

            _buttonUpdate();

            _automatically_strategy_cancel = false;
            _window_auto_start_wait->hide();
        }
    );
}

bool UiZapret2::_autoStartTryNext() const
{
    if (_unblock->automaticallyStrategy())
        return true;

    auto strategy_dirs = _unblock->listVersionStrategy();

    auto it = std::ranges::find(strategy_dirs,
                                JSToCPP<std::string>(_select_version_strategy->getSelectedOptionValue()));

    auto save_version = [this](std::string version)
    {
        _select_version_strategy->setSelectedOptionValue(version);
        _unblock->changeDirVersionStrategy(version);
        _ui->uiBase()->userConfig()->writeSectionParameter("REMEMBER_CONFIGURATION", "version_strategy", version);
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
            return true;
        }
    );

    return false;
}

void UiZapret2::_startServiceFromConfig()
{
    Core::get().addTask(
        [this]
        {
            _ui->getWindowWaitStartService()->show();

            _tcpGlobalChange(true);

            if (_select_config->isShow())
                _unblock->changeStrategy(JSToCPP(_select_config->getSelectedOptionValue()));
            else if (auto config = _ui->uiBase()->userConfig()->parameterSection<std::string>(
                "REMEMBER_CONFIGURATION", "config"))
                _unblock->changeStrategy(config.value());
            else
                Debug::fatal("REMEMBER_CONFIGURATION parameter config = null");

            _unblock->startService();
            _buttonUpdate();
            _ui->getWindowWaitStartService()->hide();
        }
    );
}

void UiZapret2::_tcpGlobalChange(bool state) const
{
    if (state == true)
    {
        auto tcp_set_global = _ui->uiBase()->userConfig()->parameterSection<bool>("SYSTEM", "enable_tcp_global");
        if ((!tcp_set_global) || (!tcp_set_global.value()))
        {
            system("netsh interface tcp set global timestamps=enabled");
            _ui->uiBase()->userConfig()->writeSectionParameter("SYSTEM", "enable_tcp_global", "true");
        }

        return;
    }

    system("netsh interface tcp set global timestamps=disabled");
    _ui->uiBase()->userConfig()->writeSectionParameter("SYSTEM", "enable_tcp_global", "false");
}

void UiZapret2::_initTestingWindow()
{
    _window_wait_testing->create(Localization::Str{"str_please_wait"}, "str_secondary_window_description_wait_domain");
    _window_wait_testing->setType(SecondaryWindow::Type::Wait);

    _list_domain->create("#_window_wait_testing .description", "str_h2_verified_domains");

    _window_wait_testing->addEventCancel(
        [this](JSArgs)
        {
            _unblock->testingDomainCancel();
            return false;
        }
    );

    if (_ui->getTestingDomainsStartup()->getState())
        _testingServiceDomains();
}

void UiZapret2::_testingServiceDomains()
{
    _window_wait_testing->show();

    Core::get().addTaskParallel(
        [this]
        {
            _unblock->testingDomain(
                [this](std::string_view url, bool state)
                {
                    _list_domain->createLiSuccess(url, state);
                },
                false
            );
        }
    );

    Core::get().taskComplete(
        [this]
        {
            _window_wait_testing->hide();
            _list_domain->clear();
        }
    );
}
