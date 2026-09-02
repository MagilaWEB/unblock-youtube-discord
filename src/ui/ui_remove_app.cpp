#include "ui.h"

void Ui::_removeApp()
{
	_remove_app->create("#unblock section .common", "str_button_remove");
	_remove_app->addTutorialStep("str_tour_remove_app_title", "str_tour_remove_app_description", 15);
	_remove_app->addEventClick(
		[ui_self = self](JSArgs)
		{
			ui_self->_window_remove_app->show();
			return false;
		}
	);

	_window_remove_app->create(Localization::Str{ "str_warning" }, "str_remove_app_description");
	_window_remove_app->setType(SecondaryWindow::Type::YesNo);

	_window_remove_app->addEventYesNo(
		[ui_self = self](JSArgs args)
		{
			if (JSToCPP<bool>(args[0]))
				ui_self->_removeAppRun();

			ui_self->_window_remove_app->hide();
			return true;
		}
	);
}

constexpr static pcstr del_update_script_cmd{ R"(
ECHO off
SET CURRENT_DIR=%~dp0

goto wait_loop

:wait_loop
tasklist /fi "imagename eq engine.exe" /v | find /i "Unblock Version:" >nul
if %errorlevel% == 0 (
    timeout /t 1 /nobreak >nul
    goto wait_loop
) else (
   goto close_unblock
)

:close_unblock

start cmd /c rd "%CURRENT_DIR%" /S /Q&exit
exit
)" };

void Ui::_removeAppRun()
{
	_ui_unblock->stopAllServices();

	_unblock->dnsHosts(false);
	console(false);

	std::string del_bat_path{ (Core::get().currentPath() / "del_unblock").string() + ".bat" };
	std::string run_bat{ "start cmd /c " + del_bat_path };

	std::fstream bat;
	bat.open(del_bat_path.c_str(), std::ios::out | std::ios::binary);
	bat.clear();
	bat << del_update_script_cmd;
	bat.close();

	while (!bat.is_open())
		bat.open(del_bat_path.c_str(), std::ios::in);
	bat.close();

	system(run_bat.c_str());

	OnClose(nullptr);
}
