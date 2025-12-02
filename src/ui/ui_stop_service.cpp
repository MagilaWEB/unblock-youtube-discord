#include "ui.h"

#include "../unblock/unblock.h"

void Ui::_stopInit()
{
	_window_wait_stop_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_stop_wait_description");
	_window_wait_stop_service->setType(SecondaryWindow::Type::Wait);

	_stop_service_all->create("#home section .buttons_stop", "str_b_stop_service_all");
	_stop_service_all->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask(
				[this]
				{
					_window_wait_stop_service->show();
					_unblock->removeService();
					_unblock->removeService(true);
					_buttonUpdate();
					_activeService();
					_window_wait_stop_service->hide();
				}
			);
			return false;
		}
	);

	_stopProxy();
	_stopUnblock();
	
	//_stopTorProxy();
}

void Ui::_stopUnblock()
{
	_stop_unblock->create("#home section .buttons_stop", "str_b_stop_unblock", true);

	_stop_unblock->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask(
				[this]
				{
					_window_wait_stop_service->show();
					_unblock->removeService();
					_buttonUpdate();
					_activeService();
					_window_wait_stop_service->hide();
				}
			);

			return false;
		}
	);
}

void Ui::_stopProxy()
{
	_stop_proxy_dpi->create("#home section .buttons_stop", "str_b_stop_proxy_dpi", true);

	_stop_proxy_dpi->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask(
				[this]
				{
					_window_wait_stop_service->show();
					_unblock->removeService(true);
					_buttonUpdate();
					_activeService();
					_window_wait_stop_service->hide();
				}
			);

			return false;
		}
	);
}

//void Ui::_stopTorProxy()
//{
//	_stop_tor_proxy->remove();
//	_stop_tor_proxy->create("#home section .buttons_stop", "str_b_stop_tor_proxy");
//
//	_stop_tor_proxy->addEventClick(
//		[this](JSArgs)
//		{
//			Core::get().addTask(
//				[this]
//				{
//					_window_wait_stop_service->show();
//					_unblock->removeTor();
//					_activeService();
//					_window_wait_stop_service->hide();
//				}
//			);
//
//			return false;
//		}
//	);
//}
