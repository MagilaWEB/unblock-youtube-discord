#include "ui.h"
#include "ui_base.h"

Ui::Ui(UiBase* ui_base) : _ui_base(ui_base)
{
	_file_service_list->open({ Core::get().configsPath() / "service_setting" }, ".config", true);

	_file_service_list->forLineParametersSection(
		"LIST",
		[this](std::string key, std::string /*value*/)
		{
			_unblock_list_enable_services.emplace(key, std::make_shared<CheckBox>(std::string{ "_unblock_to_list_" } + key));
			return false;
		}
	);
}

void Ui::initialize()
{
	if (_init)
		return;

	auto js_global					 = JSGlobalObject();
	_updateCountStartStopButtonToCss = js_global["updateCountStartStopButtonToCss"];

	_window_wait_update_unblock->create(Localization::Str{ "str_please_wait" }, "str_window_wait_update_unblock");
	_window_wait_update_unblock->setType(SecondaryWindow::Type::Wait);

	_window_wait_check_update_unblock->create(Localization::Str{ "str_please_wait" }, "str_window_check_update_unblock");
	_window_wait_check_update_unblock->setType(SecondaryWindow::Type::Wait);

	_window_update_unblock->create(Localization::Str{ "str_warning" }, "");
	_window_update_unblock->hide();

	_window_update_unblock->setType(SecondaryWindow::Type::YesNo);
	_window_update_unblock->addEventYesNo(
		[this](JSArgs args)
		{
			if (JSToCPP<bool>(args[0]))
			{
				Core::get().addTask(
					[this]
					{
						_window_update_unblock->hide();
						_window_wait_update_unblock->show();
						_unblock->appUpdate();
						_window_wait_update_unblock->hide();
						_ui_base->OnClose(nullptr);
					}
				);

				return false;
			}

			_window_update_unblock->hide();
			return false;
		}
	);

	_enable_check_update_startup->create(
		"#setting section .common",
		"str_checkbox_check_update_app_startup_title",
		Localization::Str{ "str_checkbox_check_update_app_startup_description" }
	);

	auto result = _ui_base->userSetting()->parameterSection<bool>("SUSTEM", "check_update_app_startup");
	_enable_check_update_startup->setState(result ? result.value() : true);

	if (_enable_check_update_startup->getState())
		_checkAppUpdate();

	_start_check_update_app->create("#setting section .common", "str_bottom_check_update_app_startup_title");

	_start_check_update_app->addEventClick(
		[this](JSArgs)
		{
			_checkAppUpdate(true);
			return false;
		}
	);

	_checkConflictService();

	_settingInit();

	// HOME
	_startInit();
	_stopInit();
	_testingInit();

	_footerElements();

	_init = true;
}

void Ui::jsUpdate()
{
	_settingDnsHostsUpdateInfoWindow();
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

void Ui::_checkAppUpdate(bool window_show)
{
	Core::get().addTask(
		[this, window_show]
		{
			if (window_show)
				_window_wait_check_update_unblock->show();

			if (auto new_version = _unblock->checkUpdate())
			{
				if (window_show)
					_window_wait_check_update_unblock->hide();

				static pcstr desc = Localization::Str{ "str_window_update_unblock" }();
				_window_update_unblock->setDescription(utils::format(desc, new_version.value().c_str()).c_str());
				_window_update_unblock->show();
				return;
			}

			if (window_show)
				_window_wait_check_update_unblock->hide();
		}
	);
}

void Ui::_checkConflictService()
{
	_window_warning_conflict_service->create(Localization::Str{ "str_warning" }, "");
	_window_warning_conflict_service->setType(SecondaryWindow::Type::YesNo);

	static Localization::Str lang_disc{ "str_window_warning_conflict_service" };

	std::string description = lang_disc();

	auto& conflict_service = _unblock->getConflictingServices();
	if (!conflict_service.empty())
	{
#if __clang__
		[[clang::no_destroy]]
#endif
		static std::string names_services;

		for (auto& service : conflict_service)
			names_services.append(service.getName()).append(",");
		names_services.pop_back();

		description = utils::format(description.c_str(), names_services.c_str());
		_window_warning_conflict_service->setDescription(description.c_str());

		_window_warning_conflict_service->show();

		_window_warning_conflict_service->addEventYesNo(
			[this, &conflict_service](JSArgs args)
			{
				if (args[0].ToBoolean())
					for (auto& service : conflict_service)
						service.remove();

				conflict_service.clear();

				_window_warning_conflict_service->hide();

				return true;
			}
		);
	}
}
