#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

void Ui::_settingInit()
{
	_settingShowConsole();
	_settingTestDomainsStartup();
	_settingEnableDnsHosts();
	_settingMaxTimeWait();

	_settingUnblockEnable();
	_settingUnblockListEnableServices();

	_settingUnblockEnableManual();
	_settingUnblockEnableManualSelect();
	_settingUnblockSelectStrategyVersion();

}

void Ui::_settingShowConsole()
{
#ifndef DEBUG
	{
		_show_console
			->create("#setting section .common", "str_checkbox_show_console_title", Localization::Str{ "str_checkbox_show_console_description" });

		auto result = _ui_base->userSetting()->parameterSection<bool>("SUSTEM", "show_console");
		_show_console->setState(result ? result.value() : false);

		_show_console->addEventClick(
			[this](JSArgs args)
			{
				_ui_base->console(JSToCPP<bool>(args[0]));
				_ui_base->userSetting()->writeSectionParameter("SUSTEM", "show_console", JSToCPP(args[0]));
				return false;
			}
		);
	}
#endif
}

void Ui::_settingTestDomainsStartup()
{
	_testing_domains_startup
		->create("#setting section .common", "str_checkbox_testing_startup_title", Localization::Str{ "str_checkbox_testing_startup_description" });

	auto result = _ui_base->userSetting()->parameterSection<bool>("TESTING", "startup");
	_testing_domains_startup->setState(result ? result.value() : false);

	_testing_domains_startup->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("TESTING", "startup", JSToCPP(args[0]));
			return false;
		}
	);
}

void Ui::_settingEnableDnsHosts()
{
	_window_to_warn_enable_dns_hosts->create(Localization::Str{ "str_warning" }, "");

	_window_to_warn_enable_dns_hosts->setType(SecondaryWindow::Type::YesNo);
	_window_to_warn_enable_dns_hosts->addEventYesNo(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("SUSTEM", "enable_dns_hosts", JSToCPP(args[0]));
			_settingEnableDnsHostsUpdate();
			_window_to_warn_enable_dns_hosts->hide();
			return false;
		}
	);

	_window_wait_update_dns->create(Localization::Str{ "str_please_wait" }, "");

	_window_wait_update_dns->setType(SecondaryWindow::Type::Wait);
	_window_wait_update_dns->addEventCancel(
		[this](JSArgs)
		{
			_unblock.dnsHostsCancelUpdate();
			return false;
		}
	);

	_enable_dns_hosts
		->create("#setting section .common", "str_checkbox_enable_dns_hosts_title", Localization::Str{ "str_checkbox_enable_dns_hosts_description" });
	_enable_dns_hosts->addEventClick(
		[this](JSArgs args)
		{
			if (JSToCPP<bool>(args[0]))
			{
				static Localization::Str window_description{ "str_window_to_warn_enable_dns_hosts_description" };
				static std::string_view description = window_description();
				static std::string str_list_name{};

				if (str_list_name.empty())
					_window_wait_response_from_server->show();

				Core::get().addTask(
					[this]
					{
						if (str_list_name.empty())
						{
							auto& list_name = _unblock.dnsHostsListName();
							for (auto& name : list_name)
								str_list_name.append(name).append(", ");

							_window_wait_response_from_server->hide();

							str_list_name.pop_back();
							str_list_name.pop_back();

							_window_to_warn_enable_dns_hosts->setDescription(utils::format(description, str_list_name));
						}
					}
				);

				_window_to_warn_enable_dns_hosts->show();
				return false;
			}

			_ui_base->userSetting()->writeSectionParameter("SUSTEM", "enable_dns_hosts", "false");
			_settingEnableDnsHostsUpdate();
			return false;
		}
	);

	_start_update_dns_hosts->create("#setting section .common", "str_button_start_dns_hosts_update_title");

	_start_update_dns_hosts->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask(
				[this]
				{
					_window_wait_update_dns->show();
					_unblock.dnsHostsUpdate();
					_unblock.dnsHosts(false);
					_unblock.dnsHosts(true);
					_window_wait_update_dns->hide();
				}
			);
			return false;
		}
	);

	_settingEnableDnsHostsUpdate();
}

