#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_editable_list.h"
#include "ui_input.h"
#include "ui_select_list.h"
#include "ui_secondary_window.h"

class Ui;
class Unblock;

class UiDnsHosts
{
private:
	std::shared_ptr<Ui>		 _ui;
	std::shared_ptr<Unblock> _unblock;

	CHECK_BOX(_enable_dns_hosts);
	BUTTON(_start_update_dns_hosts);
	SELECT_LIST(_select_region);
	EDITABLE_LIST(_region_list);
	INPUT(_dns_hosts_url);

	SECONDARY_WINDOW(_window_to_warn_enable_dns_hosts);
	SECONDARY_WINDOW(_window_wait_update_dns);
	SECONDARY_WINDOW(_window_check_region);

public:
	UiDnsHosts(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock);

	void initialize();
	void updateInfoWindow();

private:
	void _enableDnsHosts();
	void _enableDnsHostsUpdate();
	void _enableDnsHostsWarningUser();
	void _updateDnsHosts();
	void _saveRegions();
	void _rebuildRegionSelect();
	void _onRegionsChanged(JSArgs args);
	void _checkRegionAvailability(std::string region);
	void _applyActiveRegion(const std::string& region);
	void _applyBaseUrl(const std::string& url);
};
