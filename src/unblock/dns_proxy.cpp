#include "dns_proxy.h"

#include "ipc_signals.h"

#include "../core/core.h"
#include "../core/debug.h"
#include "../core/file_system.h"
#include "../core/hidden_process.h"

#include <fstream>
#include <sstream>

std::vector<std::string> DnsProxy::defaultUpstreams()
{
	// Order is the priority: the first entry is the primary resolver, the
	// rest are only fallbacks (see unblock_dns buildSettings). GeoHide leads
	// (queries must reach it for region-specific answers) across its DoH/DoT
	// endpoints and ports, then Comss.one (encrypted, unfiltered), then Xbox
	// DNS (free Smart DNS), then plain DNS as the last resort.
	return {
		"https://dns.geohide.ru:8443/dns-query",
		"https://dns.geohide.ru:853/dns-query",
		"https://dns.geohide.ru:443/dns-query",
		"tls://dns.geohide.ru:8443",
		"tls://dns.geohide.ru:853",
		"tls://dns.geohide.ru:443",
		"https://dns.comss.one/dns-query",
		"tls://dns.comss.one",
		"111.88.96.54",
		"111.88.96.55",
		"1.1.1.1",
		"1.0.0.1",
	};
}

std::vector<std::string> DnsProxy::defaultBootstrap()
{
	// Plain DNS used only to resolve hostname upstreams. Editable in the UI;
	// if the ISP poisons these, the user can point them at a reachable
	// resolver (e.g. GeoHide's own IPs).
	return { "1.1.1.1", "8.8.8.8", "9.9.9.9", "77.88.8.8" };
}

uint32_t DnsProxy::defaultTimeout()
{
	// A private resolver (GeoHide) stalls from time to time; a generous
	// default avoids spurious SERVFAILs. Editable in the UI.
	return 15'000;
}

DnsProxy::DnsProxy()
{
	_service.open();
}

std::filesystem::path DnsProxy::_configPath() const
{
	return Core::get().userPath() / "dns_proxy.conf";
}

std::filesystem::path DnsProxy::_backupPath() const
{
	return Core::get().userPath() / "dns_proxy.adapters";
}

std::filesystem::path DnsProxy::_logPath() const
{
	return Core::get().userPath() / "dns_proxy.log";
}

void DnsProxy::_writeConfig(const std::vector<std::string>& upstreams, const std::vector<std::string>& bootstrap, uint32_t timeout_ms)
{
	File conf{ false };
	conf.open(_configPath(), "", true);
	conf.clear();

	conf.writeText("listen=127.0.0.1");
	conf.writeText("port=53");
	conf.writeText("timeout=" + std::to_string(timeout_ms));
	// Explicit paths: the wrapper reads these verbatim, so engine and wrapper
	// can never disagree on where backup/log live. Status is pushed over IPC
	// (UDP 9999), not written to disk.
	conf.writeText("backup=" + _backupPath().string());
	conf.writeText("log=" + _logPath().string());

	std::string bootstrap_line;
	for (const auto& b : bootstrap)
		bootstrap_line += (bootstrap_line.empty() ? "" : ",") + b;
	conf.writeText("bootstrap=" + bootstrap_line);

	for (const auto& upstream : upstreams)
		if (!upstream.empty())
			conf.writeText("upstream=" + upstream);

	conf.close();
}

