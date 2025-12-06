#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

void Ui::_settingInit()
{
	_settingShowConsole();
	_settingTestDomainsStartup();
	_settingAccurateTesting();
	_settingMaxTimeWait();

	_settingUnblockEnable();
	_settingUnblockListEnableServices();
	_settingUnblockFilteringTopLevelDomains();
	_settingUnblockEnableManual();
	_settingUnblockEnableManualSelect();

	_settingProxyDPIEnable();
	_settingProxyDPIInputIP();
	_settingProxyDPIInputPort();
	_settingProxyDPIManualEnable();
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
	_testing_domains_startup->setState(result ? result.value() : true);

	_testing_domains_startup->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("TESTING", "startup", JSToCPP(args[0]));
			return false;
		}
	);
}

void Ui::_settingAccurateTesting()
{
	_accurate_testing
		->create("#setting section .common", "str_checkbox_accurate_testing_title", Localization::Str{ "str_checkbox_accurate_testing_description" });

	auto result = _ui_base->userSetting()->parameterSection<bool>("TESTING", "accurate");
	_accurate_testing->setState(result ? result.value() : false);

	_unblock->accurateTesting(_accurate_testing->getState());

	_accurate_testing->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("TESTING", "accurate", JSToCPP(args[0]));
			_unblock->accurateTesting(args[0].ToBoolean());
			_settingMaxTimeWaitUpdate();
			return false;
		}
	);
}

void Ui::_settingMaxTimeWait()
{
	_max_time_wait_testing
		->create("#setting section .common", Input::Types::number, 5, "str_input_max_wait_testing_title", "str_input_max_wait_testing_description");
	_max_time_wait_testing->addEventSubmit(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("TESTING", "max_time_wait_testing", JSToCPP(args[0]));
			_settingMaxTimeWaitUpdate();
			return false;
		}
	);

	_max_time_wait_accurate_testing->create(
		"#setting section .common",
		Input::Types::number,
		10,
		"str_input_max_wait_accurate_testing_title",
		"str_input_max_wait_accurate_testing_description"
	);
	_max_time_wait_accurate_testing->addEventSubmit(
		[this](JSArgs args)
		{
			_ui_base->userSetting()
				->writeSectionParameter("TESTING", "max_time_wait_accurate_testing", JSToCPP(args[0]));
			_settingMaxTimeWaitUpdate();
			return false;
		}
	);

	_settingMaxTimeWaitUpdate();
}

void Ui::_settingMaxTimeWaitUpdate()
{
	if (_accurate_testing->getState())
	{
		_max_time_wait_accurate_testing->show();
		
		// get user setting
		auto result = _ui_base->userSetting()->parameterSection<u32>("TESTING", "max_time_wait_accurate_testing");
		if (result)
			_max_time_wait_accurate_testing->setValue(result.value());

		_unblock->maxWaitAccurateTesting(_max_time_wait_accurate_testing->getValue());

		_max_time_wait_testing->hide();
	}
	else
	{
		_max_time_wait_testing->show();
		
		// get user setting
		auto result = _ui_base->userSetting()->parameterSection<u32>("TESTING", "max_time_wait_testing");
		if (result)
			_max_time_wait_testing->setValue(result.value());

		_unblock->maxWaitTesting(_max_time_wait_testing->getValue());

		_max_time_wait_accurate_testing->hide();
	}
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
			_settingUnblockFilteringTopLevelDomainsUpdate();
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
			std::string{ "str_unblock_enable_" + name + "_title" }.c_str(),
			Localization::Str{ std::string{ "str_unblock_enable_" + name + "_description" }.c_str() }
		);

		check_box->addEventClick(
			[this, name](JSArgs args)
			{
				_ui_base->userSetting()->writeSectionParameter(
					"UNBLOCK",
					(std::string{ "enable_" } + name).c_str(), JSToCPP(args[0])
				);

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

		auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", setting_name.c_str());
		if (result)
		{
			if (result.value())
				_unblock->addOptionalStrategies(name);

			check_box->setState(result.value());
		}
		else
		{
			auto state = _file_service_list->parameterSection<bool>("LIST", name.c_str());
			if (state)
			{
				if (state.value())
					_unblock->addOptionalStrategies(name);

				check_box->setState(state.value());
			}
			else
				Debug::warning(state.error().c_str());
		}

		if (check_box->getState())
			_unblock->addOptionalStrategies(name);
		else
			_unblock->removeOptionalStrategies(name);
	}
}

