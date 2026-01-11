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
	_settingUnblockFilteringTopLevelDomains();

	_settingUnblockEnableManual();
	_settingUnblockEnableManualSelect();
	_settingUnblockSelectStrategyVersion();

	_settingProxyDPIEnable();
	_settingProxyDPIInputIP();
	_settingProxyDPIInputPort();
	_settingProxyDPIManualEnable();
	_settingProxyDPISelectConfig();
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
	_testing_domains_startup->setState(result ? result.value() : true);

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
	Localization::Str window_description{ "str_window_to_warn_enable_dns_hosts_description" };
	std::string		  description = window_description();

	std::string str_list_name{};

	auto& list_name = _unblock->dnsHostsListName();
	for (auto& name : list_name)
		str_list_name.append(name).append(", ");

	str_list_name.pop_back();
	str_list_name.pop_back();

	_window_to_warn_enable_dns_hosts->create(Localization::Str{ "str_warning" }, utils::format(description.c_str(), str_list_name.c_str()).c_str());

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
			_unblock->dnsHostsCancelUpdate();
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
					_unblock->dnsHostsUpdate();
					_unblock->dnsHosts(false);
					_unblock->dnsHosts(true);
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
			float			   progress = _unblock->dnsHostsUpdateProgress();
			_window_wait_update_dns->setDescription(utils::format(disc_text.c_str(), progress).c_str());
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
				if (state && (!_unblock->dnsHostsCheck()))
				{
					_window_wait_update_dns->show();
					_unblock->dnsHostsUpdate();
					_window_wait_update_dns->hide();
				}

				_unblock->dnsHosts(state);
			}
		);
	}
	else
	{
		_start_update_dns_hosts->hide();
		_window_to_warn_enable_dns_hosts->show();
	}
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

	_settingMaxTimeWaitUpdate();
}

void Ui::_settingMaxTimeWaitUpdate()
{
	// get user setting
	u32	 second = 5;
	auto result = _ui_base->userSetting()->parameterSection<u32>("TESTING", "max_time_wait_testing");
	if (result)
	{
		second = result.value();
		if (second > 0)
			_max_time_wait_testing->setValue(second);
		else
			_max_time_wait_testing->setValue(5);
	}

	_unblock->maxWaitTesting(second);
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
				_ui_base->userSetting()->writeSectionParameter("UNBLOCK", (std::string{ "enable_" } + name).c_str(), JSToCPP(args[0]));

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

		static std::vector<std::string> strategy_dirs{};

		auto patch_dir = Core::get().configsPath() / "strategy";
		for (auto& entry : std::filesystem::directory_iterator(patch_dir))
			strategy_dirs.push_back(entry.path().filename().string());

		std::sort(
			strategy_dirs.begin(),
			strategy_dirs.end(),
			[](const std::string& left, const std::string& right)
			{
				static std::regex reg{ "\\." };
				return std::stoul(std::regex_replace(left, reg, "")) > std::stoul(std::regex_replace(right, reg, ""));
			}
		);

		for (u32 i = 0; i < strategy_dirs.size(); i++)
			_unblock_select_version_strategy->createOption(i, strategy_dirs[i].c_str());

		if (auto strategy_version = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "version_strategy"))
			_unblock_select_version_strategy->setSelectedOptionValue(strategy_version.value().c_str());

		_unblock->changeDirVersionStrategy(JSToCPP<std::string>(_unblock_select_version_strategy->getSelectedOptionValue()));

		strategy_dirs.clear();
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
	auto set_new_value = [this](Ptr<SelectList>& select, pcstr set_val, pcstr check_val, JSArgs args)
	{
		_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", set_val, JSToCPP(args[1]));

		_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", check_val, JSToCPP(select->getSelectedOptionValue()));
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

		auto& strategies_list = _unblock->getStrategiesList<StrategiesDPI>();

		if (strategies_list.empty())
			return;

		_unblock_select_config->show();
		_unblock_select_fake_bin->show();

		for (u32 i = 0; i < strategies_list.size(); i++)
			_unblock_select_config->createOption(i, strategies_list[i].c_str());

		auto set_default_select = [this](Ptr<SelectList>& select, pcstr name)
		{
			if (auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", name))
				select->setSelectedOptionValue(config.value().c_str());
			else
				_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", name, JSToCPP(select->getSelectedOptionValue()));
		};

		set_default_select(_unblock_select_config, "config");

		_buttonUpdate();

		u32	  size{ 0 };
		auto& fake_bin_list = _unblock->getFakeBinList();
		for (auto& [key, _] : fake_bin_list)
			_unblock_select_fake_bin->createOption(size++, key.c_str());

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
			_settingProxyDPIInputIPUpdate();
			_settingProxyDPIInputPortUpdate();
			_settingProxyDPIManualEnableUpdate();
			_settingProxyDPISelectConfigUpdate();
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
			_settingProxyDPISelectConfigUpdate();
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
			_ui_base->userSetting()
				->writeSectionParameter("REMEMBER_CONFIGURATION", "config_proxy", JSToCPP(_proxy_select_config->getSelectedOptionValue()));

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
		}
	);

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
