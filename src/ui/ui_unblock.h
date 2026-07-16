#pragma once
#include "ui_check_box.h"
#include "ui_button.h"
#include "ui_secondary_window.h"

class Ui;

class UiUnblock
{
	std::shared_ptr<Ui> _ui;

	CHECK_BOX(_show_console);
	CHECK_BOX(_testing_domains_startup);

	// Stop all services
	BUTTON(_stop_service_all);
	SECONDARY_WINDOW(_window_wait_stop_service);

public:
	explicit UiUnblock(std::shared_ptr<Ui> ui);

	void initialize();

	const Ptr<CheckBox>& getTestingDomainsStartup() const { return _testing_domains_startup; }
	const Ptr<SecondaryWindow>& getWindowWaitStopService() { return _window_wait_stop_service; }
	void stopAllServices() const;

private:
	void _showConsole();
	void _testDomainsStartup();
	void _stopService();
};
