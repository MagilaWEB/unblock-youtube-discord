#include "ui_unblock.h"

#include "ui.h"

#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"

UiUnblock::UiUnblock(std::shared_ptr<Ui> ui) : _ui(std::move(ui))
{
}

void UiUnblock::initialize()
{
	_showConsole();
	_testDomainsStartup();
	_keepPhysicalSize();
	_stopService();
}

void UiUnblock::_showConsole()
{
#ifndef DEBUG
	{
		_show_console
			->create("#unblock section .common", "str_checkbox_show_console_title", Localization::Str{ "str_checkbox_show_console_description" });
		_show_console->addTutorialStep("str_tour_show_console_title", "str_tour_show_console_description", 7);

		auto result = _ui->userConfig()->parameterSection<bool>("SYSTEM", "show_console");
		_show_console->setState(result ? result.value() : false);

		_show_console->addEventClick(
			[self = _ui](JSArgs args)
			{
				self->console(JSToCPP<bool>(args[0]));
				self->userConfig()->writeSectionParameter("SYSTEM", "show_console", JSToCPP(args[0]));
				return false;
			}
		);
	}
#endif
}

void UiUnblock::_keepPhysicalSize()
{
	_keep_physical_size->create(
		"#unblock section .common",
		"str_checkbox_keep_physical_size_title",
		Localization::Str{ "str_checkbox_keep_physical_size_description" }
	);

	// Enabled by default; the engine reads it at startup into a window
	// property, so toggling applies on restart.
	const auto result = _ui->userConfig()->parameterSection<bool>("WINDOW", "keep_physical_size");
	_keep_physical_size->setState(!result || result.value());

	_keep_physical_size->addEventClick(
		[self = _ui](JSArgs args)
		{
			self->userConfig()->writeSectionParameter("WINDOW", "keep_physical_size", JSToCPP(args[0]));
			return false;
		}
	);
}

void UiUnblock::_testDomainsStartup()
{
	_testing_domains_startup
		->create("#unblock section .common", "str_checkbox_testing_startup_title", Localization::Str{ "str_checkbox_testing_startup_description" });
	_testing_domains_startup->addTutorialStep("str_tour_testing_startup_title", "str_tour_testing_startup_description", 8);

	const auto result = _ui->userConfig()->parameterSection<bool>("TESTING", "startup");
	_testing_domains_startup->setState(result ? result.value() : false);

	_testing_domains_startup->addEventClick(
		[self = _ui](JSArgs args)
		{
			self->userConfig()->writeSectionParameter("TESTING", "startup", JSToCPP(args[0]));
			return false;
		}
	);
}

void UiUnblock::_stopService()
{
	_window_wait_stop_service->create(Localization::Str{ "str_please_wait" }, "str_window_service_stop_wait_description");
	_window_wait_stop_service->setType(SecondaryWindow::Type::Info);

	_stop_service_all->create("#unblock .common", "str_b_stop_service_all");
	_stop_service_all->addTutorialStep("str_tour_stop_all_title", "str_tour_stop_all_description", 9);
	_stop_service_all->addEventClick(
		[this](JSArgs)
		{
			_window_wait_stop_service->show();
			Core::get().addTask(
				[this]
				{
					stopAllServices();
					_window_wait_stop_service->hide();
				}
			);

			return false;
		}
	);
}

void UiUnblock::stopAllServices() const
{
	_ui->_unblock->removeService();
	_ui->_unblock->localProxyTg(false);
	_ui->_unblock->dnsHosts(false);
	_ui->_ui_proxy_tg->getCheckBoxProxyTg()->setState(false);
}
