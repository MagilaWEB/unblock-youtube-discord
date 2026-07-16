#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_secondary_window.h"

class Ui;
class Unblock;

class UiDnsHosts
{
private:
	std::shared_ptr<Ui>		_ui;
	std::shared_ptr<Unblock> _unblock;

	CHECK_BOX(_enable_dns_hosts);
	BUTTON(_start_update_dns_hosts);

	SECONDARY_WINDOW(_window_to_warn_enable_dns_hosts);
	SECONDARY_WINDOW(_window_wait_update_dns);
	SECONDARY_WINDOW(_window_wait_response_from_server);

public:
	UiDnsHosts(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock);

	void initialize();
	void updateInfoWindow();

private:
	void _enableDnsHosts();
	void _enableDnsHostsUpdate();
	void _enableDnsHostsWarningUser();
};