void Ui::_settingUnblockFilteringTopLevelDomains()
{
	_unblock_filtering_top_level_domains->create(
		"#setting section .unblock",
		"str_unblock_filtering_top_level_domains_title",
		Localization::Str{ "str_unblock_filtering_top_level_domains_description" }
	);

	_unblock_filtering_top_level_domains->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("UNBLOCK", "filtering_top_level_domains", JSToCPP(args[0]));
			_unblock->changeFilteringTopLevelDomains(args[0].ToBoolean());
			return false;
		}
	);

	_settingUnblockFilteringTopLevelDomainsUpdate();
}

void Ui::_settingUnblockFilteringTopLevelDomainsUpdate()
{
	if (_unblock_enable->getState())
	{
		_unblock_filtering_top_level_domains->show();
		auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", "filtering_top_level_domains");
		_unblock_filtering_top_level_domains->setState(result ? result.value() : false);
		_unblock->changeFilteringTopLevelDomains(_unblock_filtering_top_level_domains->getState());
		return;
	}

	_unblock_filtering_top_level_domains->hide();
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
	auto set_new_value = [this](Ptr<SelectList>& select, pcstr set_val, pcstr check_val, JSArgs args)
	{
		_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", set_val, JSToCPP(args[1]));

		_ui_base->userSetting()->writeSectionParameter(
			"REMEMBER_CONFIGURATION",
			check_val, JSToCPP(select->getSelectedOptionValue())
		);
	};

	_unblock_select_config->create("#setting section .unblock", "str_select_config_title", Localization::Str{ "str_select_config_description" });
	_unblock_select_config->addEventChange(
		[this, set_new_value](JSArgs args)
		{
			set_new_value(_unblock_select_fake_bin, "config", "fake_bin", args);
			return false;
		}
	);

	_unblock_select_fake_bin
		->create("#setting section .unblock", "str_unblock_select_fake_bin_title", Localization::Str{ "str_unblock_select_fake_bin_description" });
	_unblock_select_fake_bin->addEventChange(
		[this, set_new_value](JSArgs args)
		{
			set_new_value(_unblock_select_config, "fake_bin", "config", args);
			return false;
		}
	);

	_settingUnblockEnableManualSelectUpdate();
}

void Ui::_settingUnblockEnableManualSelectUpdate()
{
	if ((!_unblock_select_config->isCreate()) || (!_unblock_select_fake_bin->isCreate()))
		return;

	if (_unblock_enable->getState() && _unblock_manual->getState())
	{
		_unblock_select_config->clear();
		_unblock_select_fake_bin->clear();

		_unblock_select_config->show();
		_unblock_select_fake_bin->show();

		auto& strategies_list = _unblock->getStrategiesList<StrategiesDPI>();
		for (u32 i = 0; i < strategies_list.size(); i++)
		{
			auto& file_name = strategies_list[i];
			_unblock_select_config->createOption(i, file_name.c_str());
		}

		auto set_default_select = [this](Ptr<SelectList>& select, pcstr name)
		{
			if (auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", name))
				select->setSelectedOptionValue(config.value().c_str());
			else
				_ui_base->userSetting()->writeSectionParameter(
					"REMEMBER_CONFIGURATION",
					name, JSToCPP(select->getSelectedOptionValue())
				);
		};

		set_default_select(_unblock_select_config, "config");

		_buttonUpdate();

		auto& fake_bin_list = _unblock->getFakeBinList();
		for (u32 i = 0; i < fake_bin_list.size(); i++)
		{
			auto& fake_bin = fake_bin_list[i];
			_unblock_select_fake_bin->createOption(i, fake_bin.key.c_str());
		}

		set_default_select(_unblock_select_fake_bin, "fake_bin");

		return;
	}

	_unblock_select_config->hide();
	_unblock_select_fake_bin->hide();

	_unblock_select_config->clear();
	_unblock_select_fake_bin->clear();
}

void Ui::_settingProxyDPIEnable()
{
	_proxy_enable->create("#setting section .proxy", "str_checkbox_proxy_enable_title", Localization::Str{ "str_checkbox_proxy_enable_description" });

	auto result = _ui_base->userSetting()->parameterSection<bool>("PROXY", "enable");
	_proxy_enable->setState(result ? result.value() : false);

	_proxy_enable->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("PROXY", "enable", JSToCPP(args[0]));

			_settingProxyDPIManualEnableUpdate();
			_buttonUpdate();
			_testingUpdate();

			return false;
		}
	);
}

