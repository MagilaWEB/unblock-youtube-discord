#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

void Ui::_startInit()
{
	_startProxy();
	_startUnblock();
	_startServiceWindow();
}

void Ui::_startUnblock()
{
	_start_unblock->create("#home section .buttons_start", "str_b_start_unblock", true);

	_start_unblock->addEventClick(
		[this](JSArgs)
		{
			_proxy_click_state = false;
			_clickStartService();
			return false;
		}
	);

	_buttonUpdate();
}

void Ui::_startProxy()
{
	_start_proxy_dpi->create("#home section .buttons_start", "str_b_start_proxy_dpi", true);

	_start_proxy_dpi->addEventClick(
		[this](JSArgs)
		{
			_proxy_click_state = true;
			_clickStartService();
			return false;
		}
	);

	_buttonUpdate();
}

// void Ui::_startTorProxy()
//{
//	if (_tor_proxy_enable->getState())
//	{
//		// Core::get().addTask(
//		//	[this]
//		//	{
//		//		_window_wait_start_service->show();
//		//		_unblock->startTor();
//		//		_updateTitleButton(true);
//		//		_activeService();
//		//		_window_wait_start_service->hide();
//		//	}
//		//);
//	}
// }

void Ui::_startServiceWindow()
{
	_window_config_not_found->create(Localization::Str{ "str_window_config_not_found_title" }, "str_window_config_not_found_description");
	_window_config_not_found->setType(SecondaryWindow::Type::OK);
	_window_config_not_found->addEventOk(
		[this](JSArgs)
		{
			_autoStart();
			_window_config_not_found->hide();
			return false;
		}
	);

	_window_config_found->create(Localization::Str{ "str_window_config_found_title" }, "str_window_config_found_description");
	_window_config_found->setType(SecondaryWindow::Type::YesNo);
	_window_config_found->addEventYesNo(
		[this](JSArgs args)
		{
			if (args[0].ToBoolean())
				_autoStart();
			else
				_startServiceFromConfig();

			_window_config_found->hide();
			return false;
		}
	);

	_window_auto_start_wait->create(Localization::Str{ "str_please_wait" }, "str_window_auto_start_wait_description");
	_window_auto_start_wait->setType(SecondaryWindow::Type::Wait);
	_window_auto_start_wait->addEventCancel(
		[this](JSArgs)
		{
			if (_proxy_click_state)
				_unblock->testingDomainCancel<ProxyStrategiesDPI>();
			else
				_unblock->testingDomainCancel<StrategiesDPI>();

			_automatically_strategy_cancel = true;
			return false;
		}
	);

	_window_continue_select_strategy->create(Localization::Str{ "str_window_continue_select_strategy_title" }, "");
	_window_continue_select_strategy->setType(SecondaryWindow::Type::YesNo);
	_window_continue_select_strategy->addEventYesNo(
		[this](JSArgs args)
		{
			if (args[0].ToBoolean())
				_autoStart();
			else
				_proxy_click_state = false;

			_window_continue_select_strategy->hide();
			return false;
		}
	);

	_window_wait_start_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_start_wait_description");
	_window_wait_start_service->setType(SecondaryWindow::Type::Wait);

	_window_configuration_selection_error->create(Localization::Str{ "str_error" }, "str_window_configuration_selection_error");
	_window_configuration_selection_error->setType(SecondaryWindow::Type::OK);
}