void Ui::_settingDnsHostsUpdateInfoWindow()
{
	LIMIT_UPDATE(Description, .5f, {
		if (_window_wait_update_dns->isShow())
		{
			static std::string disc_text{ Localization::Str{ "str_window_wait_update_dns_description" }() };
			float			   progress = _unblock.dnsHostsUpdateProgress();
			_window_wait_update_dns->setDescription(utils::format(disc_text, progress));
		}
	});
}

void Ui::_settingEnableDnsHostsUpdate()
{
	auto result = _ui_base->userSetting()->parameterSection<bool>("SUSTEM", "enable_dns_hosts");
	if (result)
	{
		const bool state = result.value();
		_enable_dns_hosts->setState(state);

		if (state)
			_start_update_dns_hosts->show();
		else
			_start_update_dns_hosts->hide();

		Core::get().addTask(
			[this, state]
			{
				if (state && (!_unblock.dnsHostsCheck()))
				{
					_window_wait_update_dns->show();
					_unblock.dnsHostsUpdate();
					_window_wait_update_dns->hide();
				}

				_unblock.dnsHosts(state);
			}
		);
	}
	else
	{
		_start_update_dns_hosts->hide();
		_window_to_warn_enable_dns_hosts->show();
	}
}

#if __clang__
[[clang::no_destroy]]
#endif
constexpr static u32 base_second{ 8 };

void Ui::_settingMaxTimeWait()
{
	_max_time_wait_testing->create(
		"#setting section .common",
		Input::Types::number,
		base_second,
		"str_input_max_wait_testing_title",
		"str_input_max_wait_testing_description"
	);
	_max_time_wait_testing->addEventSubmit(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("TESTING", "max_time_wait_testing", JSToCPP(args[0]));
			_settingMaxTimeWaitUpdate();
			return false;
		}
	);

	_settingMaxTimeWaitUpdate();
}

void Ui::_settingMaxTimeWaitUpdate()
{
	// get user setting
	u32	 second = base_second;
	auto result = _ui_base->userSetting()->parameterSection<u32>("TESTING", "max_time_wait_testing");
	if (result)
	{
		second = result.value();
		if (second > 0)
			_max_time_wait_testing->setValue(second);
		else
			_max_time_wait_testing->setValue(base_second);
	}

	_unblock.maxWaitTesting(second);
}

void Ui::_settingUnblockEnable()
{
	_unblock_enable->create("#setting section .unblock", "str_unblock_enable_title", Localization::Str{ "str_unblock_enable_description" });

	auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", "enable");
	_unblock_enable->setState(result ? result.value() : true);

	_unblock_enable->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("UNBLOCK", "enable", JSToCPP(args[0]));
			_settingUnblockListEnableServicesUpdate();
			_settingUnblockEnableManualUpdate();
			_buttonUpdate();
			_testingUpdate();
			return false;
		}
	);
}

void Ui::_settingUnblockListEnableServices()
{
	for (auto& [name, check_box] : _unblock_list_enable_services)
	{
		check_box->create(
			"#setting section .unblock",
			std::string{ "str_unblock_enable_" + name + "_title" },
			Localization::Str{ std::string{ "str_unblock_enable_" + name + "_description" } }
		);

		check_box->addEventClick(
			[this, name](JSArgs args)
			{
				_ui_base->userSetting()->writeSectionParameter("UNBLOCK", (std::string{ "enable_" } + name), JSToCPP(args[0]));

				_settingUnblockListEnableServicesUpdate();
				return false;
			}
		);
	}

	_settingUnblockListEnableServicesUpdate();
}

