#pragma once

#include "ui_secondary_window.h"
#include "ui_input.h"
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_select_list.h"

#include "ui_list_ul.h"

struct UI_API UiElements
{
protected:
	// Setting
	Ptr<File> _file_user_setting;

	// Setting Common
#ifndef DEBUG
	CHECK_BOX(_show_console);
#endif

	CHECK_BOX(_testing_domains_startup);
	CHECK_BOX(_accurate_testing);

	// Setting Unblock
	CHECK_BOX(_unblock_enable);
	CHECK_BOX(_unblock_manual);
	CHECK_BOX(_unblock_filtering_top_level_domains);
	SELECT_LIST(_unblock_select_config);
	SELECT_LIST(_unblock_select_fake_bin);

	// Setting Proxy
	CHECK_BOX(_proxy_enable);
	INPUT(_proxy_port);
	INPUT(_proxy_ip);
	
	CHECK_BOX(_proxy_manual);
	SELECT_LIST(_proxy_select_config);

	// Home
	BUTTON(_start_service);
	BUTTON(_start_proxy_service);
	BUTTON(_start_testing);
	BUTTON(_stop_service);

	UL_LIST(_list_domain);
	UL_LIST(_list_domain_video);
	UL_LIST(_list_proxy_domain);
	UL_LIST(_list_proxy_domain_video);

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
};
