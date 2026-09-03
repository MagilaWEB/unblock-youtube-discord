#pragma once
#include "ui_button.h"
#include "ui_check_box.h"
#include "ui_input.h"

class Ui;
class Unblock;

class UiProxyTg
{
private:
	std::shared_ptr<Ui>		 _ui;
	std::shared_ptr<Unblock> _unblock;

	CHECK_BOX(_proxy_tg_enable);
	BUTTON(_proxy_link_tg);

	INPUT(_proxy_tg_host);
	INPUT(_proxy_tg_port);
	INPUT(_proxy_tg_dc_ip_1);
	INPUT(_proxy_tg_dc_ip_2);
	INPUT(_proxy_tg_dc_ip_3);
	INPUT(_proxy_tg_dc_ip_4);
	INPUT(_proxy_tg_cfproxy_domain);
	BUTTON(_proxy_tg_apply);

public:
	UiProxyTg(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock);

	void initialize();

	const Ptr<CheckBox>& getCheckBoxProxyTg() const { return _proxy_tg_enable; }

private:
	void		_enableProxyTg();
	void		_enableProxyLinkTg();
	void		_proxySettings();
	void		_applyProxySettings();
	std::string _settingValue(std::string_view key, std::string_view default_value);
};
