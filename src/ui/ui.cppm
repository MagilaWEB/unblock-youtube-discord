module;

#include "ui_macros.hpp"

export module ui;

import std;
import core.file_system;
import core.localization;
import core.ptr;
import core.types;
import dom.utils_saucer;
import ext.saucer;
import ui.engine_api;
import ui.ui_background_tasks;
import ui.ui_button;
import ui.ui_check_box;
import ui.ui_editable_list;
import ui.ui_input;
import ui.ui_list_ul;
import ui.ui_secondary_window;
import ui.ui_select_list;
import ui.ui_status;
import ui.ui_text;
import unblock;
import unblock.dns_proxy;
import unblock.tg_proxy;
import unblock.zapret_engine;

export import core.utils;
export import unblock;

export import ui.engine_api;
export import ui.ui_background_tasks;
export import ui.ui_base_element;
export import ui.ui_button;
export import ui.ui_check_box;
export import ui.ui_editable_list;
export import ui.ui_input;
export import ui.ui_list_ul;
export import ui.ui_secondary_window;
export import ui.ui_select_list;
export import ui.ui_status;
export import ui.ui_text;

// The page components below hold a shared_ptr<Ui> and are the only consumers
// of Ui's private state. They live in this module (Ui's module): a class
// cannot be forward-declared across a module boundary, so the other unit's
// declaration would be a different entity — and importing them back into
// `ui` would close an import cycle CMake rejects.
export class Ui;

export class UiUnblock
{
	std::shared_ptr<Ui> _ui;

	CHECK_BOX(_show_console);
	CHECK_BOX(_testing_domains_startup);
	CHECK_BOX(_keep_physical_size);

	// Stop all services
	BUTTON(_stop_service_all);
	SECONDARY_WINDOW(_window_wait_stop_service);

public:
	explicit UiUnblock(std::shared_ptr<Ui> ui);

	void initialize();

	const Ptr<CheckBox>&		getTestingDomainsStartup() const { return _testing_domains_startup; }
	const Ptr<SecondaryWindow>& getWindowWaitStopService() { return _window_wait_stop_service; }
	void						stopAllServices() const;

private:
	void _showConsole();
	void _testDomainsStartup();
	void _keepPhysicalSize();
	void _stopService();
};

export class UiDnsProxy
{
private:
	std::shared_ptr<Ui> _ui;
	DnsProxy&			_dns_proxy;

	CHECK_BOX(_enable_dns_proxy);
	STATUS(_status_dns);
	EDITABLE_LIST(_upstreams);
	INPUT(_bootstrap);
	INPUT(_timeout);
	INPUT(_test_input);
	BUTTON(_test_button);

	SECONDARY_WINDOW(_window_test_result);
	SECONDARY_WINDOW(_window_enable_dns_proxy);

	std::string _last_status;

public:
	UiDnsProxy(std::shared_ptr<Ui> ui, DnsProxy& dns_proxy);

	void initialize();
	void updateInfoWindow();

	/** Persists DNS/enable, syncs the checkbox and applies the whole DNS
	 *  feature (proxy service + own hosts pins) in a background task. */
	void setEnabled(bool state);

	const Ptr<CheckBox>& getCheckBoxDnsProxy() const { return _enable_dns_proxy; }

private:
	void _collectUpstreams();
	void _applyUpstreams();
	void _refreshStatus();
	void _applyState(bool state);
};

export class UiProxyTg
{
private:
	std::shared_ptr<Ui> _ui;
	TgProxy&			_tg_proxy;

	CHECK_BOX(_proxy_tg_enable);
	BUTTON(_proxy_link_tg);
	BUTTON(_proxy_link_tg_copy);

