#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_editable_list.h"
#include "ui_input.h"
#include "ui_secondary_window.h"
#include "ui_status.h"

class Ui;
class DnsProxy;

class UiDnsProxy
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
