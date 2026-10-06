#include "ui_dns_proxy.h"

#include "ui.h"
#include "../unblock/dns_proxy.h"
#include "dns_config.h"

#include <algorithm>
#include <ranges>

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

	_enable_dns_proxy
		->create("#dns section .common", "str_checkbox_enable_dns_proxy_title", Localization::Str{ "str_checkbox_enable_dns_proxy_description" });
	_enable_dns_proxy->addEventClick(
		[this](JSArgs args)
		{
			const bool state = jsToCpp<bool>(args[0]);
			_ui->userConfig()->writeSectionParameter("DNS", "enable", state ? "true" : "false");

			_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
			Core::get().addTask(
				[this, state]
				{
					_dns_proxy.run(state);
					_ui->backgroundTasks()->finish("dns_proxy_apply");
				}
			);
			return false;
		}
	);

	_status_dns->create("#dns section .common");
	_status_dns->setInactive(Localization::Str{ "str_status_dns_stopped" }());

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

	_bootstrap->create(
		"#dns section .common",
		Input::Types::text,
		JSValue{ bootstrap_csv },
		Localization::Str{ "str_dns_proxy_bootstrap_title" },
		Localization::Str{ "str_dns_proxy_bootstrap_description" }
	);
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
	uint32_t timeout_sec = DnsProxy::defaultTimeout() / 1'000;
	if (auto cfg = _ui->userConfig()->parameterSection<std::string>("DNS", "timeout"))
		try
		{
			if (const int parsed = std::stoi(trimConfigLine(cfg.value())); parsed >= 1 && parsed <= 120)
				timeout_sec = static_cast<uint32_t>(parsed);
		}
		catch (...)
		{
		}
	_dns_proxy.setTimeout(timeout_sec * 1'000);

	_timeout->create(
		"#dns section .common",
		Input::Types::duration_sec,
		JSValue{ static_cast<int>(timeout_sec) },
		Localization::Str{ "str_dns_proxy_timeout_title" },
		Localization::Str{ "str_dns_proxy_timeout_description" },
		Input::Options{ 1, 120, "sec" }
	);
	_timeout->addEventSubmit(
		[this](JSArgs args)
		{
			try
			{
				int seconds = std::stoi(trimConfigLine(jsToCpp<std::string>(args[0])));
				seconds		= std::clamp(seconds, 1, 120);
				_dns_proxy.setTimeout(static_cast<uint32_t>(seconds) * 1'000);
				_collectUpstreams();
				_applyUpstreams();
			}
			catch (...)
			{
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

	const bool enabled = _ui->userConfig()->parameterSection<bool>("DNS", "enable").value_or(false);
	_enable_dns_proxy->setState(enabled);
	_refreshStatus();

	// Reconcile the service with the persisted setting: start it when enabled,
	// and kill a leftover service from a previous run when disabled.
	_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
	Core::get().addTask(
		[this, enabled]
		{
			_dns_proxy.run(enabled);
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
	if (!_ui->userConfig()->parameterSection<bool>("DNS", "enable").value_or(false))
		return;

	_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
	Core::get().addTask(
		[this]
		{
			_dns_proxy.run(true);
			_ui->backgroundTasks()->finish("dns_proxy_apply");
		}
	);
}

void UiDnsProxy::_refreshStatus()
{
	if (!_status_dns->isCreate())
		return;

	std::string queries = "0";
	std::string cached	= "0";
	std::string errors	= "0";

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
	}

	const bool		  running = _dns_proxy.isRun();
	const std::string text	  = running ? utils::format(Localization::Str{ "str_status_dns_running" }(), queries, cached, errors)
										: Localization::Str{ "str_status_dns_stopped" }();

	if (text == _last_status)
		return;

	_last_status = text;
	running ? _status_dns->setActive(text) : _status_dns->setInactive(text);
}
