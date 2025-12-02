#include "ui.h"
#include "ui_base.h"

Ui::Ui(UiBase* ui_base) : _ui_base(ui_base)
{
	_file_service_list->open({ Core::get().configsPath() / "service_setting" }, ".config", true);

	_file_service_list->forLineParametersSection(
		"LIST",
		[this](std::string key, std::string /*value*/)
		{
			_unblock_list_enable_services.emplace(key, std::make_shared<CheckBox>(std::string{ "_unblock_to_list_" } + key));
			return false;
		}
	);

	
}

void Ui::initialize()
{
	auto js_global = JSGlobalObject();
	_updateCountStartStopButtonToCss = js_global["updateCountStartStopButtonToCss"];

	_checkConflictService();

	_setting();

	// HOME
	_startInit();
	_stopInit();
	_testingInit();

	_footerElements();
}

void Ui::_footerElements()
{
	_link_to_github->create("footer", "str_link_to_github");
	_link_to_github->addEventClick(
		[](JSArgs)
		{
			Core::get().addTask([] { system("start https://github.com/MagilaWEB/unblock-youtube-discord"); });
			return false;
		}
	);
}

void Ui::_checkConflictService()
{
	_window_warning_conflict_service->create(Localization::Str{ "str_warning" }, "");
	_window_warning_conflict_service->setType(SecondaryWindow::Type::YesNo);

	static Localization::Str lang_disc{ "str_window_warning_conflict_service" };

	std::string description = lang_disc();

	auto& conflict_service = _unblock->getConflictingServices();
	if (!conflict_service.empty())
	{
#if __clang__
		[[clang::no_destroy]]
#endif
		static std::string names_services;

		for (auto& service : conflict_service)
			names_services.append(service.getName()).append(",");
		names_services.pop_back();

		description = utils::format(description.c_str(), names_services.c_str());
		_window_warning_conflict_service->setDescription(description.c_str());

		_window_warning_conflict_service->show();

		_window_warning_conflict_service->addEventYesNo(
			[this, &conflict_service](JSArgs args)
			{
				if (args[0].ToBoolean())
					for (auto& service : conflict_service)
						service.remove();

				conflict_service.clear();

				_window_warning_conflict_service->hide();

				return true;
			}
		);
	}
}
