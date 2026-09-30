// unblock_dns: console host for AdguardDns64.dll, run as a service under
// the project SvcHost (it delivers CTRL_C_EVENT on stop with 15s to exit).
//
//   unblock_dns.exe --config <path>          serve DNS per config, switch OS DNS
//   unblock_dns.exe --test-upstream <addr> [--bootstrap <csv>]
//                                              check one upstream, print OK/error
//
// Exit codes: 0 ok, 2 bad args/config, 3 DLL missing, 4 export missing,
// 5 proxy init failed.

#include "ag_bind.h"
#include "dns_adapters.h"
#include "dns_config.h"
#include "dns_proxy.h"
#include "dns_telemetry.h"
#include "dns_util.h"

#include <winsock2.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	std::atomic_bool s_stop{ false };

	BOOL WINAPI ctrlHandler(DWORD type)
	{
		if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT)
		{
			s_stop.store(true);
			return TRUE;
		}
		return FALSE;
	}
}	 // namespace

int main(int argc, char** argv)
{
	WSADATA wsa{};
	WSAStartup(MAKEWORD(2, 2), &wsa);

	// Set once at startup: add an IPv6 listener and switch adapter IPv6 DNS
	// only when the IPv6 loopback is usable (IPv4-only hosts skip it).
	const bool ipv6 = dns::ipv6LoopbackAvailable();

	std::string config_path;
	std::string test_upstream;
	std::string test_file;
	std::string result_file;
	std::string backup_path;
	std::string bootstrap_value;
	bool		repair		 = false;
	bool		no_os_switch = false;

	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg{ argv[i] };
		if (arg == "--config" && i + 1 < argc)
			config_path = argv[++i];
		else if (arg == "--test-upstream" && i + 1 < argc)
			test_upstream = argv[++i];
		else if (arg == "--test-upstream-file" && i + 1 < argc)
			test_file = argv[++i];
		else if (arg == "--bootstrap" && i + 1 < argc)
			bootstrap_value = argv[++i];
		else if (arg == "--result" && i + 1 < argc)
			result_file = argv[++i];
		else if (arg == "--backup" && i + 1 < argc)
			backup_path = argv[++i];
		else if (arg == "--repair")
			repair = true;
		else if (arg == "--no-os-switch")
			no_os_switch = true;
		else
		{
			std::cerr << "Usage:\n"
					  << "  unblock_dns --config <path> [--no-os-switch]\n"
					  << "  unblock_dns --test-upstream <addr> [--bootstrap <csv>] [--result <path>]\n"
					  << "  unblock_dns --test-upstream-file <in> [--bootstrap <csv>] --result <out>\n"
					  << "  unblock_dns --repair --backup <path>\n";
			return 2;
		}
	}

	std::vector<std::string> test_bootstrap;
	if (!bootstrap_value.empty())
	{
		auto parsed = parseBootstrapList(bootstrap_value);
		if (!parsed)
		{
			std::cerr << "Bad --bootstrap: " << bootstrap_value << "\n";
			return 2;
		}
		test_bootstrap = std::move(*parsed);
	}

	if (config_path.empty() && test_upstream.empty() && test_file.empty() && !repair)
	{
		std::cerr << "No mode given (need --config, --test-upstream[-file] or --repair)\n";
		return 2;
	}

	const auto dll_path = dns::exeDir() / "AdguardDns64.dll";

	AgBind		ag;
	std::string bind_error;
	if (!ag.load(dll_path.wstring(), bind_error))
	{
		std::cerr << bind_error << ": " << dll_path.string() << "\n";
		return 3;
	}

	ag.set_log_level(AGLL_INFO);
	ag.set_log_callback(dns::agLogCallback, nullptr);

	if (repair)
	{
		if (backup_path.empty())
		{
			std::cerr << "--repair needs --backup\n";
			return 2;
		}
		dns::openLog(dns::exeDir() / "unblock_dns_repair.log");
		return dns::repairAdapters(ag, backup_path, ipv6);
	}

	if (!test_upstream.empty() || !test_file.empty())
	{
		std::string value = test_upstream;
		if (!test_file.empty())
		{
			std::string error;
			value = dns::readFile(test_file, error);

			const size_t begin = value.find_first_not_of(" \t\r\n");
			if (begin == std::string::npos)
				value.clear();
			else
			{
				const size_t end = value.find_last_not_of(" \t\r\n");
				value			 = value.substr(begin, end - begin + 1);
			}
		}

		auto [code, text] = dns::runTestUpstream(ag, value, test_bootstrap);
		std::cout << text << "\n";

		if (!result_file.empty())
			dns::writeFileAtomic(result_file, text);

		return code;
	}

	dns::logLine(std::string{ "AdGuard C API: " } + (ag.capi_version() ? ag.capi_version() : "?"));

	std::string read_error;
	const auto	content = dns::readFile(config_path, read_error);
	if (content.empty() && !read_error.empty())
	{
		std::cerr << read_error << "\n";
		return 2;
	}

	auto [config, config_error] = parseProxyConfig(content);
	if (!config_error.empty())
	{
		std::cerr << config_error << "\n";
		return 2;
	}

	const std::filesystem::path cfg_dir = std::filesystem::path{ config_path }.parent_path();
	if (config.log_path.empty())
		config.log_path = (cfg_dir / "unblock_dns.log").string();
	if (config.backup_path.empty())
		config.backup_path = (cfg_dir / "unblock_dns.adapters").string();

	dns::openLog(config.log_path);
	dns::logLine("Starting with " + std::to_string(config.upstreams.size()) + " upstream(s)");

	dns::ProxySettingsBacking backing;
	ag_dnsproxy_settings*	  settings = dns::buildSettings(ag, config, backing, ipv6);
	if (!settings)
	{
		dns::logLine("Couldn't get default proxy settings");
		return 5;
	}

	// Init the proxy BEFORE touching OS DNS: a failed init must leave
	// the system resolvers alone.
	ag_dnsproxy_init_result result	= AGDPIR_OK;
	const char*				message = nullptr;
	ag_dnsproxy_events		events{ dns::requestProcessedCallback, dns::verifyCertificate };

	ag_dnsproxy* proxy = ag.init(settings, &events, &result, &message);
	if (!proxy || result != AGDPIR_OK)
	{
		dns::logLine(std::string{ "Proxy init failed: " } + dns::initResultText(result) + (message ? std::string{ " (" } + message + ")" : ""));
		return 5;
	}

	dns::logLine("Proxy listening on " + config.listen + ":" + std::to_string(config.port));

	if (no_os_switch)
		dns::logLine("--no-os-switch: adapters left untouched (proxy-only run)");
	else if (!dns::switchOsDns(ag, config, config.backup_path, ipv6))
		dns::logLine("Warning: no adapters switched, serving proxy only");

	SetConsoleCtrlHandler(ctrlHandler, TRUE);

	dns::pushStatus();

	auto last_tick = std::chrono::steady_clock::now();
	while (!s_stop.load())
	{
		Sleep(500);

		if (std::chrono::steady_clock::now() - last_tick > std::chrono::seconds{ 2 })
		{
			last_tick = std::chrono::steady_clock::now();
			dns::pushStatus();
		}
	}

	dns::logLine("Stopping");

	if (!no_os_switch)
		dns::restoreOsDns(ag, config.backup_path);
	ag.deinit(proxy);

	dns::logLine("Stopped");
	return 0;
}
