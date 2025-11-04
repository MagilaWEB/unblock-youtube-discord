#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_setting()
{
	// Show window console.
#ifndef DEBUG
	{
		_show_console
			->create("#setting section .common", "str_checkbox_show_console_title", Localization::Str{ "str_checkbox_show_console_description" });

		auto result = _file_user_setting->parameterSection<bool>("SUSTEM", "show_console");
		_show_console->setState(result ? result.value() : false);

		_show_console->addEventClick(
			[this](JSArgs args)
			{
				_file_user_setting->writeSectionParameter("SUSTEM", "show_console", static_cast<String>(args[0].ToString()).utf8().data());
				return false;
			}
		);
	}
#endif

	auto createSelect = [this]
	{
		_unblock_select_config->remove();
		_unblock_select_fake_bin->remove();

		if (_unblock_manual->getState())
		{
			_unblock_select_config
				->create("#setting section .unblock", "str_select_config_title", Localization::Str{ "str_select_config_description" });

			auto& strategies_list = _unblock->getStrategiesList<StrategiesDPI>();
			for (u32 i = 0; i < strategies_list.size(); i++)
			{
				auto& file_name = strategies_list[i];
				_unblock_select_config->createOption(i, file_name.c_str());
			}
			
			auto set_default_select = [this](Ptr<SelectList>& select, pcstr name)
			{
				if (auto config = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", name))
					select->setSelectedOptionValue(config.value().c_str());
				else
					_file_user_setting->writeSectionParameter(
						"REMEMBER_CONFIGURATION",
						name,
						static_cast<String>(select->getSelectedOptionValue().ToString()).utf8().data()
					);
			};

			if (_unblock->activeService())
				_start_service->setTitle("str_b_restart_service");
			else
				_start_service->setTitle("str_b_start_service");

			auto set_new_value = [this](Ptr<SelectList>& select, pcstr set_val, pcstr check_val, JSArgs args)
			{
				_file_user_setting->writeSectionParameter("REMEMBER_CONFIGURATION", set_val, static_cast<String>(args[1].ToString()).utf8().data());

				_file_user_setting->writeSectionParameter(
					"REMEMBER_CONFIGURATION",
					check_val,
					static_cast<String>(select->getSelectedOptionValue().ToString()).utf8().data()
				);
			};

			set_default_select(_unblock_select_config, "config");

			_unblock_select_config->addEventChange(
				[=](JSArgs args)
				{
					set_new_value(_unblock_select_fake_bin, "config", "fake_bin", args);
					return false;
				}
			);

			_unblock_select_fake_bin->create(
				"#setting section .unblock",
				"str_unblock_select_fake_bin_title",
				Localization::Str{ "str_unblock_select_fake_bin_description" }
			);

			auto& fake_bin_list = _unblock->getFakeBinList();
			for (u32 i = 0; i < fake_bin_list.size(); i++)
			{
				auto& fake_bin = fake_bin_list[i];
				_unblock_select_fake_bin->createOption(i, fake_bin.key.c_str());
			}

			set_default_select(_unblock_select_fake_bin, "fake_bin");

			_unblock_select_fake_bin->addEventChange(
				[=](JSArgs args)
				{
					set_new_value(_unblock_select_config, "fake_bin", "config", args);
					return false;
				}
			);
		}
	};

	auto createManual = [=]
	{
		_unblock_filtering_top_level_domains->remove();

		if (_unblock_enable->getState())
		{
			_unblock_filtering_top_level_domains->create(
				"#setting section .unblock",
				"str_unblock_filtering_top_level_domains_title",
				Localization::Str{ "str_unblock_filtering_top_level_domains_description" }
			);

			auto result = _file_user_setting->parameterSection<bool>("UNBLOCK", "filtering_top_level_domains");
			_unblock_filtering_top_level_domains->setState(result ? result.value() : false);

			_unblock->changeFilteringTopLevelDomains(_unblock_filtering_top_level_domains->getState());

			_unblock_filtering_top_level_domains->addEventClick(
				[=](JSArgs args)
				{
					_file_user_setting
						->writeSectionParameter("UNBLOCK", "filtering_top_level_domains", static_cast<String>(args[0].ToString()).utf8().data());
					_unblock->changeFilteringTopLevelDomains(args[0].ToBoolean());
					return false;
				}
			);
		}

		_unblock_manual->remove();
		if (_unblock_enable->getState())
		{
			_unblock_manual->create("#setting section .unblock", "str_manual_title", Localization::Str{ "str_manual_description" });

			auto result = _file_user_setting->parameterSection<bool>("UNBLOCK", "manual");
			_unblock_manual->setState(result ? result.value() : false);
			createSelect();

			_unblock_manual->addEventClick(
				[=](JSArgs args)
				{
					if (_unblock->activeService())
						_start_service->setTitle("str_b_restart_service");
					else
						_start_service->setTitle("str_b_restart_service");

					_file_user_setting->writeSectionParameter("UNBLOCK", "manual", static_cast<String>(args[0].ToString()).utf8().data());
					createSelect();
					return false;
				}
			);
		}
	};

	// Enable unblock (winws.exe) service
	{
		_unblock_enable->create("#setting section .unblock", "str_unblock_enable_title", Localization::Str{ "str_unblock_enable_description" });

		auto result = _file_user_setting->parameterSection<bool>("UNBLOCK", "enable");
		_unblock_enable->setState(result ? result.value() : true);

		createManual();

		_unblock_enable->addEventClick(
			[=](JSArgs args)
			{
				_file_user_setting->writeSectionParameter("UNBLOCK", "enable", static_cast<String>(args[0].ToString()).utf8().data());
				createManual();
				_startService();
				_testing();
				return false;
			}
		);
	}

	// Startup testing domains Common
	{
		_testing_domains_startup->create(
			"#setting section .common",
			"str_checkbox_testing_startup_title",
			Localization::Str{ "str_checkbox_testing_startup_description" }
		);

		auto result = _file_user_setting->parameterSection<bool>("TESTING", "startup");
		_testing_domains_startup->setState(result ? result.value() : true);

		_testing_domains_startup->addEventClick(
			[this](JSArgs args)
			{
				_file_user_setting->writeSectionParameter("TESTING", "startup", static_cast<String>(args[0].ToString()).utf8().data());
				return false;
			}
		);
	}

	// Accurate testing Common
	{
		_accurate_testing->create(
			"#setting section .common",
			"str_checkbox_accurate_testing_title",
			Localization::Str{ "str_checkbox_accurate_testing_description" }
		);

		auto result = _file_user_setting->parameterSection<bool>("TESTING", "accurate");
		_accurate_testing->setState(result ? result.value() : false);

		_unblock->accurateTesting(_accurate_testing->getState());

		_accurate_testing->addEventClick(
			[this](JSArgs args)
			{
				_file_user_setting->writeSectionParameter("TESTING", "accurate", static_cast<String>(args[0].ToString()).utf8().data());
				_unblock->accurateTesting(args[0].ToBoolean());
				return false;
			}
		);
	}

	// Proxy Service
	{
		_proxy_enable
			->create("#setting section .proxy", "str_checkbox_proxy_enable_title", Localization::Str{ "str_checkbox_proxy_enable_description" });

		auto result = _file_user_setting->parameterSection<bool>("PROXY", "enable");
		_proxy_enable->setState(result ? result.value() : false);

		auto createSelectProxy = [this]
		{
			_proxy_select_config->remove();

			if (_proxy_manual->getState())
			{
				_proxy_select_config
					->create("#setting section .proxy", "str_select_config_title", Localization::Str{ "str_select_config_description" });

				auto& strategies_list = _unblock->getStrategiesList<ProxyStrategiesDPI>();
				for (u32 i = 0; i < strategies_list.size(); i++)
				{
					auto& file_name = strategies_list[i];
					_proxy_select_config->createOption(i, file_name.c_str());
				}

				if (auto config = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy"))
					_proxy_select_config->setSelectedOptionValue(config.value().c_str());

				_proxy_select_config->addEventChange(
					[this](JSArgs args)
					{
						_file_user_setting
							->writeSectionParameter("REMEMBER_CONFIGURATION", "config_proxy", static_cast<String>(args[1].ToString()).utf8().data());
						return false;
					}
				);
			}
		};

		auto createManualProxy = [=]
		{
			_proxy_manual->remove();
			if (_proxy_enable->getState())
			{
				_proxy_manual->create("#setting section .proxy", "str_manual_title", Localization::Str{ "str_manual_description" });

				auto result = _file_user_setting->parameterSection<bool>("PROXY", "manual");
				_proxy_manual->setState(result ? result.value() : false);
				createSelectProxy();

				_proxy_manual->addEventClick(
					[=](JSArgs args)
					{
						_file_user_setting->writeSectionParameter("PROXY", "manual", static_cast<String>(args[0].ToString()).utf8().data());
						createSelectProxy();
						return false;
					}
				);
			}
		};

		auto create_setting_proxy = [=]
		{
			_proxy_port
				->create("#setting section .proxy", Input::Types::number, "1080", "str_input_proxy_port_title", "str_input_proxy_port_description");

			auto port = _file_user_setting->parameterSection<u32>("PROXY", "port");
			if (port)
				_proxy_port->setValue(port.value());

			_proxy_port->addEventSubmit(
				[this](JSArgs args)
				{
					auto port = static_cast<String>(args[0].ToString());
					_file_user_setting->writeSectionParameter("PROXY", "port", port.utf8().data());
					return false;
				}
			);

			_proxy_ip->create("#setting section .proxy", Input::Types::ip, "127.0.0.1", "str_input_proxy_ip_title", "str_input_proxy_ip_description");
			auto ip = _file_user_setting->parameterSection<pcstr>("PROXY", "ip");
			if (ip)
				_proxy_ip->setValue(ip.value());

			_proxy_port->addEventSubmit(
				[this](JSArgs args)
				{
					auto ip = static_cast<String>(args[0].ToString());
					_file_user_setting->writeSectionParameter("PROXY", "ip", ip.utf8().data());
					return false;
				}
			);
		};

		_proxy_enable->addEventClick(
			[=](JSArgs args)
			{
				createManualProxy();

				auto input_checked = static_cast<String>(args[0].ToString());
				_file_user_setting->writeSectionParameter("PROXY", "enable", input_checked.utf8().data());

				if (args[0].ToBoolean())
					create_setting_proxy();
				else
				{
					_proxy_port->remove();
					_proxy_ip->remove();
				}

				_startService();
				_testing();

				return false;
			}
		);

		if (_proxy_enable->getState())
		{
			createManualProxy();
			create_setting_proxy();
		}
	}
}