void Ui::_buttonUpdate()
{
	if (_unblock_enable->getState())
	{
		_start_unblock->show();
		_stop_unblock->show();

		if (auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
			if (_unblock->activeService())
				_start_unblock->setTitle("str_b_restart_unblock");
			else
				_start_unblock->setTitle("str_b_start_unblock");
		else
			_start_unblock->setTitle("str_b_start_find_config");
	}
	else
	{
		_start_unblock->hide();
		_stop_unblock->hide();
	}

	if (_proxy_enable->isCreate() && _proxy_enable->getState())
	{
		_start_proxy_dpi->show();
		_stop_proxy_dpi->show();

		if (_ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy"))
			if (_unblock->activeService(true))
				_start_proxy_dpi->setTitle("str_b_restart_proxy_dpi");
			else
				_start_proxy_dpi->setTitle("str_b_start_proxy_dpi");
		else
			_start_proxy_dpi->setTitle("str_b_start_find_config_proxy");
	}
	else
	{
		_start_proxy_dpi->hide();
		_stop_proxy_dpi->hide();
	}

	if (_updateCountStartStopButtonToCss)
		_updateCountStartStopButtonToCss({});

	/*if (_tor_proxy_enable->getState() && _unblock->activeTor())
		_start_proxy_dpi->setTitle("str_b_restart_service_proxy");
	else
		_start_proxy_dpi->setTitle("str_b_start_service_proxy");*/
}

void Ui::_clickStartService()
{
	if (_ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", _proxy_click_state ? "config_proxy" : "config"))
	{
		if (_proxy_click_state ? _proxy_manual->getState() : _unblock_manual->getState())
		{
			_startServiceFromConfig();
			return;
		}

		_window_config_found->show();
		return;
	}

	_window_config_not_found->show();
}

void Ui::_autoStart()
{
	auto errorAutomaticallyStrategy = [this]
	{
		const bool state =
			_proxy_click_state ? _unblock->automaticallyStrategy<ProxyStrategiesDPI>() : _unblock->automaticallyStrategy<StrategiesDPI>();

		if (!state)
		{
			_window_configuration_selection_error->show();
			_window_configuration_selection_error->addEventOk(
				[this](JSArgs)
				{
					_window_configuration_selection_error->hide();
					return true;
				}
			);
		}

		return state;
	};

	Core::get().addTask(
		[this, errorAutomaticallyStrategy]
		{
			InputConsole::textOk(Localization::Str{ "str_beginning_auto_selection" }());

			_window_auto_start_wait->setDescription("str_window_auto_start_wait_description");
			_window_auto_start_wait->show();

			while (errorAutomaticallyStrategy())
			{
				if (_automatically_strategy_cancel)
				{
					_unblock->stopService(_proxy_click_state);
					break;
				}

				_unblock->startService(_proxy_click_state);

				std::string _strategy_name =
					_proxy_click_state ? _unblock->getNameStrategies<ProxyStrategiesDPI>() : _unblock->getNameStrategies<StrategiesDPI>();

				Localization::Str desc_base{ "str_window_auto_start_wait_description" };
				Localization::Str desc_base2{ "str_window_auto_start_wait_name_strategy_description" };
#if __clang__
				[[clang::no_destroy]]
#endif
				static std::string text_desc_base;
				text_desc_base = utils::format(desc_base2(), _strategy_name.c_str());
				text_desc_base.insert(0, "\n");
				text_desc_base.insert(0, desc_base());

				_window_auto_start_wait->setDescription(text_desc_base.c_str());

				_proxy_click_state ? _unblock->testingDomain<ProxyStrategiesDPI>() : _unblock->testingDomain<StrategiesDPI>();

				if (!_automatically_strategy_cancel && _unblock->validDomain(_proxy_click_state))
				{
#if __clang__
					[[clang::no_destroy]]
#endif
					static std::string text_desc;

					if (_proxy_click_state)
					{
						_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", "config_proxy", _strategy_name.c_str());
						Localization::Str desc{ "str_window_continue_select_strategy_proxy_description" };
						text_desc = utils::format(desc(), _strategy_name.c_str());
					}
					else
					{
						std::string _fake_bin = _unblock->getNameFakeBin();
						_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", "config", _strategy_name.c_str());
						_ui_base->userSetting()->writeSectionParameter("REMEMBER_CONFIGURATION", "fake_bin", _fake_bin.c_str());

						Localization::Str desc{ "str_window_continue_select_strategy_description" };
						text_desc = utils::format(desc(), _strategy_name.c_str(), _fake_bin.c_str());
					}

					_window_continue_select_strategy->setDescription(text_desc.c_str());
					_window_continue_select_strategy->show();
					break;
				}
			}

			_buttonUpdate();
			_activeServiceUpdate();

			_automatically_strategy_cancel = false;
			_window_auto_start_wait->hide();
		}
	);
}

void Ui::_startServiceFromConfig()
{
	auto config = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", _proxy_click_state ? "config_proxy" : "config");
	if (config)
	{
		Core::get().addTask(
			[this, config]
			{
				_window_wait_start_service->show();

				if (_proxy_click_state)
					_unblock->changeProxyStrategy(config.value().c_str());
				else if (auto fake_bin = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "fake_bin"))
					_unblock->changeStrategy(config.value().c_str(), fake_bin.value().c_str());
				else
					Debug::error(fake_bin.error().c_str());

				_unblock->startService(_proxy_click_state);
				_buttonUpdate();
				_activeServiceUpdate();
				_proxy_click_state = false;
				_window_wait_start_service->hide();
			}
		);
	}
	else
		Debug::fatal(config.error().c_str());
}
