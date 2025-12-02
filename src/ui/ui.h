#pragma once
#include "ui_secondary_window.h"
#include "ui_input.h"
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_select_list.h"

#include "ui_list_ul.h"

#include "../unblock/unblock.h"

class UiBase;
class Ui final
{
	std::shared_ptr<UiBase> _ui_base{ nullptr };

	Ptr<Unblock> _unblock;

	// Setting
	Ptr<File> _file_service_list;

	std::atomic_bool _automatically_strategy_cancel{ false };
	std::atomic_bool _proxy_click_state{ false };

	// Setting Common
#ifndef DEBUG
	CHECK_BOX(_show_console);
#endif

	CHECK_BOX(_testing_domains_startup);
	CHECK_BOX(_accurate_testing);
	INPUT(_max_time_wait_testing);
	INPUT(_max_time_wait_accurate_testing);

	// Setting Unblock
	CHECK_BOX(_unblock_enable);
	CHECK_BOX(_unblock_manual);
	CHECK_BOX(_unblock_filtering_top_level_domains);
	SELECT_LIST(_unblock_select_config);
	SELECT_LIST(_unblock_select_fake_bin);

	// Setting Unblock list enable services
	std::map<std::string, std::shared_ptr<CheckBox>> _unblock_list_enable_services{};

	// Setting Proxy
	CHECK_BOX(_proxy_enable);
	INPUT(_proxy_port);
	INPUT(_proxy_ip);
	CHECK_BOX(_proxy_manual);
	SELECT_LIST(_proxy_select_config);

	// Setting Proxy TOR
	//CHECK_BOX(_tor_proxy_enable);

	// Home
	BUTTON(_start_unblock);
	BUTTON(_stop_unblock);
	BUTTON(_start_proxy_dpi);
	BUTTON(_stop_proxy_dpi);
	BUTTON(_stop_service_all);
	BUTTON(_start_testing);

	UL_LIST(_active_service);

	UL_LIST(_list_domain);
	UL_LIST(_list_domain_video);
	UL_LIST(_list_proxy_domain);
	UL_LIST(_list_proxy_domain_video);

	SECONDARY_WINDOW(_window_warning_conflict_service);

	SECONDARY_WINDOW(_window_wait_start_service);
	SECONDARY_WINDOW(_window_wait_stop_service);
	SECONDARY_WINDOW(_window_wait_testing);

	SECONDARY_WINDOW(_window_config_not_found);
	SECONDARY_WINDOW(_window_config_found);
	SECONDARY_WINDOW(_window_auto_start_wait);
	SECONDARY_WINDOW(_window_continue_select_strategy);

	SECONDARY_WINDOW(_window_configuration_selection_error);

	// footer
	BUTTON(_link_to_github);

	enum StoppingService : u32
	{
		eUnblock = (1 << 0),
		eProxyDpi = (1 << 1)
	};

public:
	Ui() = delete;
	Ui(UiBase* ui_base);
	Ui(const Ui&)			 = default;
	Ui(Ui&&)				 = default;
	Ui& operator=(const Ui&) = default;
	Ui& operator=(Ui&&)		 = default;
	virtual ~Ui()			 = default;

	void initialize();

private:
	void _checkConflictService();

	void _setting();

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
	void _startProxy();
	//void _startTorProxy();

	// Starting services base methods 
	void _startServiceWindow();
	void _clickStartService();
	void _autoStart();
	void _startServiceFromConfig();
	JSFunction _updateCountStartStopButtonToCss;

	// Starting services update button
	void _buttonUpdate();

	// Stopping services
	void _stopInit();
	void _stoppingButtonUpdate();
	void _stoppingServices(StoppingService type);
	//void _stopTorProxy();

	// base footer
	void _footerElements();
};
