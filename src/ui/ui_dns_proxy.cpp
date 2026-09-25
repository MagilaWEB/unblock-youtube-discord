#include "ui_dns_proxy.h"

#include "ui.h"
#include "../unblock/unblock.h"
#include "../unblock_dns/dns_config.h"

#include <algorithm>
#include <ranges>

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

	const auto presets = Unblock::defaultDnsProxyUpstreams();

	_upstream_cf->create("#dns section .common", "str_dns_proxy_cf_title", Localization::Str{ std::string{ presets[0].address } });
	_upstream_google->create("#dns section .common", "str_dns_proxy_google_title", Localization::Str{ std::string{ presets[1].address } });
	_upstream_quad9->create("#dns section .common", "str_dns_proxy_quad9_title", Localization::Str{ std::string{ presets[2].address } });

	auto on_preset = [this](JSArgs)
	{
		_collectUpstreams();
		_applyUpstreams();
		return false;
	};
	_upstream_cf->addEventClick(on_preset);
	_upstream_google->addEventClick(on_preset);
	_upstream_quad9->addEventClick(on_preset);

	_custom_upstreams->create(
		"#dns section .common",
		Localization::Str{ "str_dns_proxy_servers_title" },
		Localization::Str{ "str_dns_proxy_servers_description" }(),
		Localization::Str{ "str_input_dns_proxy_custom_placeholder" }()
	);
	_custom_upstreams->setValidator([](const std::string& value) { return parseUpstreamValue(trimConfigLine(value)).has_value(); });
	_custom_upstreams->addEventChange(
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

					std::string output;
					_unblock->dnsProxyTestUpstream(value, output);
					_window_test_result->setDescription(Localization::Str{ output.empty() ? std::string{ "—" } : output });
					_window_test_result->show();
				}
			);
			return false;
		}
	);

	// Restore persisted servers (if any), otherwise the three presets.
	std::vector<Unblock::DnsProxyUpstream> upstreams = Unblock::defaultDnsProxyUpstreams();
	if (auto cfg = _ui->userConfig()->parameterSectionVector("DNS", "upstreams"))
		if (!cfg.value().empty())
		{
			upstreams.clear();
			for (auto& line : cfg.value())
			{
				// enabled|name|address|bootstrap
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

				if (parts.size() != 4 || !parseUpstreamValue(parts[2] + "|" + parts[3]))
					continue;

				upstreams.push_back({ parts[0] == "1", std::move(parts[1]), std::move(parts[2]), std::move(parts[3]) });
			}

			if (upstreams.empty())
				upstreams = Unblock::defaultDnsProxyUpstreams();
		}

	auto find_preset = [&upstreams](std::string_view address) -> const Unblock::DnsProxyUpstream*
	{
		for (auto& u : upstreams)
			if (u.address == address)
				return &u;
		return nullptr;
	};

	if (auto* cf = find_preset(presets[0].address))
		_upstream_cf->setState(cf->enabled);
	if (auto* google = find_preset(presets[1].address))
		_upstream_google->setState(google->enabled);
	if (auto* quad9 = find_preset(presets[2].address))
		_upstream_quad9->setState(quad9->enabled);

	std::vector<std::string> customs;
	for (auto& u : upstreams)
		if (!find_preset(u.address))
			customs.push_back(u.address + "|" + u.bootstrap);
	_custom_upstreams->setItems(std::move(customs));

	_unblock->setDnsProxyUpstreams(upstreams);

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
	const auto presets = Unblock::defaultDnsProxyUpstreams();

	std::vector<Unblock::DnsProxyUpstream> upstreams{
		{	  _upstream_cf->getState(), presets[0].name, presets[0].address, presets[0].bootstrap },
		{ _upstream_google->getState(), presets[1].name, presets[1].address, presets[1].bootstrap },
		{  _upstream_quad9->getState(), presets[2].name, presets[2].address, presets[2].bootstrap },
	};

	for (auto& item : _custom_upstreams->items())
		if (auto parsed = parseUpstreamValue(item))
		{
			std::string bootstrap;
			for (size_t i = 0; i < parsed->bootstrap.size(); ++i)
				bootstrap += (i ? "," : "") + parsed->bootstrap[i];
			upstreams.push_back({ true, parsed->address, parsed->address, std::move(bootstrap) });
		}

	_unblock->setDnsProxyUpstreams(upstreams);

	std::vector<std::string> stored;
	for (auto& u : upstreams)
		stored.push_back(std::string{ u.enabled ? "1" : "0" } + "|" + u.name + "|" + u.address + "|" + u.bootstrap);
	_ui->userConfig()->writeSectionParameterVector("DNS", "upstreams", stored);
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

	const std::string status = _unblock->dnsProxyStatus();
	for (auto row : status | std::views::split('\n'))
	{
		const std::string line{ row.begin(), row.end() };
		if (line.starts_with("queries="))
			queries = line.substr(8);
		else if (line.starts_with("cache_hits="))
			cached = line.substr(11);
	}

	const bool		  running = _unblock->dnsProxyIsRun();
	const std::string text =
		running ? utils::format(Localization::Str{ "str_status_dns_running" }(), queries, cached) : Localization::Str{ "str_status_dns_stopped" }();

	if (text == _last_status)
		return;

	_last_status = text;
	running ? _status_dns->setActive(text) : _status_dns->setInactive(text);
}
