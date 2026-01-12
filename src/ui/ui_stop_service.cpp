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
			_stoppingServices(eUnblock);
			_tcpGlobalChange(false);
			return false;
		}
	);

	_stop_proxy_dpi->create(".buttons_stop", "str_b_stop_proxy_dpi");
	_stop_proxy_dpi->addEventClick(
		[this](JSArgs)
		{
			_stoppingServices(eProxyDpi);
			return false;
		}
	);

	_stop_service_all->create(".buttons_stop", "str_b_stop_service_all");
	_stop_service_all->addEventClick(
		[this](JSArgs)
		{
			_stoppingServices(StoppingService::eUnblock);
			_stoppingServices(StoppingService::eProxyDpi);
			_tcpGlobalChange(false);
			return false;
		}
	);

	_buttonUpdate();
}

void Ui::_stoppingServices(StoppingService type)
{
	Core::get().addTask(
		[this, type]
		{
			_window_wait_stop_service->show();

			_unblock->removeService(type & StoppingService::eProxyDpi);

			_buttonUpdate();
			_activeServiceUpdate();
			_window_wait_stop_service->hide();
		}
	);
}