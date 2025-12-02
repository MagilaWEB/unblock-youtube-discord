#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

void Ui::_setting()
{
	// Show window console.
#ifndef DEBUG
	{
		_show_console
			->create("#setting section .common", "str_checkbox_show_console_title", Localization::Str{ "str_checkbox_show_console_description" });

		auto result = _ui_base->userSetting()->parameterSection<bool>("SUSTEM", "show_console");
		_show_console->setState(result ? result.value() : false);

		_show_console->addEventClick(
			[this](JSArgs args)
			{
				_ui_base->userSetting()->writeSectionParameter("SUSTEM", "show_console", static_cast<String>(args[0].ToString()).utf8().data());
				return false;
			}
		);
	}
#endif

	auto createSelect = [this]
	{
		_unblock_select_config->remove();
		_unblock_select_fake_bin->remove();

		if (_unblock_enable->getState() && _unblock_manual->getState())
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
				if (auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", name))
					select->setSelectedOptionValue(config.value().c_str());
				else
					_ui_base->userSetting()->writeSectionParameter(
						"REMEMBER_CONFIGURATION",
						name,
						static_cast<String>(select->getSelectedOptionValue().ToString()).utf8().data()
					);
			};

			auto set_new_value = [this](Ptr<SelectList>& select, pcstr set_val, pcstr check_val, JSArgs args)
			{
				_ui_base->userSetting()
					->writeSectionParameter("REMEMBER_CONFIGURATION", set_val, static_cast<String>(args[1].ToString()).utf8().data());

				_ui_base->userSetting()->writeSectionParameter(
					"REMEMBER_CONFIGURATION",
					check_val,
					static_cast<String>(select->getSelectedOptionValue().ToString()).utf8().data()
				);
			};

			set_default_select(_unblock_select_config, "config");

			_buttonUpdate();

			_unblock_select_config->addEventChange(
				[this, set_new_value](JSArgs args)
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
				[this, set_new_value](JSArgs args)
				{
					set_new_value(_unblock_select_config, "fake_bin", "config", args);
					return false;
				}
			);
		}
	};

	auto createElements = [this, createSelect]
	{
		// Unblock enable services
		_unblock->clearOptionalStrategies();

		for (auto& [name, check_box] : _unblock_list_enable_services)
		{
			check_box->remove();
			if (_unblock_enable->getState())
			{
				check_box->create(
					"#setting section .unblock",
					std::string{ "str_unblock_enable_" + name + "_title" }.c_str(),
					Localization::Str{ std::string{ "str_unblock_enable_" + name + "_description" }.c_str() }
				);

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

				check_box->addEventClick(
					[this, setting_name, name](JSArgs args)
					{
						_ui_base->userSetting()
							->writeSectionParameter("UNBLOCK", setting_name.c_str(), static_cast<String>(args[0].ToString()).utf8().data());

						if (args[0].ToBoolean())
							_unblock->addOptionalStrategies(name);
						else
							_unblock->removeOptionalStrategies(name);
						return false;
					}
				);
			}
		}

		_unblock_filtering_top_level_domains->remove();

		if (_unblock_enable->getState())
		{
			_unblock_filtering_top_level_domains->create(
				"#setting section .unblock",
				"str_unblock_filtering_top_level_domains_title",
				Localization::Str{ "str_unblock_filtering_top_level_domains_description" }
			);

			auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", "filtering_top_level_domains");
			_unblock_filtering_top_level_domains->setState(result ? result.value() : false);

			_unblock->changeFilteringTopLevelDomains(_unblock_filtering_top_level_domains->getState());

			_unblock_filtering_top_level_domains->addEventClick(
				[this](JSArgs args)
				{
					_ui_base->userSetting()
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

			auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", "manual");
			_unblock_manual->setState(result ? result.value() : false);

			_unblock_manual->addEventClick(
				[this, createSelect](JSArgs args)
				{
					_ui_base->userSetting()->writeSectionParameter("UNBLOCK", "manual", static_cast<String>(args[0].ToString()).utf8().data());
					createSelect();
					return false;
				}
			);
		}

		createSelect();
	};

	// Enable unblock (winws.exe) service
	{
		_unblock_enable->create("#setting section .unblock", "str_unblock_enable_title", Localization::Str{ "str_unblock_enable_description" });

		auto result = _ui_base->userSetting()->parameterSection<bool>("UNBLOCK", "enable");
		_unblock_enable->setState(result ? result.value() : true);

		createElements();

		_unblock_enable->addEventClick(
			[this, createElements](JSArgs args)
			{
				_ui_base->userSetting()->writeSectionParameter("UNBLOCK", "enable", static_cast<String>(args[0].ToString()).utf8().data());

				createElements();
				_buttonUpdate();
				_testingUpdate();
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

		auto result = _ui_base->userSetting()->parameterSection<bool>("TESTING", "startup");
		_testing_domains_startup->setState(result ? result.value() : true);

		_testing_domains_startup->addEventClick(
			[this](JSArgs args)
			{
				_ui_base->userSetting()->writeSectionParameter("TESTING", "startup", static_cast<String>(args[0].ToString()).utf8().data());
				return false;
			}
		);
	}

	// MAX time wait testing domains Common
	auto create_time_wait_testing = [this]
	{
		_max_time_wait_testing->remove();
		_max_time_wait_accurate_testing->remove();

		if (_accurate_testing->getState())
		{
			_max_time_wait_accurate_testing->create(
				"#setting section .common",
				Input::Types::number,
				10,
				"str_input_max_wait_accurate_testing_title",
				"str_input_max_wait_accurate_testing_description"
			);

			auto result = _ui_base->userSetting()->parameterSection<u32>("TESTING", "max_time_wait_accurate_testing");
			if (result)
				_max_time_wait_accurate_testing->setValue(result.value());

			_unblock->maxWaitAccurateTesting(static_cast<u32>(_max_time_wait_accurate_testing->getValue().ToInteger()));

			_max_time_wait_accurate_testing->addEventSubmit(
				[this](JSArgs args)
				{
					_ui_base->userSetting()
						->writeSectionParameter("TESTING", "max_time_wait_accurate_testing", static_cast<String>(args[0].ToString()).utf8().data());
					_unblock->maxWaitAccurateTesting(static_cast<u32>(args[0].ToInteger()));
					return false;
				}
			);
		}
		else
		{
			_max_time_wait_testing->create(
				"#setting section .common",
				Input::Types::number,
				5,
				"str_input_max_wait_testing_title",
				"str_input_max_wait_testing_description"
			);

			auto result = _ui_base->userSetting()->parameterSection<u32>("TESTING", "max_time_wait_testing");
			if (result)
				_max_time_wait_testing->setValue(result.value());

			_unblock->maxWaitTesting(static_cast<u32>(_max_time_wait_testing->getValue().ToInteger()));

			_max_time_wait_testing->addEventSubmit(
				[this](JSArgs args)
				{
					_ui_base->userSetting()
						->writeSectionParameter("TESTING", "max_time_wait_testing", static_cast<String>(args[0].ToString()).utf8().data());
					_unblock->maxWaitTesting(static_cast<u32>(args[0].ToInteger()));
					return false;
				}
			);
		}
	};

	// Accurate testing Common
	{
		_accurate_testing->create(
			"#setting section .common",
			"str_checkbox_accurate_testing_title",
			Localization::Str{ "str_checkbox_accurate_testing_description" }
		);

		auto result = _ui_base->userSetting()->parameterSection<bool>("TESTING", "accurate");
		_accurate_testing->setState(result ? result.value() : false);

		_unblock->accurateTesting(_accurate_testing->getState());

		create_time_wait_testing();

		_accurate_testing->addEventClick(
			[this, create_time_wait_testing](JSArgs args)
			{
				_ui_base->userSetting()->writeSectionParameter("TESTING", "accurate", static_cast<String>(args[0].ToString()).utf8().data());
				_unblock->accurateTesting(args[0].ToBoolean());
				create_time_wait_testing();
				return false;
			}
		);
	}

	// Proxy Service
	{
		_proxy_enable->create("#setting section .proxy", "str_checkbox_proxy_enable_title", Localization::Str{ "str_checkbox_proxy_enable_description" });

		auto result = _ui_base->userSetting()->parameterSection<bool>("PROXY", "enable");
		_proxy_enable->setState(result ? result.value() : false);

		auto createSelectProxy = [this]
		{
			_proxy_select_config->remove();

			if (_proxy_enable->getState() && _proxy_manual->getState())
			{
				_proxy_select_config
					->create("#setting section .proxy", "str_select_config_title", Localization::Str{ "str_select_config_description" });

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
						static_cast<String>(_proxy_select_config->getSelectedOptionValue().ToString()).utf8().data()
					);

				_buttonUpdate();

				_proxy_select_config->addEventChange(
					[this](JSArgs args)
					{
						_ui_base->userSetting()
							->writeSectionParameter("REMEMBER_CONFIGURATION", "config_proxy", static_cast<String>(args[1].ToString()).utf8().data());
						return false;
					}
				);
			}
		};

		auto createManualProxy = [this, createSelectProxy]
		{
			_proxy_manual->remove();
			if (_proxy_enable->getState())
			{
				_proxy_manual->create("#setting section .proxy", "str_manual_title", Localization::Str{ "str_manual_description" });

				auto result_manual = _ui_base->userSetting()->parameterSection<bool>("PROXY", "manual");
				_proxy_manual->setState(result_manual ? result_manual.value() : false);
				_proxy_manual->addEventClick(
					[this, createSelectProxy](JSArgs args)
					{
						_ui_base->userSetting()->writeSectionParameter("PROXY", "manual", static_cast<String>(args[0].ToString()).utf8().data());
						createSelectProxy();
						return false;
					}
				);
			}

			createSelectProxy();
		};

		auto create_setting_proxy = [this]
		{
			_proxy_port
				->create("#setting section .proxy", Input::Types::number, "1080", "str_input_proxy_port_title", "str_input_proxy_port_description");

			auto port = _ui_base->userSetting()->parameterSection<u32>("PROXY", "port");
			if (port)
				_proxy_port->setValue(port.value());

			_proxy_port->addEventSubmit(
				[this](JSArgs args)
				{
					auto arg_port = static_cast<String>(args[0].ToString());
					_ui_base->userSetting()->writeSectionParameter("PROXY", "port", arg_port.utf8().data());
					return false;
				}
			);

			_proxy_ip->create("#setting section .proxy", Input::Types::ip, "127.0.0.1", "str_input_proxy_ip_title", "str_input_proxy_ip_description");
			auto ip = _ui_base->userSetting()->parameterSection<pcstr>("PROXY", "ip");
			if (ip)
				_proxy_ip->setValue(ip.value());

			_proxy_port->addEventSubmit(
				[this](JSArgs args)
				{
					auto arg_ip = static_cast<String>(args[0].ToString());
					_ui_base->userSetting()->writeSectionParameter("PROXY", "ip", arg_ip.utf8().data());
					return false;
				}
			);
		};

		_proxy_enable->addEventClick(
			[this, createManualProxy, create_setting_proxy](JSArgs args)
			{
				createManualProxy();

				auto input_checked = static_cast<String>(args[0].ToString());
				_ui_base->userSetting()->writeSectionParameter("PROXY", "enable", input_checked.utf8().data());

				if (args[0].ToBoolean())
					create_setting_proxy();
				else
				{
					_proxy_port->remove();
					_proxy_ip->remove();
				}

				_buttonUpdate();
				_testingUpdate();

				return false;
			}
		);

		if (_proxy_enable->getState())
		{
			createManualProxy();
			create_setting_proxy();
		}
	}

	// Tor Proxy
	/*{
		_tor_proxy_enable->create(
			"#setting section .tor_proxy",
			"str_checkbox_tor_proxy_enable_title",
			Localization::Str{ "str_checkbox_tor_proxy_enable_description" }
		);

		auto result = _ui_base->userSetting()->parameterSection<bool>("TOR_PROXY", "enable");
		_tor_proxy_enable->setState(result ? result.value() : false);

		_tor_proxy_enable->addEventClick(
			[this](JSArgs args)
			{
				auto input_checked = static_cast<String>(args[0].ToString());
				_ui_base->userSetting()->writeSectionParameter("TOR_PROXY", "enable", input_checked.utf8().data());
				_startTorProxy();
				return false;
			}
		);
	}*/
}
