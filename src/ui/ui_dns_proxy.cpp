module;

#include "../core/macros.hpp"

module ui;

import std;
import common.dns_config;
import core;
import core.debug;
import core.localization;
import core.service_diagnostics;
import core.timer;
import core.utils;
import dom.utils_saucer;
import ui.widget_background_tasks;
import ui.widget_button;
import ui.widget_check_box;
import ui.widget_editable_list;
import ui.widget_input;
import ui.widget_secondary_window;
import ui.widget_status;
import unblock.dns_proxy;

UiDnsProxy::UiDnsProxy(std::shared_ptr<Ui> ui, DnsProxy& dns_proxy) : _ui(std::move(ui)), _dns_proxy(dns_proxy)
{
}

void UiDnsProxy::initialize()
{
	_window_test_result->create(Localization::Str{ "str_window_test_upstream_title" }, "");
	_window_test_result->setType(SecondaryWindow::Type::OK);
	_window_test_result->addEventOk(
		[this](JSArgs)
		{
			_window_test_result->hide();
			return false;
		}
	);

	// First-run consent: the app asks once before it starts touching the system
	// DNS. The answer is remembered by its own key, kept apart from the live
	// state, which is never persisted.
	_window_enable_dns_proxy->create(Localization::Str{ "str_warning" }, "str_window_enable_dns_proxy_description");
	_window_enable_dns_proxy->setType(SecondaryWindow::Type::YesNo);
	_window_enable_dns_proxy->addEventYesNo(
		[this](JSArgs args)
		{
			_window_enable_dns_proxy->hide();
			_ui->userConfig()->writeSectionParameter("DNS", "consent", "true");
			setEnabled(jsToCpp<bool>(args[0]));
			return false;
		}
	);

	_enable_dns_proxy
		->create("#dns section .common", "str_checkbox_enable_dns_proxy_title", Localization::Str{ "str_checkbox_enable_dns_proxy_description" });
	_enable_dns_proxy->addTutorialStep("str_tour_dns_proxy_title", "str_tour_dns_proxy_description", 17);
	_enable_dns_proxy->addEventClick(
		[this](JSArgs args)
		{
			setEnabled(jsToCpp<bool>(args[0]));
			return false;
		}
	);

	_status_dns->create("#dns section .common");
	_status_dns->setInactive(Localization::Str{ "str_status_dns_stopped" }());
	_status_dns->addEventClick(
		[this](JSArgs)
		{
			_ui->showServiceReport();
			return false;
		}
	);

	// Every upstream lives in one editable list as a clean address; the
	// built-in presets are only the default entries. The first one is the
	// primary resolver, the rest are fallbacks. The bootstrap list right
	// below is shared and applied automatically to hostname upstreams.
	_upstreams->create(
		"#dns section .common",
		Localization::Str{ "str_dns_proxy_servers_title" },
		Localization::Str{ "str_dns_proxy_servers_description" }(),
		Localization::Str{ "str_input_dns_proxy_custom_placeholder" }()
	);
	_upstreams->addTutorialStep("str_tour_dns_servers_title", "str_tour_dns_servers_description", 18);
	_upstreams->setValidator([](const std::string& value) { return isValidUpstreamAddress(trimConfigLine(value)); });
	_upstreams->addEventChange(
		[this](JSArgs)
		{
			_collectUpstreams();
			_applyUpstreams();
			return false;
		}
	);

	// Shared bootstrap: plain DNS used only to resolve the hostname of
	// DoH/DoT upstreams. Editable in case the ISP blocks the defaults.
	std::string				 bootstrap_csv;
	std::vector<std::string> bootstrap = DnsProxy::defaultBootstrap();
	if (auto cfg = _ui->userConfig()->parameterSection<std::string>("DNS", "bootstrap"))
		if (auto parsed = parseBootstrapList(trimConfigLine(cfg.value())))
			if (!parsed->empty())
				bootstrap = std::move(*parsed);
	for (const auto& b : bootstrap)
		bootstrap_csv += (bootstrap_csv.empty() ? "" : ",") + b;
	_dns_proxy.setBootstrap(bootstrap);

	// The hint cites the shipped defaults (configs/dns_proxy.config), not the
	// user's override; Localization::Str fills the {} of the text with them.
	const std::string default_bootstrap_csv = DnsProxy::defaultBootstrap() | std::views::join_with(',') | std::ranges::to<std::string>();

	_bootstrap->create(
		"#dns section .common",
		Input::Types::text,
		JSValue{ bootstrap_csv },
		Localization::Str{ "str_dns_proxy_bootstrap_title" },
		Localization::Str{ "str_dns_proxy_bootstrap_description", default_bootstrap_csv }
	);
	_bootstrap->addTutorialStep("str_tour_dns_bootstrap_title", "str_tour_dns_bootstrap_description", 19);
	_bootstrap->setValidator([](const std::string& value) { return parseBootstrapList(trimConfigLine(value)).has_value(); });
	_bootstrap->addEventSubmit(
		[this](JSArgs args)
		{
			if (auto parsed = parseBootstrapList(trimConfigLine(jsToCpp<std::string>(args[0]))))
			{
				_dns_proxy.setBootstrap(std::move(*parsed));
				_collectUpstreams();
				_applyUpstreams();
			}
			return false;
		}
	);

	// Upstream exchange timeout (seconds). Private resolvers stall now and
	// then, so it is tunable without rebuilding.
	std::uint32_t timeout_sec = DnsProxy::defaultTimeout() / 1'000;
	if (const auto cfg = _ui->userConfig()->parameterSection<std::uint32_t>("DNS", "timeout"); cfg && *cfg >= 1 && *cfg <= 120)
		timeout_sec = *cfg;
	_dns_proxy.setTimeout(timeout_sec * 1'000);

	_timeout->create(
		"#dns section .common",
		Input::Types::duration_sec,
		JSValue{ static_cast<int>(timeout_sec) },
		Localization::Str{ "str_dns_proxy_timeout_title" },
		Localization::Str{ "str_dns_proxy_timeout_description" },
		Input::Options{ 1, 120, "sec" }
	);
	_timeout->addTutorialStep("str_tour_dns_timeout_title", "str_tour_dns_timeout_description", 20);
	_timeout->addEventSubmit(
		[this](JSArgs args)
		{
			const std::string text = trimConfigLine(jsToCpp<std::string>(args[0]));

			if (int seconds = 0; std::from_chars(text.data(), text.data() + text.size(), seconds).ec == std::errc{})
			{
				_dns_proxy.setTimeout(static_cast<std::uint32_t>(std::clamp(seconds, 1, 120)) * 1'000);
				_collectUpstreams();
				_applyUpstreams();
			}
			return false;
		}
	);

	_test_input->create(
		"#dns section .common",
		Input::Types::text,
		JSValue{ "" },
		Localization::Str{ "str_input_test_upstream_title" },
		Localization::Str{ "str_input_test_upstream_description" }
	);
	// Inline red flag while the typed server line does not parse.
	_test_input->setValidator([](const std::string& value) { return isValidUpstreamAddress(trimConfigLine(value)); });

	_test_button->create("#dns section .common", "str_button_test_upstream_title");
	_test_button->addTutorialStep("str_tour_dns_test_title", "str_tour_dns_test_description", 21);
	_test_button->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask(
				[this]
				{
					// Blocking DOM getter: background task only, never the UI thread.
					const auto value = jsToCpp<std::string>(_test_input->getValue());
					// Empty or invalid — the field already says so, no window.
					if (value.empty() || !isValidUpstreamAddress(trimConfigLine(value)))
						return;

					_ui->backgroundTasks()->start("dns_proxy_test", "str_task_dns_proxy_test_title");

					std::string output;
					_dns_proxy.testUpstream(value, output);

					_ui->backgroundTasks()->finish("dns_proxy_test");

					_window_test_result->setDescription(Localization::Str{ output.empty() ? std::string{ "—" } : output });
					_window_test_result->show();
				}
			);
			return false;
		}
	);

	_reset_dns->create("#dns section .common", "str_button_reset_defaults_title");
	_reset_dns->addEventClick(
		[this](JSArgs)
		{
			// Reset the three DNS parameters to their built-in defaults: seed
			// the proxy, refresh the widgets, persist and apply. The enable
			// switch is left alone — the user asked to reset the settings,
			// not to turn the feature off.
			const auto upstreams = DnsProxy::defaultUpstreams();
			_dns_proxy.setUpstreams(upstreams);
			_dns_proxy.setBootstrap(DnsProxy::defaultBootstrap());
			_dns_proxy.setTimeout(DnsProxy::defaultTimeout());

			_upstreams->setItems(upstreams);

			std::string bootstrap_csv;
			for (const auto& b : DnsProxy::defaultBootstrap())
				bootstrap_csv += (bootstrap_csv.empty() ? "" : ",") + b;
			_bootstrap->setValue(JSValue{ bootstrap_csv });
			_timeout->setValue(JSValue{ static_cast<int>(DnsProxy::defaultTimeout() / 1'000) });

			_collectUpstreams();
			_applyUpstreams();
			return false;
		}
	);

	// Restore persisted servers, otherwise the built-in presets.
	std::vector<std::string> items;
	if (auto cfg = _ui->userConfig()->parameterSectionVector("DNS", "upstreams"))
		for (auto& line : cfg.value())
		{
			const std::string item = trimConfigLine(line);
			if (isValidUpstreamAddress(item))
				items.push_back(item);
		}

	if (items.empty())
		items = DnsProxy::defaultUpstreams();

	_upstreams->setItems(std::move(items));

	// Seed the proxy and persist the list.
	_collectUpstreams();

	// The checkbox is the live service state, never a stored preference: the
	// proxy is a system service that outlives the app, so whatever it is doing
	// now is the truth. A launch only mirrors it and never starts or stops it.
	_enable_dns_proxy->setState(_dns_proxy.isRun());
	_refreshStatus();

	// The very first decision is a one-time consent, remembered separately.
	if (!_ui->userConfig()->parameterSection<bool>("DNS", "consent").value_or(false))
	{
		// Older builds kept the decision under "enable": carry it over so an
		// upgrade does not ask again.
		if (_ui->userConfig()->parameterSection<bool>("DNS", "enable"))
			_ui->userConfig()->writeSectionParameter("DNS", "consent", "true");
		else
			_window_enable_dns_proxy->show();
	}
}

void UiDnsProxy::setEnabled(bool state)
{
	_enable_dns_proxy->setState(state);
	_applyState(state);
}

void UiDnsProxy::_applyState(bool state)
{
	_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
	_apply_pending = true;
	Core::get().addTask(
		[this, state]
		{
			_ui->unblock()->setDnsEnabled(state);
			_apply_pending = false;
			_ui->backgroundTasks()->finish("dns_proxy_apply");
		}
	);
}

void UiDnsProxy::updateInfoWindow()
{
	LIMIT_UPDATE(Description, 2.F, { _refreshStatus(); })
}

void UiDnsProxy::_collectUpstreams()
{
	std::vector<std::string> upstreams;

	for (auto& item : _upstreams->items())
	{
		const std::string address = trimConfigLine(item);
		if (isValidUpstreamAddress(address))
			upstreams.push_back(address);
	}

	_dns_proxy.setUpstreams(upstreams);
	_ui->userConfig()->writeSectionParameterVector("DNS", "upstreams", upstreams);

	// Persist the shared bootstrap alongside the servers.
	std::string bootstrap_csv;
	for (const auto& b : _dns_proxy.bootstrap())
		bootstrap_csv += (bootstrap_csv.empty() ? "" : ",") + b;
	_ui->userConfig()->writeSectionParameter("DNS", "bootstrap", bootstrap_csv);

	_ui->userConfig()->writeSectionParameter("DNS", "timeout", std::to_string(_dns_proxy.timeout() / 1'000));
}

void UiDnsProxy::_applyUpstreams()
{
	if (!_dns_proxy.isRun())
		return;

	_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
	_apply_pending = true;
	Core::get().addTask(
		[this]
		{
			_ui->unblock()->runDnsProxy(true);
			_apply_pending = false;
			_ui->backgroundTasks()->finish("dns_proxy_apply");
		}
	);
}

void UiDnsProxy::_refreshStatus()
{
	if (!_status_dns->isCreate())
		return;

	std::string queries		   = "0";
	std::string cached		   = "0";
	std::string errors		   = "0";
	std::string vpn_paused_raw = "0";

	const std::string status = _dns_proxy.status();
	for (auto row : status | std::views::split('\n'))
	{
		const std::string line{ row.begin(), row.end() };
		if (line.starts_with("queries="))
			queries = line.substr(8);
		else if (line.starts_with("cache_hits="))
			cached = line.substr(11);
		else if (line.starts_with("errors="))
			errors = line.substr(7);
		else if (line.starts_with("vpn_paused="))
			vpn_paused_raw = line.substr(11);
	}

	const bool running = _dns_proxy.isRun();
	const bool issue   = _ui->unblock()->serviceReportCovers("unblock_dns");

	// The service keeps running but steps aside while a VPN tunnel is up: the
	// adapters hold the tunnel's own resolvers, so our protection is idle. The
	// key is the service's own verdict: gating it on `running` would drop the
	// pause for a tick whenever the SCM read lags, and the status text would
	// then stop changing for good.
	const bool vpn_paused = vpn_paused_raw != "0";

	// Mirror reality: the box follows the service, not a stored preference.
	// While a start/stop task is in flight the live state lags the intent, so
	// keep the optimistic value instead of flickering back.
	if (!_apply_pending.load() && _enable_dns_proxy->getState() != running)
		_enable_dns_proxy->setState(running);

	std::string text;
	if (issue)
		text = Localization::Str{ "str_status_dns_issue", _ui->unblock()->lastServiceReport().verdict }();
	else if (vpn_paused)
		text = Localization::Str{ "str_status_dns_vpn_paused" }();
	else if (running)
		text = Localization::Str{ "str_status_dns_running", queries, cached, errors }();
	else
		text = Localization::Str{ "str_status_dns_stopped" }();

	// One line per variant change: the log narrates exactly what the line shows
	// and why (issue / vpn pause / running / stopped) with the raw key and the
	// service state. Counter-only changes stay silent.
	const int variant = issue ? 0 : (vpn_paused ? 1 : (running ? 2 : 3));
	if (variant != _last_variant)
	{
		_last_variant = variant;
		Debug::info("DNS status: variant={} raw=[{}] service={} -> [{}]", variant, vpn_paused_raw, running ? "running" : "stopped", text);
	}

	// Re-assert every tick, not only on change: a paint missed once (the line
	// was written while its tab was hidden) would otherwise stay stale until
	// the text changed again. The widget clears the label before writing, so
	// the assignment always invalidates.

	if (issue || vpn_paused)
		_status_dns->setWarning(text);
	else if (running)
		_status_dns->setActive(text);
	else
		_status_dns->setInactive(text);
}
