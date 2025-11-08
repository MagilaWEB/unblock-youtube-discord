#include "ui.h"

#include "../unblock/unblock.h"

static std::atomic_bool automatically_strategy_cancel{ false };
static std::atomic_bool proxy_click_state{ false };

void Ui::_startService()
{
	_start_service->remove();
	_start_proxy_service->remove();

	if (_proxy_enable->getState())
	{
		_start_proxy_service->create("#home section .button_start_stop", "str_b_start_service_proxy", true);

		_start_proxy_service->addEventClick(
			[this](JSArgs)
			{
				proxy_click_state = true;

				auto config_proxy = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy");
				if (config_proxy)
					if (_proxy_manual->getState())
					{
						Core::addTask(
							[=]
							{
								_window_wait_start_service->show();
								_unblock->changeProxyStrategy(config_proxy.value().c_str());
								_unblock->startService(true);
								_updateTitleButton(true);
								_window_wait_start_service->hide();
							}
						);
					}
					else
						_window_config_found->show();

				else
					_window_config_not_found->show();

				return false;
			}
		);
	}

	if (_unblock_enable->getState())
	{
		_start_service->create("#home section .button_start_stop", "str_b_start_service", true);

		_start_service->addEventClick(
			[this](JSArgs)
			{
				auto config = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config");
				if (config)
				{
					auto fake_bin = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "fake_bin");

					if (fake_bin && _unblock_manual->getState())
					{
						Core::addTask(
							[=]
							{
								_window_wait_start_service->show();
								_unblock->changeStrategy(config.value().c_str(), fake_bin.value().c_str());
								_unblock->startService();
								_updateTitleButton();
								_window_wait_start_service->hide();
							}
						);
						return false;
					}

					if (!fake_bin)
					{
						Debug::warning("The fake_bin parameter is not specified in the REMEMBER_CONFIGURATION. The fake_bin parameter is required "
									   "for operation.");
						_window_config_not_found->show();
						return false;
					}
					_window_config_found->show();
				}
				else
					_window_config_not_found->show();

				return false;
			}
		);
	}

	_updateTitleButton();
	_updateTitleButton(true);
}

