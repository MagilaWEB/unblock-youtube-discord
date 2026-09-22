#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_editable_list.h"
#include "ui_input.h"
#include "ui_list_ul.h"
#include "ui_select_list.h"
#include "ui_secondary_window.h"
#include "ui_status.h"
#include "../unblock/zapret_engine.h"

class Ui;
class Unblock;
class File;

// Single bypass page with an engine selector (Zapret2 / classic Zapret1).
// All control logic is shared; engine-specific blocks (helper settings and
// lists, fake profile selector) are shown/hidden on engine switch, and the
// version/config selectors are repopulated from the active engine.
class UiZapretPage
{
	std::shared_ptr<Ui> _ui;
	Technology			_technology{ Technology::Zapret2 };

	std::shared_ptr<File> _file_service_list;

	// Engine selector
	SELECT_LIST(_select_engine);
	// Running engine status indicator
	STATUS(_status_engine);
	// Select list for strategy version
	SELECT_LIST(_select_version_strategy);
	// Select list for config
	SELECT_LIST(_select_config);
	// Select list for the fake profile (Zapret1 only)
	SELECT_LIST(_select_fake_bin);
	// Map of service enable checkboxes
	std::map<std::string, std::shared_ptr<CheckBox>> _list_enable_services{};

	// Shared custom lists: hosts, ip-set and exclusions
	EDITABLE_LIST(_list_custom_hosts);
	EDITABLE_LIST(_list_custom_ip_set);
	EDITABLE_LIST(_list_custom_domains_exclude);
	EDITABLE_LIST(_list_custom_ip_exclude);

	struct CustomListDef
	{
		std::string_view  config_key;
		std::string_view  title;
		std::string_view  description;
		std::string_view  placeholder;
		bool			  (*validate)(std::string_view);
		Ptr<EditableList> UiZapretPage::* widget;
	};

	static std::span<const CustomListDef> customListDefs();

	// Start button
	BUTTON(_start_button);
	BUTTON(_run_auto_config_zapret);
	BUTTON(_stop_zapret);

	// Windows for strategy/config selection
	SECONDARY_WINDOW(_window_auto_start_wait);
	SECONDARY_WINDOW(_window_continue_select_strategy);

	SECONDARY_WINDOW(_window_configuration_selection_error);
	SECONDARY_WINDOW(_window_no_bypass_targets);

	// Warns when the other technology is running and this one is started.
	SECONDARY_WINDOW(_window_warning_technology_busy);

	std::atomic_bool _automatically_strategy_cancel{ false };
	std::atomic_bool _domain_testing_cancel{ false };

	// Testing
	BUTTON(_start_testing_zapret);
	SECONDARY_WINDOW(_window_wait_testing);
	SECONDARY_WINDOW(_window_info_testing);
	UL_LIST(_list_host);
	UL_LIST(_list_host_info);

	// Last known states, to touch the DOM only on change (Ui::update ticks).
	bool					  _last_running{ false };
	std::optional<Technology> _last_active{};

public:
	explicit UiZapretPage(std::shared_ptr<Ui> ui);

	Technology technology() const { return _technology; }

	void initialize();

	// Refreshes the start button and the status line on state change.
	// Cheap: touches the DOM only on change, called from Ui::update.
	void updateState();

	// Pulls the service checkboxes from the shared state. Cheap: touches
	// the DOM only on change, called from Ui::update.
	void updateServices();

private:
	// Per-technology userConfig section for version/config/fake_bin.
	std::string_view _rememberSection() const
	{
		return _technology == Technology::Zapret1 ? "REMEMBER_CONFIGURATION_ZAPRET1" : "REMEMBER_CONFIGURATION";
	}
	// Auto-pick wait description: Zapret1 has no helper, so it gets its own text.
	std::string_view _autoStartWaitDescription() const
	{
		return _technology == Technology::Zapret1 ? "str_window_auto_start_wait_description_zapret1" : "str_window_auto_start_wait_description";
	}
	// Display name of a technology for messages.
	static std::string _technologyName(Technology technology);

	void _selectEngine();
	void _applyTechnology(Technology technology);

	/** Switches the page to another engine, warning first when the other
	 *  engine is currently running (two engines cannot run at once). */
	void _requestTechnologySwitch(Technology technology);

	void _listEnableServices();
	void _listEnableServicesUpdate();

	// Resolves the wanted checkbox state (userConfig, then config defaults)
	// and applies it with an Unblock sync. With force=false it is a no-op
	// when the widget already shows the wanted state.
	void _applyServiceState(const std::string& name, const std::shared_ptr<CheckBox>& check_box, bool force);

	void _initCustomLists();
	void _saveCustomLists();

	void _selectStrategyVersion();
	void _selectStrategyVersionUpdate();

	void _selectConfig();
	void _selectConfigUpdate();

	// Fake profile selector (Zapret1 only).
	void _initFakeKey();
	void _selectFakeBin();
	void _selectFakeBinUpdate();

	void _initMainControls();

	void _testingInit();

	void _buttonUpdate();
	void _updateStatus(std::optional<Technology> active);

	/** Something to bypass: an enabled service or a custom host / IP. */
	bool _hasBypassTargets() const;
	/** Shows the "nothing to bypass" window and returns false when there is none. */
	bool _requireBypassTargets();

	/** Runs proceed() right away, or after the user confirms stopping the
	 *  other technology when it is running. */
	void _startWithTechnologyCheck(std::function<void()>&& proceed);

	void _clickStartService();

	void _autoStart();
	bool _autoStartTryNext() const;

	// Mirrors the engine's active fake profile into the selector and the
	// saved config (Zapret1 only). The autopick loop advances profiles
	// behind the UI's back, so without this the selector would show a
	// profile that is no longer active (cancel / winning combination).
	void _syncFakeProfile();

	void _startServiceFromConfig();

	void _tcpGlobalChange(bool state = false) const;

	void _initTestingWindow();
	void _testingServiceDomains();
};
