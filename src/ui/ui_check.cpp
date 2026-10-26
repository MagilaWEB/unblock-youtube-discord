module ui;

import std;
import core;
import core.localization;
import core.utils;
import dom.utils_saucer;
import ui.ui_secondary_window;

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
				ui_self->onClose(nullptr);
				return false;
			}
		);

		_window_root_directory_error->show();
	}
}