void Ui::_settingProxyDPIManualEnable()
{
	_proxy_manual->create("#setting section .proxy", "str_manual_title", Localization::Str{ "str_manual_description" });

	_proxy_manual->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("PROXY", "manual", JSToCPP(args[0]));
			_settingProxyDPIManualEnableUpdate();
			return false;
		}
	);

	_settingProxyDPIManualEnableUpdate();
}

void Ui::_settingProxyDPIManualEnableUpdate()
{
	if (_proxy_enable->getState())
	{
		_proxy_manual->show();
		auto result = _ui_base->userSetting()->parameterSection<bool>("PROXY", "manual");
		_proxy_manual->setState(result ? result.value() : false);
		return;
	}

	_proxy_manual->hide();
}

void Ui::_settingProxyDPISelectConfig()
{
	_proxy_select_config->create("#setting section .proxy", "str_select_config_title", Localization::Str{ "str_select_config_description" });

	_proxy_select_config->addEventChange(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", "config_proxy", JSToCPP(args[1]));
			_settingProxyDPISelectConfigUpdate();
			return false;
		}
	);

	_settingProxyDPISelectConfigUpdate();
}

void Ui::_settingProxyDPISelectConfigUpdate()
{
	if (_proxy_enable->getState() && _proxy_manual->getState())
	{
		_proxy_select_config->show();

		auto& strategies_list = _unblock->getStrategiesList<ProxyStrategiesDPI>();
		for (u32 i = 0; i < strategies_list.size(); i++)
		{
			auto& file_name = strategies_list[i];
			_proxy_select_config->createOption(i, file_name.c_str());
		}

		if (auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy"))
			_proxy_select_config->setSelectedOptionValue(config.value().c_str());
		else
			_ui_base->userSetting()->writeSectionParameter(
				"REMEMBER_CONFIGURATION",
				"config_proxy",
				JSToCPP(_proxy_select_config->getSelectedOptionValue()
				)
			);

		_buttonUpdate();
		return;
	}

	_proxy_select_config->hide();
}

void Ui::_settingProxyDPIInputIP()
{
	_proxy_ip->create("#setting section .proxy", Input::Types::ip, "127.0.0.1", "str_input_proxy_ip_title", "str_input_proxy_ip_description");
	if (auto ip = _ui_base->userSetting()->parameterSection<pcstr>("PROXY", "ip"))
		_proxy_ip->setValue(ip.value());

	_proxy_ip->addEventSubmit(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("PROXY", "ip", JSToCPP(args[0]));
			_settingProxyDPIInputIPUpdate();
		return false;
	});

	_settingProxyDPIInputIPUpdate();
}

void Ui::_settingProxyDPIInputIPUpdate()
{
	if (_proxy_enable->getState())
	{
		_proxy_ip->show();

		if (auto ip = _ui_base->userSetting()->parameterSection<pcstr>("PROXY", "ip"))
			_proxy_ip->setValue(ip.value());

		_unblock->changeProxyIP(JSToCPP(_proxy_ip->getValue()));
		return;
	}

	_proxy_ip->hide();
}

void Ui::_settingProxyDPIInputPort()
{
	_proxy_port->create("#setting section .proxy", Input::Types::number, "1080", "str_input_proxy_port_title", "str_input_proxy_port_description");

	_proxy_port->addEventSubmit(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("PROXY", "port", JSToCPP(args[0]));
			_settingProxyDPIInputPortUpdate();
			return false;
		}
	);

	_settingProxyDPIInputPortUpdate();
}

void Ui::_settingProxyDPIInputPortUpdate()
{
	if (_proxy_enable->getState())
	{
		_proxy_port->show();
		if (auto port = _ui_base->userSetting()->parameterSection<u32>("PROXY", "port"))
			_proxy_port->setValue(port.value());

		_unblock->changeProxyPort(JSToCPP<u32>(_proxy_port->getValue()));
		return;
	}

	_proxy_port->hide();
}