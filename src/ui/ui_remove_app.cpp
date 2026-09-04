#include "ui.h"

#include <windows.h>

#include <filesystem>

void Ui::_removeApp()
{
	_remove_app->create("#unblock section .common", "str_button_remove");
	_remove_app->addTutorialStep("str_tour_remove_app_title", "str_tour_remove_app_description", 17);
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

void Ui::_removeAppRun()
{
	_ui_unblock->stopAllServices();

	_unblock->dnsHosts(false);
	console(false);

	// Uninstall is delegated to the standalone unblock_update.exe. We launch a
	// %TEMP% copy of it because it must outlive engine.exe and then remove the
	// application root it does not reside in itself.
	const auto		temp_root = Core::get().tempPath() / "unblock";
	std::error_code ec;
	std::filesystem::create_directories(temp_root, ec);

	const auto updater = Core::get().binPath() / "unblock_update.exe";
	const auto self	   = temp_root / "unblock_update.exe";
	std::filesystem::copy_file(updater, self, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
	{
		Debug::error("Failed to prepare unblock_update: {}", ec.message());
		OnClose(nullptr);
		return;
	}

	std::wstring cmd_line =
		L"\"" + self.wstring() + L"\" \"" + Core::get().currentPath().wstring() + L"\" " + std::to_wstring(GetCurrentProcessId()) + L" remove";

	STARTUPINFOW		startup{};
	PROCESS_INFORMATION process{};
	startup.cb = sizeof(startup);

	if (!CreateProcessW(nullptr, cmd_line.data(), nullptr, nullptr, FALSE, 0, nullptr, temp_root.c_str(), &startup, &process))
		Debug::error("Failed to start unblock_update: {}", static_cast<u32>(GetLastError()));
	else
	{
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}

	OnClose(nullptr);
}
