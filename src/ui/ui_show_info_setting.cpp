#include "ui.h"
#include "ui_base.h"

void Ui::_initShowInfoSetting()
{
	_show_info_selected_service_setting->create(".buttons_start", "str_show_info_selected_service_setting");
	_show_info_selected_service_setting->addEventClick(
		[this](JSArgs)
		{
			auto config			 = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config");
			auto not_selected	 = Localization::Str{ "str_not_selected" }();
			auto format_template = Localization::Str{ "str_window_info_selected_service_setting" }();
			auto config_str		 = config ? config.value() : not_selected;

			_window_info_selected_service_setting->setDescription(
				utils::format(format_template, config_str, JSToCPP(_unblock_select_version_strategy->getSelectedOptionValue()))
			);
			_window_info_selected_service_setting->show();
			return false;
		}
	);

	_window_info_selected_service_setting->create(Localization::Str{ "str_info" }, "");
	_window_info_selected_service_setting->setType(SecondaryWindow::Type::OK);
	_window_info_selected_service_setting->addEventOk(
		[this](JSArgs)
		{
			_window_info_selected_service_setting->hide();
			return false;
		}
	);
}