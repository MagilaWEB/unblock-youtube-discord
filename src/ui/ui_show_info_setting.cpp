#include "ui.h"
#include "ui_base.h"

void Ui::_initShowInfoSetting()
{
	_show_info_selected_service_setting->create(".buttons_start", "str_show_info_selected_service_setting");
	_show_info_selected_service_setting->addEventClick(
		[this](JSArgs)
		{
			auto			  config	   = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config");
			auto			  fake_bin	   = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "fake_bin");
			auto			  config_proxy = _ui_base->userSetting()->parameterSection<std::string>("REMEMBER_CONFIGURATION", "config_proxy");
			Localization::Str not_selected{ "str_not_selected" };

			std::string description = utils::format(
				Localization::Str{ "str_window_info_selected_service_setting" }(),
				config ? config.value().c_str() : not_selected(),
				fake_bin ? fake_bin.value().c_str() : not_selected(),
				JSToCPP(_unblock_select_version_strategy->getSelectedOptionValue()),
				config_proxy ? config_proxy.value().c_str() : not_selected()
			);

			_window_info_selected_service_setting->setDescription(description.c_str());
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
