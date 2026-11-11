module unblock.tg_proxy;

import ext.windows;
import core;
import core.debug;
import core.file_system;
import core.service;
import core.utils;

namespace
{
	// Defaults live in configs/tg_proxy.config [DEFAULTS]: the first-run values,
	// the reset button and the arguments the proxy service is started with.
	// Opened once - edit the config and restart to pick up changes.
	File& configDefaults()
	{
		static File config{ false };

		if (!config.isOpen())
		{
			config.open(Core::get().configsPath() / "tg_proxy", ".config", true);

			if (!config.isOpen())
				Debug::error("configs/tg_proxy.config is missing or unreadable: the Telegram proxy defaults live there!");
		}

		return config;
	}

	std::string defaultValue(std::string_view parameter)
	{
		if (auto value = configDefaults().parameterSection<std::string>("DEFAULTS", std::string{ parameter }))
			return value.value();

		Debug::warning("tg proxy: configs/tg_proxy.config has no [DEFAULTS] {} - the default is empty", parameter);
		return {};
	}
}

TgProxy::TgProxy()
{
	_service.open();
}

std::string TgProxy::defaultHost()
{
	return defaultValue("host");
}

std::string TgProxy::defaultPort()
{
	return defaultValue("port");
}

std::array<std::string, 4> TgProxy::defaultDcIp()
{
	if (auto dc_ip = configDefaults().parameterSectionVector("DEFAULTS", "dc_ip"); dc_ip && dc_ip.value().size() == 4)
		return { dc_ip.value()[0], dc_ip.value()[1], dc_ip.value()[2], dc_ip.value()[3] };

	Debug::error("configs/tg_proxy.config [DEFAULTS] dc_ip must list exactly 4 addresses!");
	return {};
}

std::string TgProxy::defaultCfproxyDomain()
{
	return defaultValue("cfproxy_domain");
}

std::string TgProxy::defaultSecret()
{
	return defaultValue("secret");
}

void TgProxy::run(bool state)
{
	if (!state)
	{
		_service.remove();
		return;
	}

	_service.remove();
	_service.setDescription("Local proxy telegram.");
	_service.setArgs(
		{ (Core::get().binariesPath() / "tg-ws-proxy.exe").string(),
		  std::string{ "--secret " } + defaultSecret(),
		  "--dc-ip 1:" + _dc_ip[0] + " --dc-ip 2:" + _dc_ip[1] + " --dc-ip 3:" + _dc_ip[2] + " --dc-ip 4:" + _dc_ip[3],
		  //		  "--cfproxy-worker-domain " + _cfproxy_domain,
		  "--host " + _host,
		  "--port " + _port }
	);
	_service.create();
	_service.start();
}

bool TgProxy::isRun()
{
	return _service.isRun();
}

const ServiceFailure& TgProxy::getLastFailure() const
{
	return _service.getLastFailure();
}

std::string TgProxy::link() const
{
	std::string tg{ "tg://proxy?server=" };
	tg.append(_host);
	tg.append("&port=");
	tg.append(_port);
	tg.append("&secret=");
	tg.append(defaultSecret());
	return tg;
}

void TgProxy::linkRun()
{
	Core::get().addTask(
		[this]
		{
			const std::string tg = link();

			// unblock runs elevated (the engine self-elevates). A direct
			// ShellExecuteA from an elevated process cannot activate a packaged
			// (Store/MSIX) Telegram, so the click silently does nothing there.
			// Handing the URL to explorer.exe de-elevates the handoff: the
			// already running shell (non-elevated) opens the link. Fall back to
			// a direct open if explorer is unavailable.
			//
			// ShellExecuteA forwards '&' in the URL verbatim, whereas
			// system("start ...") routes the link through cmd.exe, which
			// interprets '&' as a command separator and truncates the URL.
			const std::string quoted = "\"" + tg + "\"";
			auto result = reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", "explorer.exe", quoted.c_str(), nullptr, SW_SHOWNORMAL));
			if (result <= 32)
			{
				Debug::warning("tg proxy: explorer handoff failed (code {}), trying direct open", result);
				result = reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", tg.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
			}

			if (result <= 32)
				Debug::error("tg proxy: could not open {} (ShellExecute code {})", tg, result);
			else
				Debug::ok("tg proxy: opened {}", tg);
		}
	);
}

void TgProxy::copyLink() const
{
	if (utils::copyToClipboard(link()))
		Debug::ok("tg proxy: link copied to clipboard");
	else
		Debug::error("tg proxy: failed to copy link to clipboard");
}

void TgProxy::setParams(std::string_view host, std::string_view port, std::array<std::string, 4> dc_ip, std::string_view cfproxy_domain)
{
	_host			= host;
	_port			= port;
	_dc_ip			= std::move(dc_ip);
	_cfproxy_domain = cfproxy_domain;
}
