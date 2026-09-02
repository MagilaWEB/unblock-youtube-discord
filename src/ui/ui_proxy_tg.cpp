#include "ui_proxy_tg.h"

#include "ui.h"
#include "../unblock/unblock.h"

UiProxyTg::UiProxyTg(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock)
    : _ui(std::move(ui)), _unblock(std::move(unblock))
{
}

void UiProxyTg::initialize()
{
	_enableProxyTg();
	_enableProxyLinkTg();
	_proxySettings();
}

void UiProxyTg::_enableProxyTg()
{
	_proxy_tg_enable
		->create("#tg_ws_proxy section .common", "str_checkbox_enable_proxy_tg_title",
				 Localization::Str{"str_checkbox_enable_proxy_tg_description"});
	_proxy_tg_enable->addTutorialStep("str_tour_proxy_tg_title", "str_tour_proxy_tg_description", 12);
	_proxy_tg_enable->setState(_unblock->localProxyTgIsRun());
	_proxy_tg_enable->addEventClick(
		[this](JSArgs args)
		{
			Core::get().addTaskParallel(
				[this, args]
				{
					const bool state = JSToCPP<bool>(args[0]);
					if (state)
						_ui->getWindowWaitStartService()->show();
					else
						_ui->getUiUnblock()->getWindowWaitStopService()->show();

					_unblock->localProxyTg(state);

					if (state)
						_ui->getWindowWaitStartService()->hide();
					else
						_ui->getUiUnblock()->getWindowWaitStopService()->hide();
				}
			);

			return false;
		}
	);
}

void UiProxyTg::_enableProxyLinkTg()
{
	_proxy_link_tg->create("#tg_ws_proxy section .common", "str_button_proxy_link_tg_title");
	_proxy_link_tg->addTutorialStep("str_tour_proxy_link_title", "str_tour_proxy_link_description", 13);

	_proxy_link_tg->addEventClick(
		[this](JSArgs)
		{
			_unblock->localProxyTgLinkRun();
			return false;
		}
	);
}

std::string UiProxyTg::_settingValue(std::string_view key, std::string_view default_value)
{
	if (auto result = _ui->userConfig()->parameterSection<std::string>("TG_WS_PROXY", std::string{ key }))
		return result.value();

	return std::string{ default_value };
}

void UiProxyTg::_proxySettings()
{
	auto host	 = _settingValue("host", _unblock->tgProxyHost());
	auto port	 = _settingValue("port", _unblock->tgProxyPort());
	auto cfproxy = _settingValue("cfproxy_worker_domain", _unblock->tgProxyCfproxyDomain());
	auto dc_ip	 = _unblock->tgProxyDcIp();

	std::array<std::string, 4> dc_settings;
	for (u32 i = 0; i < 4; i++)
		dc_settings[i] = _settingValue(utils::format("dc_ip_{}", i + 1), dc_ip[i]);

	_proxy_tg_host->create("#tg_ws_proxy section .common", Input::Types::ip, JSValue{ host.c_str() }, Localization::Str{"str_proxy_tg_host_title"}, Localization::Str{"str_proxy_tg_host_description"});
	_proxy_tg_port->create("#tg_ws_proxy section .common", Input::Types::port, JSValue{ port.c_str() }, Localization::Str{"str_proxy_tg_port_title"}, Localization::Str{"str_proxy_tg_port_description"});
	_proxy_tg_dc_ip_1->create("#tg_ws_proxy section .common", Input::Types::ip, JSValue{ dc_settings[0].c_str() }, Localization::Str{"str_proxy_tg_dc_ip_1_title"}, Localization::Str{"str_proxy_tg_dc_ip_1_description"});
	_proxy_tg_dc_ip_2->create("#tg_ws_proxy section .common", Input::Types::ip, JSValue{ dc_settings[1].c_str() }, Localization::Str{"str_proxy_tg_dc_ip_2_title"}, Localization::Str{"str_proxy_tg_dc_ip_2_description"});
	_proxy_tg_dc_ip_3->create("#tg_ws_proxy section .common", Input::Types::ip, JSValue{ dc_settings[2].c_str() }, Localization::Str{"str_proxy_tg_dc_ip_3_title"}, Localization::Str{"str_proxy_tg_dc_ip_3_description"});
	_proxy_tg_dc_ip_4->create("#tg_ws_proxy section .common", Input::Types::ip, JSValue{ dc_settings[3].c_str() }, Localization::Str{"str_proxy_tg_dc_ip_4_title"}, Localization::Str{"str_proxy_tg_dc_ip_4_description"});
	_proxy_tg_cfproxy_domain->create("#tg_ws_proxy section .common", Input::Types::text, JSValue{ cfproxy.c_str() }, Localization::Str{"str_proxy_tg_cfproxy_domain_title"}, Localization::Str{"str_proxy_tg_cfproxy_domain_description"});

	_unblock->setTgProxyParams(host, port, dc_settings, cfproxy);

	_proxy_tg_apply->create("#tg_ws_proxy section .common", "str_b_proxy_tg_apply");
	_proxy_tg_apply->addTutorialStep("str_tour_proxy_apply_title", "str_tour_proxy_apply_description", 14);
	_proxy_tg_apply->addEventClick(
		[this](JSArgs)
		{
			_applyProxySettings();
			return false;
		}
	);
}

void UiProxyTg::_applyProxySettings()
{
	Core::get().addTask(
		[this]
		{
			_ui->getUiUnblock()->getWindowWaitStopService()->show();
			_unblock->localProxyTg(false);
			_ui->getUiUnblock()->getWindowWaitStopService()->hide();

			const auto host	 = JSToCPP<std::string>(_proxy_tg_host->getValue());
			const auto port	 = JSToCPP<std::string>(_proxy_tg_port->getValue());
			const auto cfproxy = JSToCPP<std::string>(_proxy_tg_cfproxy_domain->getValue());
			std::array<std::string, 4> dc_ip{
				JSToCPP<std::string>(_proxy_tg_dc_ip_1->getValue()),
				JSToCPP<std::string>(_proxy_tg_dc_ip_2->getValue()),
				JSToCPP<std::string>(_proxy_tg_dc_ip_3->getValue()),
				JSToCPP<std::string>(_proxy_tg_dc_ip_4->getValue())
			};

			_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", "host", host);
			_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", "port", port);
			_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", "cfproxy_worker_domain", cfproxy);
			for (u32 i = 0; i < 4; i++)
				_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", utils::format("dc_ip_{}", i + 1), dc_ip[i]);

			_unblock->setTgProxyParams(host, port, dc_ip, cfproxy);

			_ui->getWindowWaitStartService()->show();
			_unblock->localProxyTg(true);
			_ui->getWindowWaitStartService()->hide();
		}
	);
}