void Ui::_settingUnblockListEnableServicesUpdate()
{
	for (auto& [name, check_box] : _unblock_list_enable_services)
	{
		if (_unblock_enable->getState())
			check_box->show();
		else
		{
			check_box->hide();
			continue;
		}

		std::string setting_name{ "enable_" + name };

		if (auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", setting_name))
		{
			if (result.value())
				_unblock.addOptionalStrategies(name);

			check_box->setState(result.value());
		}
		else if (auto state = _file_service_list->parameterSection<bool>("LIST", name))
		{
			if (state.value())
				_unblock.addOptionalStrategies(name);

			check_box->setState(state.value());
		}
		else
			Debug::warning(state.error());

		if (check_box->getState())
			_unblock.addOptionalStrategies(name);
		else
			_unblock.removeOptionalStrategies(name);
	}
}

void Ui::_settingUnblockSelectStrategyVersion()
{
	_unblock_select_version_strategy
		->create("#setting section .unblock", "str_select_version_strategy_title", Localization::Str{ "str_select_version_strategy_description" });
	_unblock_select_version_strategy->addEventChange(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", "version_strategy", JSToCPP(args[1]));
			_settingUnblockSelectStrategyVersionUpdate();
			return false;
		}
	);

	_settingUnblockSelectStrategyVersionUpdate();
}

void Ui::_settingUnblockSelectStrategyVersionUpdate()
{
	if (!_unblock_select_version_strategy->isCreate())
		return;

	if (_unblock_enable->getState())
	{
		_unblock_select_version_strategy->clear();

		_unblock_select_version_strategy->show();

		auto strategy_dirs = _unblock.listVersionStrategy();

		for (u32 i = 0; i < strategy_dirs.size(); i++)
			_unblock_select_version_strategy->createOption(i, strategy_dirs[i]);

		if (auto strategy_version = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "version_strategy"))
			_unblock_select_version_strategy->setSelectedOptionValue(strategy_version.value());

		_unblock.changeDirVersionStrategy(JSToCPP<std::string>(_unblock_select_version_strategy->getSelectedOptionValue()));
	}

	_settingUnblockEnableManualSelectUpdate();
}

void Ui::_settingUnblockEnableManual()
{
	_unblock_manual->create("#setting section .unblock", "str_manual_title", Localization::Str{ "str_manual_description" });

	_unblock_manual->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("UNBLOCK", "manual", JSToCPP(args[0]));
			_settingUnblockEnableManualSelectUpdate();
			return false;
		}
	);

	_settingUnblockEnableManualUpdate();
}

void Ui::_settingUnblockEnableManualUpdate()
{
	if (_unblock_enable->getState())
	{
		_unblock_manual->show();
		auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", "manual");
		_unblock_manual->setState(result ? result.value() : false);
	}
	else
		_unblock_manual->hide();

	_settingUnblockEnableManualSelectUpdate();
}

void Ui::_settingUnblockEnableManualSelect()
{
	_unblock_select_config->create("#setting section .unblock", "str_select_config_title", Localization::Str{ "str_select_config_description" });
	_unblock_select_config->addEventChange(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", "config", JSToCPP(args[1]));
			return false;
		}
	);

	_settingUnblockEnableManualSelectUpdate();
}

void Ui::_settingUnblockEnableManualSelectUpdate()
{
	if (!_unblock_select_config->isCreate())
		return;

	if (_unblock_enable->getState() && _unblock_manual->getState())
	{
		_unblock_select_config->clear();

		auto& strategies_list = _unblock.getStrategiesList();

		if (strategies_list.empty())
			return;

		_unblock_select_config->show();

		for (u32 i = 0; i < strategies_list.size(); i++)
			_unblock_select_config->createOption(i, strategies_list[i]);


		if (auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
			_unblock_select_config->setSelectedOptionValue(config.value());
		else
			_ui_base->userSetting()
				->writeSectionParameter("REMEMBER_CONFIGURATION", "config", JSToCPP(_unblock_select_config->getSelectedOptionValue()));

		_buttonUpdate();

		return;
	}

	_unblock_select_config->hide();
	_unblock_select_config->clear();
}
