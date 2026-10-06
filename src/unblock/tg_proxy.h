#pragma once

#include "../core/service.h"

#include <array>
#include <string>
#include <string_view>

/** Local Telegram WS proxy (tg-ws-proxy.exe) as a Windows service. */
class TgProxy final
{
	Service _service{ "TgWsProxy", "SvcHost.exe" };

	std::string				   _host{ "127.0.0.1" };
	std::string				   _port{ "9101" };
	std::array<std::string, 4> _dc_ip{ "149.154.175.50", "91.105.192.100", "149.154.175.100", "149.154.167.91" };
	std::string				   _cfproxy_domain{ "unblock.kermanua1488.workers.dev" };

public:
	TgProxy();

	void run(bool state = true);
	bool isRun();

	/** tg:// proxy link built from the current host/port/secret. */
	[[nodiscard]] std::string link() const;

	/** Opens the link in Telegram. Launched de-elevated (see linkRun). */
	void linkRun();

	/** Copies the link to the clipboard (fallback for the manual setup). */
	void copyLink() const;

	void setParams(std::string_view host, std::string_view port, std::array<std::string, 4> dc_ip, std::string_view cfproxy_domain);

	const std::string&				  host() const { return _host; }
	const std::string&				  port() const { return _port; }
	const std::array<std::string, 4>& dcIp() const { return _dc_ip; }
	const std::string&				  cfproxyDomain() const { return _cfproxy_domain; }
};
