#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_list_ul.h"
#include "ui_select_list.h"
#include "ui_secondary_window.h"

class Ui;
class Unblock;
class File;

class UiZapret2
{
	std::shared_ptr<Ui> _ui;
	std::shared_ptr<Unblock> _unblock;
	std::shared_ptr<File> _file_service_list;

	// Select list for strategy version
	SELECT_LIST(_select_version_strategy);
	// Select list for config
	SELECT_LIST(_select_config);
	// Map of service enable checkboxes
	std::map<std::string, std::shared_ptr<CheckBox>> _list_enable_services{};

	// Start button
	BUTTON(_start_button);

	// Windows for strategy/config selection
	SECONDARY_WINDOW(_window_config_not_found);
	SECONDARY_WINDOW(_window_config_found);
	SECONDARY_WINDOW(_window_auto_start_wait);
	SECONDARY_WINDOW(_window_continue_select_strategy);

	SECONDARY_WINDOW(_window_configuration_selection_error);

	std::atomic_bool _automatically_strategy_cancel{ false };

	// Testing
	BUTTON(_start_testing_zapret);
	SECONDARY_WINDOW(_window_wait_testing);
	UL_LIST(_list_domain);

public:
	UiZapret2(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock, std::shared_ptr<File> file_service_list);

	const Ptr<SelectList>& getSelectVersionStrategy() const { return _select_version_strategy; }
	const Ptr<SelectList>& getSelectConfig() const { return _select_config; }

	const Ptr<Button>&			getStartButton() { return _start_button; }
	const Ptr<SecondaryWindow>& getWindowConfigNotFound() { return _window_config_not_found; }
	const Ptr<SecondaryWindow>& getWindowConfigFound() { return _window_config_found; }
	const Ptr<SecondaryWindow>& getWindowAutoStartWait() { return _window_auto_start_wait; }
	const Ptr<SecondaryWindow>& getWindowContinueSelectStrategy() { return _window_continue_select_strategy; }
	const Ptr<SecondaryWindow>& getWindowConfigurationSelectionError() { return _window_configuration_selection_error; }

	void initialize();


	void testingInit();
	void testingUpdate() const;

private:
	void _settingListEnableServices();
	void _settingListEnableServicesUpdate();

	void _settingSelectStrategyVersion();
	void _settingSelectStrategyVersionUpdate();

	void _settingSelectConfig();
	void _settingSelectConfigUpdate();

	void _startInit();

	void _buttonUpdate();

	void _clickStartService();

	void _autoStart();

	void _startServiceFromConfig();

	void _tcpGlobalChange(bool state = false) const;

	void _testingWindow();
	void _testingServiceDomains();
};