void Ui::_startServiceWindow()
{
	_window_config_not_found->create(Localization::Str{ "str_window_config_not_found_title" }, "str_window_config_not_found_description");
	_window_config_not_found->setType(SecondaryWindow::Type::OK);

	_window_config_found->create(Localization::Str{ "str_window_config_found_title" }, "str_window_config_found_description");
	_window_config_found->setType(SecondaryWindow::Type::YesNo);

	_window_continue_select_strategy->create(Localization::Str{ "str_window_continue_select_strategy_title" }, "");
	_window_continue_select_strategy->setType(SecondaryWindow::Type::YesNo);

	_window_auto_start_wait->create(Localization::Str{ "str_please_wait" }, "str_window_auto_start_wait_description");
	_window_auto_start_wait->setType(SecondaryWindow::Type::Wait);

	_window_wait_start_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_start_wait_description");
	_window_wait_start_service->setType(SecondaryWindow::Type::Wait);

	_window_configuration_selection_error->create(Localization::Str{ "str_error" }, "str_window_configuration_selection_error");
	_window_configuration_selection_error->setType(SecondaryWindow::Type::OK);

	_window_auto_start_wait->addEventCancel(
		[this](JSArgs)
		{
			if (proxy_click_state)
				_unblock->testingDomainCancel<ProxyStrategiesDPI>();
			else
				_unblock->testingDomainCancel<StrategiesDPI>();

			automatically_strategy_cancel = true;
			return false;
		}
	);


	auto auto_config = [this]
	{
		Debug::ok("auto Start");

		_window_auto_start_wait->setDescription("str_window_auto_start_wait_description");
		_window_auto_start_wait->show();

		auto errorAutomalicallyStrategy = [this] {

			const bool state =
				proxy_click_state ? _unblock->automaticallyStrategy<ProxyStrategiesDPI>() : _unblock->automaticallyStrategy<StrategiesDPI>();
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

		while (errorAutomalicallyStrategy())
		{
			if (automatically_strategy_cancel)
			{
				_unblock->stopService(proxy_click_state);
				break;
			}

			_unblock->startService(proxy_click_state);

			std::string _strategy_name =
				proxy_click_state ? _unblock->getNameStrategies<ProxyStrategiesDPI>() : _unblock->getNameStrategies<StrategiesDPI>();

			Localization::Str  desc_base{ "str_window_auto_start_wait_description" };
			Localization::Str  desc_base2{ "str_window_auto_start_wait_name_strategy_description" };
			static std::string text_desc_base;
			text_desc_base = utils::format(desc_base2(), _strategy_name.c_str());
			text_desc_base.insert(0, "\n");
			text_desc_base.insert(0, desc_base());

			_window_auto_start_wait->setDescription(text_desc_base.c_str());

			proxy_click_state ? _unblock->testingDomain<ProxyStrategiesDPI>() : _unblock->testingDomain<StrategiesDPI>();

			if (!automatically_strategy_cancel && _unblock->validDomain(proxy_click_state))
			{
				static std::string text_desc;

				if (proxy_click_state)
				{
					_file_user_setting->writeSectionParameter("REMEMBER_CONFIGURATION", "config_proxy", _strategy_name.c_str());
					Localization::Str  desc{ "str_window_continue_select_strategy_proxy_description" };				
					text_desc = utils::format(desc(), _strategy_name.c_str());
				}
				else
				{
					std::string _fake_bin = _unblock->getNameFakeBin();
					_file_user_setting->writeSectionParameter("REMEMBER_CONFIGURATION", "config", _strategy_name.c_str());
					_file_user_setting->writeSectionParameter("REMEMBER_CONFIGURATION", "fake_bin", _fake_bin.c_str());

					Localization::Str  desc{ "str_window_continue_select_strategy_description" };
					text_desc = utils::format(desc(), _strategy_name.c_str(), _fake_bin.c_str());
				}

				_window_continue_select_strategy->setDescription(text_desc.c_str());
				_window_continue_select_strategy->show();
				break;
			}
		}

		_updateTitleButton(proxy_click_state);

		automatically_strategy_cancel = false;

		_window_auto_start_wait->hide();
	};

	_window_config_not_found->addEventOk(
		[=](JSArgs)
		{
			Core::addTask(auto_config);
			_window_config_not_found->hide();
			return false;
		}
	);

	_window_config_found->addEventYesNo(
		[=](JSArgs args)
		{
			if (args[0].ToBoolean())
				Core::addTask(auto_config);
			else
			{
				if (proxy_click_state)
				{
					if (auto config_proxy = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy"))
						_unblock->changeProxyStrategy(config_proxy.value().c_str());
				}
				else
				{
					auto fake_bin = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "fake_bin");
					if (auto config = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
						_unblock->changeStrategy(config.value().c_str(), fake_bin.value().c_str());
				}

				Core::addTask(
					[this]
					{
						_window_wait_start_service->show();
						_unblock->startService(proxy_click_state);
						_window_wait_start_service->hide();
						proxy_click_state = false;
					}
				);
			}

			_window_config_found->hide();
			return false;
		}
	);

	_window_continue_select_strategy->addEventYesNo(
		[=](JSArgs args)
		{
			if (args[0].ToBoolean())
				Core::addTask(auto_config);
			else
				proxy_click_state = false;

			_window_continue_select_strategy->hide();
			return false;
		}
	);
}

void Ui::_updateTitleButton(bool proxy)
{
	if (proxy)
	{
		if (auto config = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy"))
			if (_unblock->activeService(true))
				_start_proxy_service->setTitle("str_b_restart_service_proxy");
			else
				_start_proxy_service->setTitle("str_b_start_service_proxy");
		else
			_start_proxy_service->setTitle("str_b_start_find_config_proxy");

		return;
	}

	if (auto config = _file_user_setting->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config"))
		if (_unblock->activeService())
			_start_service->setTitle("str_b_restart_service");
		else
			_start_service->setTitle("str_b_start_service");
	else
		_start_service->setTitle("str_b_start_find_config");
}
