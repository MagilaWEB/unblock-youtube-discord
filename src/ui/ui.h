#pragma once
#include "ui_secondary_window.h"
#include "ui_input.h"
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_select_list.h"

#include "ui_list_ul.h"

#include "../unblock/unblock.h"

class UiBase;
class Ui final : public utils::DefaultInit
{
	std::shared_ptr<UiBase> _ui_base{ nullptr };

	Unblock _unblock;

	// Setting
	std::shared_ptr<File> _file_service_list;

	std::atomic_bool _automatically_strategy_cancel{ false };
	bool			 _init{ false };

	// Setting Common
#ifndef DEBUG
	CHECK_BOX(_show_console);
#endif
	CHECK_BOX(_enable_check_update_startup);
	BUTTON(_start_check_update_app);
	CHECK_BOX(_testing_domains_startup);
	CHECK_BOX(_enable_dns_hosts);
	BUTTON(_start_update_dns_hosts);

	// Remove app
	BUTTON(_remove_app);
	SECONDARY_WINDOW(_window_remove_app);

	// Setting Unblock
	CHECK_BOX(_unblock_enable);
	CHECK_BOX(_unblock_manual);
	SELECT_LIST(_unblock_select_version_strategy);
	SELECT_LIST(_unblock_select_config);

	// Setting Unblock list enable services
	std::map<std::string, std::shared_ptr<CheckBox>> _unblock_list_enable_services{};

	// Home
	BUTTON(_start_unblock);
	BUTTON(_stop_unblock);
	BUTTON(_stop_service_all);
	BUTTON(_start_testing);
	BUTTON(_show_info_selected_service_setting);

	UL_LIST(_active_service);

	UL_LIST(_list_domain);
	UL_LIST(_list_domain_video);
	UL_LIST(_list_domain_to_modal);

	SECONDARY_WINDOW(_window_update_unblock);
	SECONDARY_WINDOW(_window_wait_update_unblock);
	SECONDARY_WINDOW(_window_error_update_unblock);
	SECONDARY_WINDOW(_window_wait_check_update_unblock);

	SECONDARY_WINDOW(_window_warning_conflict_service);

	SECONDARY_WINDOW(_window_warning_whitelist);
	SECONDARY_WINDOW(_window_wait_test_whitelist);
	SECONDARY_WINDOW(_window_warning_no_internet);

	SECONDARY_WINDOW(_window_info_selected_service_setting);
	SECONDARY_WINDOW(_window_wait_response_from_server);
	SECONDARY_WINDOW(_window_wait_start_service);
	SECONDARY_WINDOW(_window_wait_stop_service);
	SECONDARY_WINDOW(_window_wait_testing);
	SECONDARY_WINDOW(_window_wait_update_dns);

	SECONDARY_WINDOW(_window_config_not_found);
	SECONDARY_WINDOW(_window_config_found);
	SECONDARY_WINDOW(_window_auto_start_wait);
	SECONDARY_WINDOW(_window_continue_select_strategy);

	SECONDARY_WINDOW(_window_configuration_selection_error);

	SECONDARY_WINDOW(_window_to_warn_enable_dns_hosts);

	// footer
	BUTTON(_link_to_github);
	BUTTON(_link_to_telegram);

	enum StoppingService : u32
	{
		eUnblock  = (1 << 0),
	};

public:
	Ui(UiBase* ui_base);

	void initialize();

	void jsUpdate();

private:
	void _checkConflictService();

	void _removeApp();
	void _removeAppRun();

	void _initShowInfoSetting();

	void _checkWhitelist();

	// update
	void _updateApp();
	void _checkAppUpdate();
	void _updateAppWindow();
	void _updateAppProgressWindowInfo();

	// setting
	void _settingInit();
	void _settingShowConsole();
	void _settingTestDomainsStartup();
	void _settingEnableDnsHosts();
	void _settingEnableDnsHostsUpdate();
	void _settingEnableDnsHostsWarningUser();
	void _settingDnsHostsUpdateInfoWindow();

	void _settingUnblockEnable();

	void _settingUnblockListEnableServices();
	void _settingUnblockListEnableServicesUpdate();

	void _settingUnblockSelectStrategyVersion();
	void _settingUnblockSelectStrategyVersionUpdate();

	void _settingUnblockEnableManual();
	void _settingUnblockEnableManualUpdate();

	void _settingUnblockEnableManualSelect();
	void _settingUnblockEnableManualSelectUpdate();

	// Testing
	void _testingInit();
	void _testingUpdate();
	void _testingWindow();

	// Testing methods base
	void _activeServiceUpdate();
	void _testingServiceDomains();

	// Starting services
	void _startInit();
	void _startUnblock();

	// Starting services base methods
	void	   _startServiceWindow();
	void	   _clickStartService();
	void	   _autoStart();
	void	   _startServiceFromConfig();
	JSFunction _updateCountStartStopButtonToCss;

	// Starting services update button
	void _buttonUpdate();

	// Stopping services
	void _stopInit();
	void _stoppingServices(StoppingService type);
	// void _stopTorProxy();

	// base footer
	void _footerElements();

	void _tcpGlobalChange(bool state);
};
