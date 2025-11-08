#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_stopService()
{
	_window_wait_stop_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_stop_wait_description");
	_window_wait_stop_service->setType(SecondaryWindow::Type::Wait);

	_stop_service->create("#home section .button_start_stop", "str_b_stop_service");

	_stop_service->addEventClick(
		[this](JSArgs)
		{
			Core::addTask(
				[this]
				{
					_window_wait_stop_service->show();
					_unblock->removeService();
					_unblock->removeService(true);
					_updateTitleButton();
					_updateTitleButton(true);
					_window_wait_stop_service->hide();
				}
			);

			return false;
		}
	);
}