void DnsProxy::run(bool state)
{
	if (!state)
	{
		_service.remove();
		// SvcHost stops its child with TerminateProcess (see MagilaWEB/svc_host),
		// so the wrapper never gets to run its own OS-DNS restore on stop. Undo
		// the adapter switch from here, using the backup written when it happened.
		repairBoot();
		IPCSignals::get().clear("dns.queries");
		IPCSignals::get().clear("dns.cache_hits");
		IPCSignals::get().clear("dns.errors");
		return;
	}

	const auto& upstreams = _upstreams.empty() ? defaultUpstreams() : _upstreams;
	const auto& bootstrap = _bootstrap.empty() ? defaultBootstrap() : _bootstrap;
	const auto	timeout	  = _timeout_ms != 0 ? _timeout_ms : defaultTimeout();

	_writeConfig(upstreams, bootstrap, timeout);

	// Reset the IPC counters so a stale run's numbers don't show on start.
	IPCSignals::get().clear("dns.queries");
	IPCSignals::get().clear("dns.cache_hits");
	IPCSignals::get().clear("dns.errors");

	_service.remove();
	_service.setDescription("Unblock DNS proxy (AdGuard DnsLibs).");
	_service.setArgs({ (Core::get().binPath() / "unblock_dns.exe").string(), "--config", "\"" + _configPath().string() + "\"" });
	_service.create();
	_service.start();
}

bool DnsProxy::isRun()
{
	return _service.isRun();
}

void DnsProxy::setUpstreams(std::vector<std::string> upstreams)
{
	_upstreams = std::move(upstreams);
}

const std::vector<std::string>& DnsProxy::upstreams() const
{
	return _upstreams;
}

void DnsProxy::setBootstrap(std::vector<std::string> bootstrap)
{
	_bootstrap = std::move(bootstrap);
}

const std::vector<std::string>& DnsProxy::bootstrap() const
{
	return _bootstrap;
}

void DnsProxy::setTimeout(uint32_t timeout_ms)
{
	_timeout_ms = timeout_ms;
}

uint32_t DnsProxy::timeout() const
{
	return _timeout_ms;
}

std::string DnsProxy::status() const
{
	auto&			   ipc = IPCSignals::get();
	std::ostringstream ss;
	ss << "queries=" << ipc.getLatestU32("dns.queries").value_or(0) << "\n";
	ss << "cache_hits=" << ipc.getLatestU32("dns.cache_hits").value_or(0) << "\n";
	ss << "errors=" << ipc.getLatestU32("dns.errors").value_or(0) << "\n";
	return ss.str();
}

bool DnsProxy::testUpstream(const std::string& value, std::string& output)
{
	const auto in_path	= Core::get().tempPath() / "unblock_dns_test.in";
	const auto out_path = Core::get().tempPath() / "unblock_dns_test.out";

	{
		File in{ false };
		in.open(in_path, "", true);
		in.clear();
		in.writeText(value);
		in.close();
	}

	std::error_code ec;
	std::filesystem::remove(out_path, ec);

	std::string bootstrap_csv;
	const auto& bootstrap = _bootstrap.empty() ? defaultBootstrap() : _bootstrap;
	for (const auto& b : bootstrap)
		bootstrap_csv += (bootstrap_csv.empty() ? "" : ",") + b;

	const bool ok = runHiddenProcess(
		{ (Core::get().binPath() / "unblock_dns.exe").string(),
		  "--test-upstream-file",
		  in_path.string(),
		  "--bootstrap",
		  bootstrap_csv,
		  "--result",
		  out_path.string() },
		15'000
	);

	std::ifstream result{ out_path, std::ios::binary };
	if (result)
	{
		std::ostringstream ss;
		ss << result.rdbuf();
		output = ss.str();
	}
	else
	{
		output = ok ? "OK" : "FAIL: unblock_dns produced no result";
	}

	return output.starts_with("OK");
}

void DnsProxy::repairBoot()
{
	if (_service.isRun())
		return;

	const auto backup_path = _backupPath();
	if (!std::filesystem::exists(backup_path))
		return;

	// All adapter work lives in the wrapper (DLL API), the engine never
	// touches the registry itself.
	if (runHiddenProcess({ (Core::get().binPath() / "unblock_dns.exe").string(), "--repair", "--backup", backup_path.string() }, 15'000))
		Debug::warning("DNS proxy was killed without restore, adapters repaired.");
	else
		Debug::warning("DNS proxy adapter restore failed, adapters may still point at the local proxy.");
}
