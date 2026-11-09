module ui;

import std;
import core;
import core.localization;
import core.service_diagnostics;
import core.utils;
import dom.utils_saucer;
import ui.widget_button;
import ui.widget_check_box;
import ui.widget_input;
import unblock.tg_proxy;

UiProxyTg::UiProxyTg(std::shared_ptr<Ui> ui, TgProxy& tg_proxy) : _ui(std::move(ui)), _tg_proxy(tg_proxy)
{
}

void UiProxyTg::initialize()
{
	_enableProxyTg();

	// The status sits right under the enable checkbox (same order as on the DNS
	// page): controls are appended to the block in creation order, so creating
	// it later would push it to the bottom of the page.
	_status_tg->create("#tg_ws_proxy section .common");
	_status_tg->setInactive(Localization::Str{ "str_status_tg_stopped" }());
	_status_tg->addEventClick(
		[this](JSArgs)
		{
			_ui->showServiceReport();
			return false;
		}
	);

	_enableProxyLinkTg();
	_proxySettings();

	updateStatus();
}

void UiProxyTg::updateStatus()
{
	if (!_status_tg->isCreate())
		return;

	const bool running	 = _tg_proxy.isRun();
	const bool issue	 = _ui->unblock()->serviceReportCovers("TgWsProxy");
	const u64  report_id = issue ? _ui->unblock()->lastServiceReportId() : 0;

	if (running == _last_running && report_id == _last_report_id)
		return;

	_last_running	= running;
	_last_report_id = report_id;

	std::string text;
	if (issue)
		text = utils::format(Localization::Str{ "str_status_tg_issue" }(), _ui->unblock()->lastServiceReport().verdict);
	else
		text = running ? Localization::Str{ "str_status_tg_running" }() : Localization::Str{ "str_status_tg_stopped" }();

	if (issue)
		_status_tg->setWarning(text);
	else if (running)
		_status_tg->setActive(text);
	else
		_status_tg->setInactive(text);
}

