#include "ui_dns_proxy.h"

#include "ui.h"
#include "../unblock/unblock.h"
#include "../unblock_dns/dns_config.h"

#include <algorithm>
#include <ranges>
#include <unordered_set>

namespace
{
	std::vector<std::string> _splitPipe(const std::string& line)
	{
		std::vector<std::string> parts;
		size_t					 pos = 0;
		while (pos <= line.size())
		{
			size_t end = line.find('|', pos);
			if (end == std::string::npos)
				end = line.size();
			parts.push_back(line.substr(pos, end - pos));
			pos = end + 1;
		}
		return parts;
	}
}

UiDnsProxy::UiDnsProxy(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock) : _ui(std::move(ui)), _unblock(std::move(unblock))
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
			const bool state = JSToCPP<bool>(args[0]);
			_ui->userConfig()->writeSectionParameter("DNS", "enable", state ? "true" : "false");

			_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
			Core::get().addTask(
				[this, state]
				{
					_unblock->dnsProxy(state);
					_ui->backgroundTasks()->finish("dns_proxy_apply");
				}
			);
			return false;
		}
	);

	_status_dns->create("#dns section .common");
	_status_dns->setInactive(Localization::Str{ "str_status_dns_stopped" }());

	// Every upstream lives in one editable list as "address|bootstrap"; the
	// built-in presets (Cloudflare, Google, Quad9, GeoHide) are only the default
	// entries, so they can be edited or removed like any other.
	_upstreams->create(
		"#dns section .common",
		Localization::Str{ "str_dns_proxy_servers_title" },
		Localization::Str{ "str_dns_proxy_servers_description" }(),
		Localization::Str{ "str_input_dns_proxy_custom_placeholder" }()
	);
	_upstreams->setValidator([](const std::string& value) { return parseUpstreamValue(trimConfigLine(value)).has_value(); });
	_upstreams->addEventChange(
		[this](JSArgs)
		{
			_collectUpstreams();
			_applyUpstreams();
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
	_test_input->setValidator([](const std::string& value) { return parseUpstreamValue(trimConfigLine(value)).has_value(); });

	_test_button->create("#dns section .common", "str_button_test_upstream_title");
	_test_button->addEventClick(
		[this](JSArgs)
		{
			Core::get().addTask(
				[this]
				{
					// Blocking DOM getter: background task only, never the UI thread.
					const auto value = JSToCPP<std::string>(_test_input->getValue());
					// Empty or invalid — the field already says so, no window.
					if (value.empty() || !parseUpstreamValue(trimConfigLine(value)))
						return;

					_ui->backgroundTasks()->start("dns_proxy_test", "str_task_dns_proxy_test_title");

					std::string output;
					_unblock->dnsProxyTestUpstream(value, output);

					_ui->backgroundTasks()->finish("dns_proxy_test");

					_window_test_result->setDescription(Localization::Str{ output.empty() ? std::string{ "—" } : output });
					_window_test_result->show();
				}
			);
			return false;
		}
	);

	// Restore persisted servers, otherwise the built-in presets. Accept both
	// the old "enabled|name|address|bootstrap" lines and the current
	// "address|bootstrap" items.
	std::vector<std::string>	   items;
	std::unordered_set<std::string> seen;
	if (auto cfg = _ui->userConfig()->parameterSectionVector("DNS", "upstreams"))
		for (auto& line : cfg.value())
		{
			const auto		  parts = _splitPipe(line);
			const std::string item	= parts.size() == 4 ? parts[2] + "|" + parts[3] : line;
			if (parseUpstreamValue(trimConfigLine(item)) && seen.insert(item).second)
				items.push_back(item);
		}

	if (items.empty())
		for (auto& u : Unblock::defaultDnsProxyUpstreams())
			items.push_back(u.address + "|" + u.bootstrap);

	_upstreams->setItems(std::move(items));

	// Seed Unblock and persist the (possibly migrated) list.
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
			_unblock->dnsProxy(enabled);
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
	std::vector<Unblock::DnsProxyUpstream> upstreams;

	for (auto& item : _upstreams->items())
		if (auto parsed = parseUpstreamValue(item))
		{
			std::string bootstrap;
			for (size_t i = 0; i < parsed->bootstrap.size(); ++i)
				bootstrap += (i ? "," : "") + parsed->bootstrap[i];
			upstreams.push_back({ true, parsed->address, parsed->address, std::move(bootstrap) });
		}

	_unblock->setDnsProxyUpstreams(upstreams);
	_ui->userConfig()->writeSectionParameterVector("DNS", "upstreams", _upstreams->items());
}

void UiDnsProxy::_applyUpstreams()
{
	if (!_ui->userConfig()->parameterSection<bool>("DNS", "enable").value_or(false))
		return;

	_ui->backgroundTasks()->start("dns_proxy_apply", "str_task_dns_proxy_title");
	Core::get().addTask(
		[this]
		{
			_unblock->dnsProxy(true);
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

	const std::string status = _unblock->dnsProxyStatus();
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

	const bool		  running = _unblock->dnsProxyIsRun();
	const std::string text =
		running ? utils::format(Localization::Str{ "str_status_dns_running" }(), queries, cached, errors) : Localization::Str{ "str_status_dns_stopped" }();

	if (text == _last_status)
		return;

	_last_status = text;
	running ? _status_dns->setActive(text) : _status_dns->setInactive(text);
}
