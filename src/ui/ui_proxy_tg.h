#pragma once
#include "ui_button.h"
#include "ui_check_box.h"

class Ui;
class Unblock;

class UiProxyTg
{
private:
	std::shared_ptr<Ui>		_ui;
	std::shared_ptr<Unblock> _unblock;

	CHECK_BOX(_proxy_tg_enable);
	BUTTON(_proxy_link_tg);

public:
	UiProxyTg(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock);

	void initialize();

	const Ptr<CheckBox>& getCheckBoxProxyTg() const { return _proxy_tg_enable; }

private:
	void _enableProxyTg();
	void _enableProxyLinkTg();
};
