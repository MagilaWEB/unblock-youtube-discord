#include "ui.h"
#include "ui_base.h"
#include "utils_ultralight.hpp"

void Ui::_updateApp()
{
	_updateAppWindow();

	_enable_check_update_startup->create(
		"#setting section .common",
		"str_checkbox_check_update_app_startup_title",
		Localization::Str{ "str_checkbox_check_update_app_startup_description" }
	);

	auto result = _ui_base->userSetting()->parameterSection<bool>("SUSTEM", "check_update_app_startup");
	_enable_check_update_startup->setState(result ? result.value() : true);

	_enable_check_update_startup->addEventClick(
		[this](JSArgs args)
		{
			_ui_base->userSetting()->writeSectionParameter("SUSTEM", "check_update_app_startup", JSToCPP(args[0]));
			return false;
		}
	);

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
}

void Ui::_checkAppUpdate(bool window_show)
{
	Core::get().addTask(
		[this, window_show]
		{
			if (window_show)
				_window_wait_check_update_unblock->show();

			if (auto new_version = _unblock.checkUpdate())
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

void Ui::_updateAppWindow()
{
	_window_wait_update_unblock->create(Localization::Str{ "str_please_wait" }, "str_window_wait_update_unblock");
	_window_wait_update_unblock->setType(SecondaryWindow::Type::Info);

	_window_wait_check_update_unblock->create(Localization::Str{ "str_please_wait" }, "str_window_check_update_unblock");
	_window_wait_check_update_unblock->setType(SecondaryWindow::Type::Info);

	_window_error_update_unblock->create(Localization::Str{ "str_error" }, "str_window_error_update_unblock");
	_window_error_update_unblock->setType(SecondaryWindow::Type::OK);

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
						bool state = _unblock.appUpdate();
						_window_wait_update_unblock->hide();

						if (state)
							_ui_base->OnClose(nullptr);
						else
						{
							_window_error_update_unblock->show();
							_window_error_update_unblock->addEventOk(
								[this](JSArgs)
								{
									_window_error_update_unblock->hide();
									return true;
								}
							);
						}
					}
				);

				return false;
			}

			_window_update_unblock->hide();
			return false;
		}
	);
}
