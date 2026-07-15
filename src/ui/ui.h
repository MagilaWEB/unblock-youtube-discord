#pragma once
#include "ui_secondary_window.h"
#include "ui_button.h"
#include "ui_check_box.h"
#include "../unblock/unblock.h"
#include "ui_dns_hosts.h"
#include "ui_proxy_tg.h"
#include "ui_zapret.h"

class UiBase;
class UiZapret2;
class Ui final : public utils::DefaultInit, public std::enable_shared_from_this<Ui>
{
	std::shared_ptr<UiBase> _ui_base;

	std::shared_ptr<Unblock> _unblock;

	// Setting
	std::shared_ptr<File> _file_service_list;

	bool _init{ false };

	// Setting Common
#ifndef DEBUG
	CHECK_BOX(_show_console);
#endif

	// Setting update unblock app
	CHECK_BOX(_enable_check_update_startup);
	BUTTON(_start_check_update_app);

	// Setting Whitelist
	CHECK_BOX(_testing_domains_startup);

	// Remove app
	BUTTON(_remove_app);
	SECONDARY_WINDOW(_window_remove_app);

	std::unique_ptr<UiDnsHosts> _ui_dns_hosts;
	std::unique_ptr<UiProxyTg> _ui_proxy_tg;
	std::unique_ptr<UiZapret2> _ui_zapret2;

	// Home
	BUTTON(_stop_zapret);
	BUTTON(_run_auto_config_zapret);

	// Service all stoping
	BUTTON(_stop_service_all);

	// Root directory error
	SECONDARY_WINDOW(_window_root_directory_error);

	// Update unblock app
	SECONDARY_WINDOW(_window_update_unblock);
	SECONDARY_WINDOW(_window_wait_update_unblock);
	SECONDARY_WINDOW(_window_error_update_unblock);
	SECONDARY_WINDOW(_window_wait_check_update_unblock);

	SECONDARY_WINDOW(_window_warning_conflict_service);

	SECONDARY_WINDOW(_window_warning_whitelist);
	SECONDARY_WINDOW(_window_wait_test_whitelist);
	SECONDARY_WINDOW(_window_warning_no_internet);
	SECONDARY_WINDOW(_window_wait_start_service);
	SECONDARY_WINDOW(_window_wait_stop_service);

	// footer
	BUTTON(_link_to_github);
	BUTTON(_link_to_telegram);

public:
	std::shared_ptr<Ui> self;

	explicit Ui(std::shared_ptr<UiBase> ui_base);

	void initialize();
	void postConstruct();

	void jsUpdate();

	// Этот метод
	const Ptr<SecondaryWindow>& getWindowWaitStartService() { return _window_wait_start_service; }
	const Ptr<SecondaryWindow>& getWindowWaitStopService() { return _window_wait_stop_service; }

	const Ptr<CheckBox>& getTestingDomainsStartup() const { return _testing_domains_startup; }

	std::shared_ptr<UiBase>& uiBase() { return _ui_base; }

private:
	void _initializeAppState();
	void _initializeSettings();
	void _initializeHome();
	void _initializeFooter();
	void _initializeWindowBase() const;

	void _checkConflictService();

	void _checkValidRootDirectory();

	void _removeApp();
	void _removeAppRun();

	void _checkWhitelist();

	// update
	void _updateApp();
	void _checkAppUpdate();
	void _updateAppWindow();
	void _updateAppProgressWindowInfo();

	// setting init and show info setting
	void _settingInit();
	void _settingShowConsole();
	void _settingTestDomainsStartup();
	// Stopping services methods
	void _stopInit();
	void _stoppingServices();
	void _stoppingAllServices();

	// base footer 
	void _footerElements();
};
