#pragma once
#include "ui_check_box.h"

class Ui;

class UiUnblock
{
	std::shared_ptr<Ui> _ui;

	CHECK_BOX(_show_console);
	CHECK_BOX(_testing_domains_startup);

public:
	explicit UiUnblock(std::shared_ptr<Ui> ui);

	void initialize();

	const Ptr<CheckBox>& getTestingDomainsStartup() const { return _testing_domains_startup; }

private:
	void _showConsole();
	void _testDomainsStartup();
};
