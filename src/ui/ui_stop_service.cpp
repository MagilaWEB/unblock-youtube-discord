#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

#ifdef __clang__
	#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"
#endif

void Ui::_stopInit()
{
	_window_wait_stop_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_stop_wait_description");
	_window_wait_stop_service->setType(SecondaryWindow::Type::Info);

	_stop_zapret->create("#zapret .common", "str_b_stop_zapret");
	_stop_zapret->addEventClick(
		[self = self](JSArgs)
		{
			self->_window_wait_stop_service->show();
			Core::get().addTask(
				[self = self]
				{
					self->_stoppingServices();
					self->_window_wait_stop_service->hide();
				}
			);
			return false;
		}
	);

	_run_auto_config_zapret->create("#zapret .common", "str_b_run_auto_config_zapret");

	_run_auto_config_zapret->addEventClick(
		[self = self](JSArgs)
		{
			//_autoStart();
			return false;
		}
	);

	_stop_service_all->create("#unblock .common", "str_b_stop_service_all");
	_stop_service_all->addEventClick(
		[self = self](JSArgs)
		{
			self->_window_wait_stop_service->show();
			Core::get().addTask(
				[self = self]
				{
					self->_stoppingAllServices();

					self->_window_wait_stop_service->hide();
				}
			);

			return false;
		}
	);

}

void Ui::_stoppingServices()
{
	_unblock->removeService();

	//_tcpGlobalChange(false);
}

void Ui::_stoppingAllServices()
{
	_unblock->localProxyTg(false);
	_ui_proxy_tg->getCheckBoxProxyTg()->setState(false);

	_stoppingServices();
}