	INPUT(_proxy_tg_host);
	INPUT(_proxy_tg_port);
	INPUT(_proxy_tg_dc_ip_1);
	INPUT(_proxy_tg_dc_ip_2);
	INPUT(_proxy_tg_dc_ip_3);
	INPUT(_proxy_tg_dc_ip_4);
	// INPUT(_proxy_tg_cfproxy_domain);
	BUTTON(_proxy_tg_apply);

public:
	UiProxyTg(std::shared_ptr<Ui> ui, TgProxy& tg_proxy);

	void initialize();

	const Ptr<CheckBox>& getCheckBoxProxyTg() const { return _proxy_tg_enable; }

private:
	void		_enableProxyTg();
	void		_enableProxyLinkTg();
	void		_proxySettings();
	void		_applyProxySettings();
	std::string _settingValue(std::string_view key, std::string_view default_value);
};

export class UiZapretHelper
{
	std::shared_ptr<Ui> _ui;
	std::string			_root;

	// Hosts currently being checked by zapret-helper
	UL_LIST(_list_helper_checking);
	std::vector<std::string> _last_helper_checking;
	std::string				 _last_helper_checking_title;

	// Hosts that have been checked by zapret-helper at least once
	UL_LIST(_list_helper_seen);
	std::vector<std::string> _last_helper_seen;

	// Valid hosts with confirmed strategy (zapret statistics)
	UL_LIST(_list_helper_valid);
	std::vector<std::pair<std::string, std::string>> _last_helper_valid;

	// Hosts with current errors
	UL_LIST(_list_helper_error);
	std::vector<std::pair<std::string, std::string>> _last_helper_error;

	// Hosts fully tried on every strategy (dead, nothing left to attempt)
	UL_LIST(_list_helper_exhausted);
	std::vector<std::pair<std::string, std::string>> _last_helper_exhausted;

	// Hosts the helper tried, but Lua never judged (no valid/error/exhausted)
	UL_LIST(_list_helper_unjudged);
	std::vector<std::string> _last_helper_unjudged;

	// Live effectiveness line at the bottom of the general settings block:
	// valid / (valid+error+exhausted)
	TEXT_LABEL(_helper_summary);

	// Helper runtime settings ([HELPER] section). The on-disk file is stale
	// while unblock runs, so Apply writes userConfig (memory) + pushes UDP
	// CONFIG: to the running helper + stores the message in Unblock for the
	// next startService() push.
	INPUT(_helper_pool);
	INPUT(_helper_recheck_min);
	INPUT(_helper_errors_progress_min);
	INPUT(_helper_errors_recheck_sec);

	bool _visible{ true };

	struct HelperSettingDef
	{
		std::string_view key;
		Input::Types	 type;
		u32				 fallback;
		u32				 min_value;
		u32				 max_value;
		std::string_view unit;
		std::string_view title;
		std::string_view description;
		Ptr<Input> UiZapretHelper::* widget;
	};

	static std::span<const HelperSettingDef> helperSettingDefs();

public:
	UiZapretHelper(std::shared_ptr<Ui> ui, std::string_view root);

	void initialize();

	// Shows/hides the whole helper block (only meaningful on Zapret2).
	// Cheap: touches the DOM only on change.
	void setVisible(bool visible);

	void updateChecking();
	void updateSeen();
	void updateValid();
	void updateError();
	void updateExhausted();
	void updateUnjudged();
	void updateSummary();

private:
	std::string _sel(std::string_view tail) const { return _root + std::string{ tail }; }

	void _initHelperChecking();
	void _initHelperSeen();
	void _initHelperValid();
	void _initHelperError();
	void _initHelperExhausted();
	void _initHelperUnjudged();
	void _initHelperSummary();

	void _initHelperSettings();
	void _applyHelperSettings();
	u32	 _helperSettingU32(std::string_view key, u32 fallback) const;
	void _pushHelperSettings() const;
};

