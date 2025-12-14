#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_stopInit()
{
	_window_wait_stop_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_stop_wait_description");
	_window_wait_stop_service->setType(SecondaryWindow::Type::Wait);

	_stop_unblock->create("#home section .buttons_stop", "str_b_stop_unblock");
	_stop_unblock->addEventClick(
		[this](JSArgs)
		{
			_stoppingServices(eUnblock);
			return false;
		}
	);

	_stop_proxy_dpi->create("#home section .buttons_stop", "str_b_stop_proxy_dpi");
	_stop_proxy_dpi->addEventClick(
		[this](JSArgs)
		{
			_stoppingServices(eProxyDpi);
			return false;
		}
	);

	_stop_service_all->create("#home section .buttons_stop", "str_b_stop_service_all");
	_stop_service_all->addEventClick(
		[this](JSArgs)
		{
			_stoppingServices(StoppingService(eUnblock | eProxyDpi));
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

			if (type & StoppingService::eUnblock)
				_unblock->removeService();

			if (type & StoppingService::eProxyDpi)
				_unblock->removeService(true);

			_buttonUpdate();
			_activeServiceUpdate();
			_window_wait_stop_service->hide();
		}
	);
}