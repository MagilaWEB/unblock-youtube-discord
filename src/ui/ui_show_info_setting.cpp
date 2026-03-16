#include "ui.h"
#include "ui_base.h"

void Ui::_initShowInfoSetting()
{
	_show_info_selected_service_setting->create(".buttons_start", "str_show_info_selected_service_setting");
	_show_info_selected_service_setting->addEventClick(
		[this](JSArgs)
		{
			auto config	  = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config");
			auto fake_bin = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "fake_bin");

			std::string		 not_selected	 = Localization::Str{ "str_not_selected" }();
			std::string		 format_template = Localization::Str{ "str_window_info_selected_service_setting" }();
			std::string		 config_str		 = config ? config.value() : not_selected;
			std::string		 fake_bin_str	 = fake_bin ? fake_bin.value() : not_selected;
			std::string		 version_str	 = JSToCPP(_unblock_select_version_strategy->getSelectedOptionValue());
			std::string		 description	 = utils::format(format_template, config_str, fake_bin_str, version_str);

			_window_info_selected_service_setting->setDescription(description);
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
