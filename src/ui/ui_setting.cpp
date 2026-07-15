#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

#ifdef __clang__
	#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"
#endif

void Ui::_settingInit()
{
	_settingShowConsole();
	_settingTestDomainsStartup();
	_ui_dns_hosts->initialize();
	_ui_proxy_tg->initialize();

	if (_ui_zapret2)
		_ui_zapret2->initialize();
}

void Ui::_settingShowConsole()
{
#ifndef DEBUG
	{
		_show_console
			->create("#unblock section .common", "str_checkbox_show_console_title", Localization::Str{ "str_checkbox_show_console_description" });

		auto result = _ui_base->userSetting()->parameterSection<bool>("SYSTEM", "show_console");
		_show_console->setState(result ? result.value() : false);

		_show_console->addEventClick(
			[self = self](JSArgs args)
			{
				self->_ui_base->console(JSToCPP<bool>(args[0]));
				self->_ui_base->userSetting()->writeSectionParameter("SYSTEM", "show_console", JSToCPP(args[0]));
				return false;
			}
		);
	}
#endif
}

void Ui::_settingTestDomainsStartup()
{
	_testing_domains_startup
		->create("#unblock section .common", "str_checkbox_testing_startup_title", Localization::Str{ "str_checkbox_testing_startup_description" });

	auto result = _ui_base->userSetting()->parameterSection<bool>("TESTING", "startup");
	_testing_domains_startup->setState(result ? result.value() : false);

	_testing_domains_startup->addEventClick(
		[self = self](JSArgs args)
		{
			self->_ui_base->userSetting()->writeSectionParameter("TESTING", "startup", JSToCPP(args[0]));
			return false;
		}
	);
}


