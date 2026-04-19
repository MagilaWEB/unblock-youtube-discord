#include "ui.h"
#include "ui_base.h"

Ui::Ui(UiBase* ui_base) : _ui_base(ui_base)
{
	_file_service_list = std::make_shared<File>();
	_file_service_list->open({ Core::get().configsPath() / "service_setting" }, ".config", true);

	_file_service_list->forLineParametersSection(
		"LIST",
		[this](std::string key, std::string /*value*/)
		{
			_unblock_list_enable_services.emplace(key, std::make_shared<CheckBox>(std::string{ "_unblock_to_list_" } + key));
			return false;
		}
	);

	_unblock.serviceConfigFile(_file_service_list);
}

void Ui::initialize()
{
	if (_init)
		return;

	auto js_global					 = JSGlobalObject();
	_updateCountStartStopButtonToCss = js_global["updateCountStartStopButtonToCss"];

	_window_wait_response_from_server->create(Localization::Str{ "str_please_wait" }, "str_window_wait_response_from_server_description");
	_window_wait_response_from_server->setType(SecondaryWindow::Type::Info);

	_checkWhitelist();

	_initShowInfoSetting();

	_updateApp();

	_checkConflictService();

	_settingInit();

	// HOME
	_startInit();
	_stopInit();
	_testingInit();

	_footerElements();

	_removeApp();

	_init = true;
}

void Ui::jsUpdate()
{
	_settingDnsHostsUpdateInfoWindow();
	_updateAppProgressWindowInfo();
}

void Ui::_footerElements()
{
	_link_to_github->create("footer", "str_link_to_github");
	_link_to_github->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask([] { system("start https://github.com/MagilaWEB/unblock-youtube-discord"); });
			return false;
		}
	);

	_link_to_telegram->create("footer", "str_link_to_telegram");
	_link_to_telegram->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask([] { system("start https://t.me/+OqRXcWFw4kpmMTcy"); });
			return false;
		}
	);
}

void Ui::_tcpGlobalChange(bool state)
{
	if (state)
	{
		auto tcp_set_global = _ui_base->userSetting()->parameterSection<bool>("SUSTEM", "enable_tcp_global");
		if ((!tcp_set_global) || (!tcp_set_global.value()))
		{
			system("netsh interface tcp set global timestamps=enabled");
			_ui_base->userSetting()->writeSectionParameter("SUSTEM", "enable_tcp_global", "true");
		}
	}
	else
	{
		system("netsh interface tcp set global timestamps=disabled");
		_ui_base->userSetting()->writeSectionParameter("SUSTEM", "enable_tcp_global", "false");
	}
}

void Ui::_checkConflictService()
{
	_window_warning_conflict_service->create(Localization::Str{ "str_warning" }, "");
	_window_warning_conflict_service->setType(SecondaryWindow::Type::YesNo);

	auto description = Localization::Str{ "str_window_warning_conflict_service" }();

	auto& conflict_service = _unblock.getConflictingServices();
	if (!conflict_service.empty())
	{
		std::string names_services;
		for (auto& service : conflict_service)
			names_services.append(service.getName()).append(",");
		names_services.pop_back();

		_window_warning_conflict_service->setDescription(utils::format(description, names_services));

		_window_warning_conflict_service->show();

		_window_warning_conflict_service->addEventYesNo(
			[this, &conflict_service](JSArgs args)
			{
				if (JSToCPP<bool>(args[0]))
					for (auto& service : conflict_service)
						service.remove();

				conflict_service.clear();

				_window_warning_conflict_service->hide();

				return true;
			}
		);
	}
}

void Ui::_checkWhitelist()
{
	if (!_window_wait_test_whitelist->isCreate())
	{
		_window_wait_test_whitelist->create(Localization::Str{ "str_please_wait" }, "str_window_wait_test_whitelist_description");
		_window_wait_test_whitelist->setType(SecondaryWindow::Type::Wait);
	}

	_window_wait_test_whitelist->show();

	if (!_window_warning_whitelist->isCreate())
	{
		_window_warning_whitelist->create(Localization::Str{ "str_warning" }, "str_window_whitelist_description");
		_window_warning_whitelist->setType(SecondaryWindow::Type::OK);
	}

	if (!_window_warning_no_internet->isCreate())
	{
		_window_warning_no_internet->create(Localization::Str{ "str_warning" }, "str_window_warning_no_internet_description");
		_window_warning_no_internet->setType(SecondaryWindow::Type::OK);
	}

	Core::get().addTaskParallel(
		[this]
		{
			const bool state_internet = _unblock.testUrl("https://yandex.ru") || _unblock.testUrl("https://vk.com");
			if (state_internet)
			{
				const bool state_block =
					_unblock.testUrl("https://google.com") || _unblock.testUrl("https://2ip.ru") || _unblock.testUrl("https://github.com");

				_window_wait_test_whitelist->hide();

				if (!state_block)
				{
					_window_warning_whitelist->show();
					_window_warning_whitelist->addEventOk(
						[this](JSArgs)
						{
							_window_warning_whitelist->hide();
							return true;
						}
					);
				}
			}
			else
			{
				_window_warning_no_internet->show();
				_window_warning_no_internet->addEventOk(
					[this](JSArgs)
					{
						_window_warning_no_internet->hide();
						return true;
					}
				);
			}
		}
	);
}
