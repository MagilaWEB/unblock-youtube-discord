#include "ui.h"

#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"

void Ui::_updateApp()
{
	_updateAppWindow();

	_enable_check_update_startup->create(
		"#unblock section .common",
		"str_checkbox_check_update_app_startup_title",
		Localization::Str{ "str_checkbox_check_update_app_startup_description" }
	);

	auto result = userConfig()->parameterSection<bool>("SYSTEM", "check_update_app_startup");
	_enable_check_update_startup->setState(result ? result.value() : true);

	_enable_check_update_startup->addEventClick(
		[self = self](JSArgs args)
		{
			self->userConfig()->writeSectionParameter("SYSTEM", "check_update_app_startup", JSToCPP(args[0]));
			return false;
		}
	);

	if (_enable_check_update_startup->getState())
		_checkAppUpdate();

	_start_check_update_app->create("#unblock section .common", "str_bottom_check_update_app_startup_title");

	_start_check_update_app->addEventClick(
		[self = self](JSArgs)
		{
			self->_checkAppUpdate();
			return false;
		}
	);
}

void Ui::_checkAppUpdate()
{
	// Guard against re-entry: startup check and manual button share this path.
	if (_ui_background_tasks->exists("check_update"))
		return;

	_start_check_update_app->setTitle(Localization::Str{ "str_button_check_update_app_progress_title" });

	_ui_background_tasks->start("check_update", "str_task_check_update_title");

	Core::get().addTask(
		[self = self]
		{
			if (auto new_version = self->_unblock->checkUpdate())
			{
				static auto desc = Localization::Str{ "str_window_update_unblock" }();
				self->_window_update_unblock->setDescription(utils::format(desc, new_version.value()));
				self->_window_update_unblock->show();
				self->_ui_background_tasks->finish("check_update");
				self->_start_check_update_app->setTitle(Localization::Str{ "str_bottom_check_update_app_startup_title" });
				return;
			}

			self->_ui_background_tasks->finish("check_update");
			self->_start_check_update_app->setTitle(Localization::Str{ "str_bottom_check_update_app_startup_title" });
		}
	);
}

void Ui::_updateAppWindow()
{
	_window_wait_update_unblock->create(Localization::Str{ "str_please_wait" }, "str_window_wait_update_unblock");
	_window_wait_update_unblock->setType(SecondaryWindow::Type::Info);

	_window_error_update_unblock->create(Localization::Str{ "str_error" }, "str_window_error_update_unblock");
	_window_error_update_unblock->setType(SecondaryWindow::Type::OK);

	_window_update_unblock->create(Localization::Str{ "str_warning" }, "");
	_window_update_unblock->hide();

	_window_update_unblock->setType(SecondaryWindow::Type::YesNo);
	_window_update_unblock->addEventYesNo(
		[self = self](JSArgs args)
		{
			if (JSToCPP<bool>(args[0]))
			{
				self->console(false);

				Core::get().addTask(
					[self = self]
					{
						self->_ui_unblock->stopAllServices();
						self->_window_update_unblock->hide();
						self->_window_wait_update_unblock->show();
						bool state = self->_unblock->appUpdate();
						self->_window_wait_update_unblock->hide();

						if (state)
							self->OnClose(nullptr);
						else
						{
							self->_window_error_update_unblock->show();
							self->_window_error_update_unblock->addEventOk(
								[self = self](JSArgs)
								{
									self->_window_error_update_unblock->hide();
									return true;
								}
							);
						}
					}
				);

				return false;
			}

			self->_window_update_unblock->hide();
			return false;
		}
	);
}

void Ui::_updateAppProgressWindowInfo()
{
	LIMIT_UPDATE(Description, .5f, {
		if (_window_wait_update_unblock->isShow())
		{
			auto  disc_text = Localization::Str{ "str_window_wait_update_unblock" }();
			float progress	= _unblock->appUpdateProgress();
			_window_wait_update_unblock->setDescription(utils::format(disc_text, progress));
		}
	})
}
