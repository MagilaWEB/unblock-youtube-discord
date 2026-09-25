#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_editable_list.h"
#include "ui_input.h"
#include "ui_secondary_window.h"
#include "ui_status.h"

class Ui;
class Unblock;

class UiDnsProxy
{
private:
	std::shared_ptr<Ui>		 _ui;
	std::shared_ptr<Unblock> _unblock;

	CHECK_BOX(_enable_dns_proxy);
	STATUS(_status_dns);
	CHECK_BOX(_upstream_cf);
	CHECK_BOX(_upstream_google);
	CHECK_BOX(_upstream_quad9);
	EDITABLE_LIST(_custom_upstreams);
	INPUT(_test_input);
	BUTTON(_test_button);

	SECONDARY_WINDOW(_window_test_result);

	std::string _last_status;

public:
	UiDnsProxy(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock);

	void initialize();
	void updateInfoWindow();

private:
	void _collectUpstreams();
	void _applyUpstreams();
	void _refreshStatus();
};
