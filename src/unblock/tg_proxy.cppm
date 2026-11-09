export module unblock.tg_proxy;

import std;
import core.service;

/** Local Telegram WS proxy (tg-ws-proxy.exe) as a Windows service. */
export class TgProxy final
{
	Service _service{ "TgWsProxy", "SvcHost.exe" };

	std::string				   _host{ defaultHost() };
	std::string				   _port{ defaultPort() };
	std::array<std::string, 4> _dc_ip{ defaultDcIp() };
	std::string				   _cfproxy_domain{ defaultCfproxyDomain() };

public:
	TgProxy();

	/** Built-in proxy settings — the reset-to-defaults target. */
	static std::string				  defaultHost();
	static std::string				  defaultPort();
	static std::array<std::string, 4> defaultDcIp();
	static std::string				  defaultCfproxyDomain();

	void run(bool state = true);
	bool isRun();

	/** Snapshot of the service's last failure (see ServiceFailure). */
	[[nodiscard]] const ServiceFailure& getLastFailure() const;

	/** Underlying Windows service, exposed for the diagnostics collector. */
	[[nodiscard]] Service& service() { return _service; }

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