void UiProxyTg::_enableProxyTg()
{
	_proxy_tg_enable->create(
		"#tg_ws_proxy section .common",
		"str_checkbox_enable_proxy_tg_title",
		Localization::Str{ "str_checkbox_enable_proxy_tg_description" }
	);
	_proxy_tg_enable->addTutorialStep("str_tour_proxy_tg_title", "str_tour_proxy_tg_description", 22);
	_proxy_tg_enable->setState(_tg_proxy.isRun());
	_proxy_tg_enable->addEventClick(
		[this](JSArgs args)
		{
			Core::get().addTask(
				[this, args]
				{
					const bool state = jsToCpp<bool>(args[0]);
					if (state)
						_ui->getWindowWaitStartService()->show();
					else
						_ui->getUiUnblock()->getWindowWaitStopService()->show();

					_ui->unblock()->runTgProxy(state);

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
	_proxy_link_tg->addTutorialStep("str_tour_proxy_link_title", "str_tour_proxy_link_description", 23);

	_proxy_link_tg->addEventClick(
		[this](JSArgs)
		{
			_tg_proxy.linkRun();
			return false;
		}
	);

	// Fallback for setups where the tg:// handoff does not open Telegram
	// (for example a Store build, or no registered handler): copy the link so
	// the user can paste it into Telegram's Saved Messages and tap it.
	_proxy_link_tg_copy->create("#tg_ws_proxy section .common", "str_button_proxy_copy_link_tg_title");
	_proxy_link_tg_copy->addTutorialStep("str_tour_proxy_copy_title", "str_tour_proxy_copy_description", 24);
	_proxy_link_tg_copy->addEventClick(
		[this](JSArgs)
		{
			_tg_proxy.copyLink();
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
	auto host	 = _settingValue("host", _tg_proxy.host());
	auto port	 = _settingValue("port", _tg_proxy.port());
	auto cfproxy = _settingValue("cfproxy_worker_domain", _tg_proxy.cfproxyDomain());
	auto dc_ip	 = _tg_proxy.dcIp();

	std::array<std::string, 4> dc_settings;
	for (const auto& [i, ip] : std::views::enumerate(dc_ip))
		dc_settings[i] = _settingValue(utils::format("dc_ip_{}", i + 1), ip);

	_proxy_tg_host->create(
		"#tg_ws_proxy section .common",
		Input::Types::ip,
		JSValue{ host.c_str() },
		Localization::Str{ "str_proxy_tg_host_title" },
		Localization::Str{ "str_proxy_tg_host_description" }
	);
	_proxy_tg_host->addTutorialStep("str_tour_proxy_settings_title", "str_tour_proxy_settings_description", 25);
	_proxy_tg_port->create(
		"#tg_ws_proxy section .common",
		Input::Types::port,
		JSValue{ port.c_str() },
		Localization::Str{ "str_proxy_tg_port_title" },
		Localization::Str{ "str_proxy_tg_port_description" }
	);
	_proxy_tg_dc_ip_1->create(
		"#tg_ws_proxy section .common",
		Input::Types::ip,
		JSValue{ dc_settings[0].c_str() },
		Localization::Str{ "str_proxy_tg_dc_ip_1_title" },
		Localization::Str{ "str_proxy_tg_dc_ip_1_description" }
	);
	_proxy_tg_dc_ip_2->create(
		"#tg_ws_proxy section .common",
		Input::Types::ip,
		JSValue{ dc_settings[1].c_str() },
		Localization::Str{ "str_proxy_tg_dc_ip_2_title" },
		Localization::Str{ "str_proxy_tg_dc_ip_2_description" }
	);
	_proxy_tg_dc_ip_3->create(
		"#tg_ws_proxy section .common",
		Input::Types::ip,
		JSValue{ dc_settings[2].c_str() },
		Localization::Str{ "str_proxy_tg_dc_ip_3_title" },
		Localization::Str{ "str_proxy_tg_dc_ip_3_description" }
	);
	_proxy_tg_dc_ip_4->create(
		"#tg_ws_proxy section .common",
		Input::Types::ip,
		JSValue{ dc_settings[3].c_str() },
		Localization::Str{ "str_proxy_tg_dc_ip_4_title" },
		Localization::Str{ "str_proxy_tg_dc_ip_4_description" }
	);
	/*_proxy_tg_cfproxy_domain->create(
		"#tg_ws_proxy section .common",
		Input::Types::text,
		JSValue{ cfproxy.c_str() },
		Localization::Str{ "str_proxy_tg_cfproxy_domain_title" },
		Localization::Str{ "str_proxy_tg_cfproxy_domain_description" }
	);*/

	_tg_proxy.setParams(host, port, dc_settings, cfproxy);

	_proxy_tg_apply->create("#tg_ws_proxy section .common", "str_b_proxy_tg_apply");
	_proxy_tg_apply->addTutorialStep("str_tour_proxy_apply_title", "str_tour_proxy_apply_description", 26);
	_proxy_tg_apply->addEventClick(
		[this](JSArgs)
		{
			_applyProxySettings();
			return false;
		}
	);

	// Reset the proxy settings to the built-in defaults. Like the DNS page,
	// the enable switch is not touched, and the service is restarted only when
	// it is already running (otherwise the reset would silently start it).
	_reset_tg->create("#tg_ws_proxy section .common", "str_button_reset_defaults_title");
	_reset_tg->addEventClick(
		[this](JSArgs)
		{
			_proxy_tg_host->setValue(JSValue{ TgProxy::defaultHost() });
			_proxy_tg_port->setValue(JSValue{ TgProxy::defaultPort() });
			const auto dc_ip = TgProxy::defaultDcIp();
			_proxy_tg_dc_ip_1->setValue(JSValue{ dc_ip[0] });
			_proxy_tg_dc_ip_2->setValue(JSValue{ dc_ip[1] });
			_proxy_tg_dc_ip_3->setValue(JSValue{ dc_ip[2] });
			_proxy_tg_dc_ip_4->setValue(JSValue{ dc_ip[3] });

			Core::get().addTask(
				[this]
				{
					const bool running = _tg_proxy.isRun();
					if (running)
					{
						_ui->getUiUnblock()->getWindowWaitStopService()->show();
						_ui->unblock()->runTgProxy(false);
						_ui->getUiUnblock()->getWindowWaitStopService()->hide();
					}

					_saveProxySettings();

					if (running)
					{
						_ui->getWindowWaitStartService()->show();
						_ui->unblock()->runTgProxy(true);
						_ui->getWindowWaitStartService()->hide();
					}
				}
			);
			return false;
		}
	);
}

void UiProxyTg::_saveProxySettings()
{
	const auto				   host = jsToCpp<std::string>(_proxy_tg_host->getValue());
	const auto				   port = jsToCpp<std::string>(_proxy_tg_port->getValue());
	std::array<std::string, 4> dc_ip{ jsToCpp<std::string>(_proxy_tg_dc_ip_1->getValue()),
									  jsToCpp<std::string>(_proxy_tg_dc_ip_2->getValue()),
									  jsToCpp<std::string>(_proxy_tg_dc_ip_3->getValue()),
									  jsToCpp<std::string>(_proxy_tg_dc_ip_4->getValue()) };

	_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", "host", host);
	_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", "port", port);
	for (const auto& [i, ip] : std::views::enumerate(dc_ip))
		_ui->userConfig()->writeSectionParameter("TG_WS_PROXY", utils::format("dc_ip_{}", i + 1), ip);

	_tg_proxy.setParams(host, port, dc_ip, "");	   // cfproxy
}

void UiProxyTg::_applyProxySettings()
{
	Core::get().addTask(
		[this]
		{
			_ui->getUiUnblock()->getWindowWaitStopService()->show();
			_ui->unblock()->runTgProxy(false);
			_ui->getUiUnblock()->getWindowWaitStopService()->hide();

			_saveProxySettings();

			_ui->getWindowWaitStartService()->show();
			_ui->unblock()->runTgProxy(true);
			_ui->getWindowWaitStartService()->hide();
		}
	);
}
