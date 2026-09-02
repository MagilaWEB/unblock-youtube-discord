#include "ui.h"

void Ui::_checkValidRootDirectory()
{
	if (hasCyrillicOrSpaceInBinaryPath())
	{
		_window_root_directory_error->create(
			Localization::Str{ "str_error" },
			utils::format(Localization::Str{ "str_root_directory_error" }(), Core::get().binariesPath().parent_path().string())
		);
		_window_root_directory_error->setType(SecondaryWindow::Type::OK);

		_window_root_directory_error->addEventOk(
			[ui_self = self](JSArgs)
			{
				ui_self->console(false);
				ui_self->OnClose(nullptr);
				return false;
			}
		);

		_window_root_directory_error->show();
	}
}
