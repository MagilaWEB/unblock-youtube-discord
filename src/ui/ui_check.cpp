#include "ui.h"
#include "ui_base.h"

void Ui::_checkValidRootDirectory()
{
	if (!_ui_base->hasCyrillicOrSpaceInBinaryPath())
	{
		_window_root_directory_error->create(
			Localization::Str{ "str_error" },
			utils::format(Localization::Str{ "str_root_directory_error" }(), Core::get().binariesPath().parent_path().string())
		);
		_window_root_directory_error->setType(SecondaryWindow::Type::OK);

		_window_root_directory_error->addEventOk(
			[this](JSArgs)
			{
				_ui_base->console(false);
				_ui_base->OnClose(nullptr);
				return false;
			}
		);

		_window_root_directory_error->show();
	}
}