// Single bypass page with an engine selector (Zapret2 / classic Zapret1).
// All control logic is shared; engine-specific blocks (helper settings and
// lists, fake profile selector) are shown/hidden on engine switch, and the
// version/config selectors are repopulated from the active engine.
export class UiZapretPage
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

	// Mirrors the engine's active fake profile into the selector and the
	// saved config (Zapret1 only). The autopick loop advances profiles
	// behind the UI's back, so without this the selector would show a
	// profile that is no longer active (cancel / winning combination).
	void _syncFakeProfile();

	void _startServiceFromConfig();

	void _initTestingWindow();
	void _testingServiceDomains();
};

export class Ui final : public utils::DefaultInit,
						public std::enable_shared_from_this<Ui>
{
	IEngineAPI* _engine;

	std::shared_ptr<Unblock> _unblock;

	bool _init{ false };

	// Update unblock app
	CHECK_BOX(_enable_check_update_startup);
	BUTTON(_start_check_update_app);

	// Remove app
	BUTTON(_remove_app);
	SECONDARY_WINDOW(_window_remove_app);

	std::unique_ptr<UiDnsProxy>		   _ui_dns_proxy;
	std::unique_ptr<UiProxyTg>		   _ui_proxy_tg;
	std::unique_ptr<UiZapretPage>	   _ui_zapret;
	std::unique_ptr<UiZapretHelper>	   _ui_zapret_helper;
	std::unique_ptr<UiUnblock>		   _ui_unblock;
	std::unique_ptr<UiBackgroundTasks> _ui_background_tasks;

	// Root directory error
	SECONDARY_WINDOW(_window_root_directory_error);

	// Update unblock app
	SECONDARY_WINDOW(_window_update_unblock);
	SECONDARY_WINDOW(_window_wait_update_unblock);
	SECONDARY_WINDOW(_window_error_update_unblock);

	SECONDARY_WINDOW(_window_warning_conflict_service);

	SECONDARY_WINDOW(_window_warning_whitelist);
	SECONDARY_WINDOW(_window_wait_test_whitelist);
	SECONDARY_WINDOW(_window_warning_no_internet);
	SECONDARY_WINDOW(_window_wait_start_service);

	// About
	BUTTON(_link_to_github);
	BUTTON(_link_to_telegram);
	BUTTON(_link_report_issue);
	BUTTON(_button_report_issue_tutorial);

public:
	std::shared_ptr<Ui> self;

	explicit Ui(IEngineAPI* engine);

	void initialize();
	void postConstruct();

	void update();

	const std::shared_ptr<File>& userConfig();
	bool						 hasCyrillicOrSpaceInBinaryPath() const { return _engine->hasCyrillicOrSpaceInBinaryPath(); }

	void		console(bool show);
	std::string langText(std::string_view text_id);

	// App termination (used when uninstalling the program).
	void onClose(saucer::application*);

	// Registers the JS bridge (expose/inject) and window subscriptions; called before navigation.
	void setup(saucer::smartview* view);

	const Ptr<SecondaryWindow>& getWindowWaitStartService() { return _window_wait_start_service; }

	UiBackgroundTasks* backgroundTasks() { return _ui_background_tasks.get(); }

	const Ptr<CheckBox>& getTestingDomainsStartup() const { return _ui_unblock->getTestingDomainsStartup(); }

	std::unique_ptr<UiUnblock>& getUiUnblock() { return _ui_unblock; }

	// Accessors for the page components (used to be friends of Ui).
	const std::shared_ptr<Unblock>& unblock() const { return _unblock; }
	UiDnsProxy*						uiDnsProxy() { return _ui_dns_proxy.get(); }
	UiProxyTg*						uiProxyTg() { return _ui_proxy_tg.get(); }

private:
	void _domReady();
	void _closeWindow();

	void _initializeAppState();
	void _initComponents();
	void _about();
	void _initializeWindowBase() const;

	void _checkConflictService();

	void _checkValidRootDirectory();

	void _removeApp();
	void _removeAppRun();

	void _checkWhitelist();

	// Update
	void _updateApp();
	void _checkAppUpdate();
	void _updateAppWindow();
	void _updateAppProgressWindowInfo();
};
