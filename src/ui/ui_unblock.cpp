#include "ui_unblock.h"

#include "ui.h"
#include "ui_base.h"

#include "../unblock/unblock.h"

#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"

UiUnblock::UiUnblock(std::shared_ptr<Ui> ui) : _ui(std::move(ui))
{
}

void UiUnblock::initialize()
{
    _showConsole();
    _testDomainsStartup();
}

void UiUnblock::_showConsole()
{
#ifndef DEBUG
    {
        _show_console
            ->create("#unblock section .common", "str_checkbox_show_console_title", Localization::Str{ "str_checkbox_show_console_description" });

        auto result = _ui->uiBase()->userSetting()->parameterSection<bool>("SYSTEM", "show_console");
        _show_console->setState(result ? result.value() : false);

        _show_console->addEventClick(
            [self = _ui](JSArgs args)
            {
                self->uiBase()->console(JSToCPP<bool>(args[0]));
                self->uiBase()->userSetting()->writeSectionParameter("SYSTEM", "show_console", JSToCPP(args[0]));
                return false;
            }
        );
    }
#endif
}

void UiUnblock::_testDomainsStartup()
{
    _testing_domains_startup
        ->create("#unblock section .common", "str_checkbox_testing_startup_title", Localization::Str{ "str_checkbox_testing_startup_description" });

    const auto result = _ui->uiBase()->userSetting()->parameterSection<bool>("TESTING", "startup");
    _testing_domains_startup->setState(result ? result.value() : false);

    _testing_domains_startup->addEventClick(
        [self = _ui](JSArgs args)
        {
            self->uiBase()->userSetting()->writeSectionParameter("TESTING", "startup", JSToCPP(args[0]));
            return false;
        }
    );
}
