#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

void Ui::_stopInit()
{
	_window_wait_stop_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_stop_wait_description");
	_window_wait_stop_service->setType(SecondaryWindow::Type::Info);

	_stop_unblock->create(".buttons_stop", "str_b_stop_unblock");
	_stop_unblock->addEventClick(
		[this](JSArgs)
		{
			_window_wait_stop_service->show();
			Core::get().addTask(
				[this]
				{
					_stoppingServices();
					_window_wait_stop_service->hide();
				}
			);
			return false;
		}
	);

	_stop_service_all->create(".buttons_stop", "str_b_stop_service_all");
	_stop_service_all->addEventClick(
		[this](JSArgs)
		{
			_window_wait_stop_service->show();
			Core::get().addTask(
				[this]
				{
					_stoppingAllServices();

					_window_wait_stop_service->hide();
				}
			);

			return false;
		}
	);

	_buttonUpdate();
}

void Ui::_stoppingServices()
{
	_unblock.removeService();

	_buttonUpdate();
	_activeServiceUpdate();

	_tcpGlobalChange(false);
}

void Ui::_stoppingAllServices()
{
	_unblock.localProxyTg(false);
	_proxy_tg_enable->setState(false);

	_stoppingServices();
}
