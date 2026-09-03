#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_editable_list.h"
#include "ui_list_ul.h"
#include "ui_select_list.h"
#include "ui_secondary_window.h"

class Ui;
class Unblock;
class File;

class UiZapret2
{
	std::shared_ptr<Ui> _ui;
	std::shared_ptr<File> _file_service_list;

	// Select list for strategy version
	SELECT_LIST(_select_version_strategy);
	// Select list for config
	SELECT_LIST(_select_config);
	// Map of service enable checkboxes
	std::map<std::string, std::shared_ptr<CheckBox>> _list_enable_services{};

	// Кастомные списки: хосты, ip-set и исключения
	EDITABLE_LIST(_list_custom_hosts);
	EDITABLE_LIST(_list_custom_ip_set);
	EDITABLE_LIST(_list_custom_domains_exclude);
	EDITABLE_LIST(_list_custom_ip_exclude);

	// Start button
	BUTTON(_start_button);
	BUTTON(_run_auto_config_zapret);
	BUTTON(_stop_zapret);

	// Windows for strategy/config selection
	SECONDARY_WINDOW(_window_auto_start_wait);
	SECONDARY_WINDOW(_window_continue_select_strategy);

	SECONDARY_WINDOW(_window_configuration_selection_error);

	std::atomic_bool _automatically_strategy_cancel{ false };
	std::atomic_bool _domain_testing_cancel{ false };

	// Testing
	BUTTON(_start_testing_zapret);
	SECONDARY_WINDOW(_window_wait_testing);
	SECONDARY_WINDOW(_window_info_testing);
	UL_LIST(_list_host);
	UL_LIST(_list_host_info);

	// Hosts currently being checked by zapret-helper
	UL_LIST(_list_helper_checking);
	std::vector<std::string> _last_helper_checking;

	// Hosts that have been checked by zapret-helper at least once
	UL_LIST(_list_helper_seen);
	std::vector<std::string> _last_helper_seen;

	// Valid hosts with confirmed strategy (zapret statistics)
	UL_LIST(_list_helper_valid);
	std::vector<std::pair<std::string, std::string>> _last_helper_valid;

	// Hosts with current errors
	UL_LIST(_list_helper_error);
	std::vector<std::pair<std::string, std::string>> _last_helper_error;

public:
	UiZapret2(std::shared_ptr<Ui> ui);

	const Ptr<SelectList>& getSelectVersionStrategy() const { return _select_version_strategy; }
	const Ptr<SelectList>& getSelectConfig() const { return _select_config; }

	const Ptr<Button>&			getStartButton() { return _start_button; }
	const Ptr<SecondaryWindow>& getWindowAutoStartWait() { return _window_auto_start_wait; }
	const Ptr<SecondaryWindow>& getWindowContinueSelectStrategy() { return _window_continue_select_strategy; }
	const Ptr<SecondaryWindow>& getWindowConfigurationSelectionError() { return _window_configuration_selection_error; }

	void initialize();

	/** Update the list of hosts that zapret-helper is checking (tick from Ui::update). */
	void updateHelperChecking();

	/** Update the list of hosts that have ever been checked via zapret (tick from Ui::update). */
	void updateHelperSeen();

	/** Update the list of valid hosts with a confirmed strategy (tick from Ui::update). */
	void updateHelperValid();

	/** Update the list of hosts with current errors (tick from Ui::update). */
	void updateHelperError();

private:
	void _listEnableServices();
	void _listEnableServicesUpdate();

	void _initCustomLists();
	void _saveCustomLists();

	void _selectStrategyVersion();
	void _selectStrategyVersionUpdate();

	void _selectConfig();
	void _selectConfigUpdate();

	void _initMainControls();

	void _testingInit();

	void _buttonUpdate();

	void _clickStartService();

	void _autoStart();
	bool _autoStartTryNext() const;

	void _startServiceFromConfig();

	void _tcpGlobalChange(bool state = false) const;

	void _initTestingWindow();
	void _testingServiceDomains();

	void _initHelperChecking();
	void _initHelperSeen();
	void _initHelperValid();
	void _initHelperError();
};
