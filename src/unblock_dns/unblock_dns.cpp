// unblock_dns: console host for AdguardDns64.dll, run as a service under
// the project SvcHost (it delivers CTRL_C_EVENT on stop with 15s to exit).
//
//   unblock_dns.exe --config <path>          serve DNS per config, switch OS DNS
//   unblock_dns.exe --test-upstream <addr>[|<boot,csv>]
//                                              check one upstream, print OK/error
//
// Exit codes: 0 ok, 2 bad args/config, 3 DLL missing, 4 export missing,
// 5 proxy init failed.

#include "ag_bind.h"
#include "dns_config.h"

#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <iphlpapi.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>

namespace
{
	std::atomic_bool g_stop{ false };

	BOOL WINAPI ctrlHandler(DWORD type)
	{
		if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT)
		{
			g_stop.store(true);
			return TRUE;
		}
		return FALSE;
	}

	struct Logger
	{
		std::mutex	  mutex;
		std::ofstream file;

		void open(const std::filesystem::path& path)
		{
			// Cap the log: start fresh past 1MB.
			std::error_code ec;
			if (std::filesystem::file_size(path, ec) > 1'024 * 1'024)
				std::filesystem::resize_file(path, 0, ec);

			file.open(path, std::ios::app);
		}

		void write(const std::string& line)
		{
			const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			char stamp[32]{};
			ctime_s(stamp, sizeof(stamp), &now);
			stamp[strcspn(stamp, "\n")] = '\0';

			std::lock_guard lock{ mutex };
			if (file.is_open())
				file << "[" << stamp << "] " << line << "\n" << std::flush;
		}
	};

	Logger g_log;

	void logLine(const std::string& line)
	{
		g_log.write(line);
	}

	void agLogCallback(void* /*attachment*/, ag_log_level level, const char* message, uint32_t length)
	{
		if (!message || length == 0)
			return;

		std::string text{ message, length };
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
			text.pop_back();

		const char* prefix = "?";
		switch (level)
		{
		case AGLL_ERR:
			prefix = "E";
			break;
		case AGLL_WARN:
			prefix = "W";
			break;
		case AGLL_INFO:
			prefix = "I";
			break;
		case AGLL_DEBUG:
			prefix = "D";
			break;
		case AGLL_TRACE:
			prefix = "T";
			break;
		}

		logLine(std::string{ "[ag:" } + prefix + "] " + text);
	}

	struct Stats
	{
		std::atomic_uint64_t queries{ 0 };
		std::atomic_uint64_t cache_hits{ 0 };
		std::atomic_uint64_t errors{ 0 };
		std::atomic_int32_t	 last_upstream{ -1 };
	};

	Stats g_stats;

	void requestProcessedCallback(const ag_dns_request_processed_event* event)
	{
		if (!event)
			return;

		g_stats.queries.fetch_add(1);
		if (event->cache_hit)
			g_stats.cache_hits.fetch_add(1);
		if (event->error)
			g_stats.errors.fetch_add(1);
		if (event->upstream_id)
			g_stats.last_upstream.store(*event->upstream_id);
	}

	std::string readFile(const std::filesystem::path& path, std::string& error)
	{
		std::ifstream in{ path, std::ios::binary };
		if (!in)
		{
			error = "Couldn't open file: " + path.string();
			return {};
		}

		std::ostringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}

	bool writeFileAtomic(const std::filesystem::path& path, const std::string& content)
	{
		const auto tmp = path.string() + ".tmp";
		{
			std::ofstream out{ tmp, std::ios::binary | std::ios::trunc };
			if (!out)
				return false;
			out << content;
		}

		std::error_code ec;
		std::filesystem::rename(tmp, path, ec);
		return !ec;
	}

	std::filesystem::path exeDir()
	{
		wchar_t buf[MAX_PATH]{};
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return std::filesystem::path{ buf }.parent_path();
	}

	// --- OS DNS switch ------------------------------------------------------

	struct AdapterDns
	{
		std::string guid;
		std::string nameserver;	   // empty = automatic
	};

	// Interfaces that currently hold an IPv4 default route (0.0.0.0/0).
	// Only those get their DNS switched: touching a VPN/tunnel or a
	// secondary adapter can break connectivity.
	std::set<DWORD> defaultRouteIndexes()
	{
		std::set<DWORD> out;

		DWORD size = 0;
		GetIpForwardTable(nullptr, &size, FALSE);

		std::vector<uint8_t> buffer(size);
		auto*				 table = reinterpret_cast<MIB_IPFORWARDTABLE*>(buffer.data());
		if (GetIpForwardTable(table, &size, FALSE) == NO_ERROR)
		{
			for (DWORD i = 0; i < table->dwNumEntries; ++i)
			{
				const auto& row = table->table[i];
				if (row.dwForwardDest == 0 && row.dwForwardMask == 0)
					out.insert(row.dwForwardIfIndex);
			}
		}

		if (out.empty())
		{
			MIB_IPFORWARDROW best{};
			if (GetBestRoute(0, 0, &best) == NO_ERROR)
				out.insert(best.dwForwardIfIndex);
		}

		return out;
	}

	std::vector<AdapterDns> listUpAdapters()
	{
		std::vector<AdapterDns> out;

		const auto wanted = defaultRouteIndexes();

		ULONG size = 0;
		GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, nullptr, &size);

		std::vector<uint8_t>  buffer(size);
		IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
		if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, adapters, &size) != NO_ERROR)
			return out;

		for (auto* a = adapters; a; a = a->Next)
		{
			if (a->OperStatus != IfOperStatusUp)
				continue;
			if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK || a->IfType == IF_TYPE_TUNNEL)
				continue;
			if (!wanted.contains(a->IfIndex))
				continue;

			out.push_back({ a->AdapterName ? a->AdapterName : "", {} });
		}

		return out;
	}

	std::vector<AdapterDns> readBackup(const std::filesystem::path& path)
	{
		std::vector<AdapterDns> out;

		std::string error;
		const auto	content = readFile(path, error);
		if (content.empty())
			return out;

		std::istringstream in{ content };
		std::string		   line;
		while (std::getline(in, line))
		{
			line = trimConfigLine(line);
			if (line.empty() || line.starts_with('#'))
				continue;

			const size_t sep = line.find('|');
			if (sep == std::string::npos)
				continue;

			out.push_back({ trimConfigLine(line.substr(0, sep)), trimConfigLine(line.substr(sep + 1)) });
		}

		return out;
	}

	void writeBackup(const std::filesystem::path& path, const std::vector<AdapterDns>& adapters, const std::string& listen)
	{
		std::string content	 = "# listen=" + listen + "\n";
		content				+= "# guid|nameserver (empty = automatic), written by unblock_dns\n";
		for (auto& a : adapters)
			content += a.guid + "|" + a.nameserver + "\n";

		writeFileAtomic(path, content);
	}

	std::string readBackupListen(const std::filesystem::path& path)
	{
		std::string error;
		const auto	content = readFile(path, error);

		std::istringstream in{ content };
		std::string		   line;
		while (std::getline(in, line))
		{
			line = trimConfigLine(line);
			if (line.starts_with("# listen="))
				return trimConfigLine(line.substr(9));
		}

		return {};
	}

	uint32_t setAdapterDns(AgBind& ag, const std::string& guid, const std::string& nameserver)
	{
		return ag.set_if_nameserver(nameserver.c_str(), guid.c_str(), false);
	}

	std::string currentAdapterDns(AgBind& ag, const std::string& guid)
	{
		char* raw = ag.get_if_nameserver(guid.c_str(), false);
		if (!raw)
			return {};

		std::string out{ raw };
		ag.str_free(raw);
		return out;
	}

	void flushResolverCache()
	{
		// DnsFlushResolverCache has no header declaration in current SDKs
		// (the dnsapi.dll export itself is alive), so bind it at runtime.
		using FlushFn		 = BOOL(WINAPI*)();
		static FlushFn flush = nullptr;
		static bool	   tried = false;

		if (!tried)
		{
			tried = true;
			if (HMODULE dnsapi = LoadLibraryW(L"dnsapi.dll"))
			{
				FARPROC proc = GetProcAddress(dnsapi, "DnsFlushResolverCache");
				if (proc)
					std::memcpy(&flush, &proc, sizeof(proc));
			}
		}

		if (flush)
			flush();
	}

	// Switches all up adapters to the proxy. A stale backup (previous run
	// killed hard) is used to recover true originals when the adapters
	// still point at our own listen address.
	bool switchOsDns(AgBind& ag, const DnsProxyConfig& config, const std::filesystem::path& backup_path)
	{
		auto adapters = listUpAdapters();

		std::vector<AdapterDns> stale = readBackup(backup_path);
		std::string				note  = stale.empty() ? "" : " (stale backup found, recovering originals)";

		logLine("Switching " + std::to_string(adapters.size()) + " adapter(s) to " + config.listen + note);

		for (auto& a : adapters)
		{
			std::string current = currentAdapterDns(ag, a.guid);

			if (current == config.listen && !stale.empty())
			{
				for (auto& s : stale)
					if (s.guid == a.guid)
					{
						current = s.nameserver;
						break;
					}
			}

			a.nameserver = current;

			if (uint32_t err = setAdapterDns(ag, a.guid, config.listen))
				logLine("Couldn't set DNS on [" + a.guid + "], error " + std::to_string(err));
		}

		writeBackup(backup_path, adapters, config.listen);
		flushResolverCache();
		return !adapters.empty();
	}

	void restoreOsDns(AgBind& ag, const std::filesystem::path& backup_path)
	{
		auto backup = readBackup(backup_path);
		logLine("Restoring DNS on " + std::to_string(backup.size()) + " adapter(s)");

		bool failed = false;
		for (auto& a : backup)
			if (uint32_t err = setAdapterDns(ag, a.guid, a.nameserver))
			{
				logLine("Couldn't restore DNS on [" + a.guid + "], error " + std::to_string(err));
				failed = true;
			}

		flushResolverCache();

		// The backup is only meaningful while adapters point at us. Drop it once
		// they are back, otherwise a later boot would "repair" a clean system.
		if (!failed)
		{
			std::error_code ec;
			std::filesystem::remove(backup_path, ec);
		}
	}

	// Boot recovery: a hard-killed run left adapters on our listen address.
	// Only adapters still pointing at that address are restored, so a manual
	// DNS change made after the crash is never clobbered. Adapter work goes
	// through the DLL API (the engine never touches the registry itself).
	int repairAdapters(AgBind& ag, const std::filesystem::path& backup_path)
	{
		if (!std::filesystem::exists(backup_path))
			return 0;

		const std::string listen = readBackupListen(backup_path);
		auto			  backup = readBackup(backup_path);

		logLine("Repairing " + std::to_string(backup.size()) + " adapter(s) (listen " + listen + ")");

		bool restored = false;
		bool failed	  = false;
		for (auto& a : backup)
		{
			if (!listen.empty() && currentAdapterDns(ag, a.guid) != listen)
				continue;

			if (uint32_t err = setAdapterDns(ag, a.guid, a.nameserver))
			{
				logLine("Couldn't restore DNS on [" + a.guid + "], error " + std::to_string(err));
				failed = true;
			}
			else
				restored = true;
		}

		if (restored)
			flushResolverCache();

		// Consumed: adapters either point at us no more (restored now, or the
		// user changed DNS manually). Keep it only when a restore failed, so a
		// later run can retry.
		if (!failed)
		{
			std::error_code ec;
			std::filesystem::remove(backup_path, ec);
		}

		return failed ? 1 : 0;
	}

	// --- Proxy settings -----------------------------------------------------

	struct ProxySettingsBacking
	{
		ag_dnsproxy_settings				  settings{};
		std::vector<ag_upstream_options>	  upstreams;
		std::vector<std::string>			  upstream_strings;
		std::vector<std::vector<std::string>> bootstrap_strings;
		std::vector<std::vector<const char*>> bootstrap_ptrs;
		std::vector<ag_listener_settings>	  listeners;
		std::string							  listen_string;
	};

	const char* initResultText(ag_dnsproxy_init_result result)
	{
		switch (result)
		{
		case AGDPIR_PROXY_NOT_SET:
			return "proxy not set";
		case AGDPIR_EVENT_LOOP_NOT_SET:
			return "event loop not set";
		case AGDPIR_INVALID_ADDRESS:
			return "invalid address";
		case AGDPIR_EMPTY_PROXY:
			return "empty proxy";
		case AGDPIR_PROTOCOL_ERROR:
			return "protocol error";
		case AGDPIR_LISTENER_INIT_ERROR:
			return "listener init error (port busy?)";
		case AGDPIR_INVALID_IPV4:
			return "invalid IPv4";
		case AGDPIR_INVALID_IPV6:
			return "invalid IPv6";
		case AGDPIR_UPSTREAM_INIT_ERROR:
			return "upstream init error";
		case AGDPIR_FALLBACK_FILTER_INIT_ERROR:
			return "fallback filter init error";
		case AGDPIR_FILTER_LOAD_ERROR:
			return "filter load error";
		case AGDPIR_MEM_LIMIT_REACHED:
			return "memory limit reached";
		case AGDPIR_NON_UNIQUE_FILTER_ID:
			return "non-unique filter id";
		case AGDPIR_OK:
			return "ok";
		}
		return "unknown";
	}

	// Fills DLL settings from our config. We build our own settings struct
	// from the library defaults (copying the scalar tuning values, not the
	// library-owned arrays), then free the defaults struct. This avoids both
	// a leak and a double-free of our own arrays on settings_free.
	ag_dnsproxy_settings* buildSettings(AgBind& ag, const DnsProxyConfig& config, ProxySettingsBacking& backing)
	{
		ag_dnsproxy_settings* defaults = ag.settings_default();
		if (!defaults)
			return nullptr;

		backing.settings.blocked_response_ttl_secs	 = defaults->blocked_response_ttl_secs;
		backing.settings.adblock_rules_blocking_mode = defaults->adblock_rules_blocking_mode;
		backing.settings.hosts_rules_blocking_mode	 = defaults->hosts_rules_blocking_mode;
		backing.settings.dns_cache_size				 = defaults->dns_cache_size;
		backing.settings.upstream_timeout_ms		 = defaults->upstream_timeout_ms;
		ag.settings_free(defaults);

		backing.listen_string = config.listen;

		backing.upstreams.reserve(config.upstreams.size());
		backing.upstream_strings.reserve(config.upstreams.size());
		backing.bootstrap_strings.reserve(config.upstreams.size());
		backing.bootstrap_ptrs.reserve(config.upstreams.size());

		int32_t id = 0;
		for (auto& u : config.upstreams)
		{
			backing.upstream_strings.push_back(u.address);
			backing.bootstrap_strings.emplace_back(u.bootstrap);
			backing.bootstrap_ptrs.emplace_back();
			for (auto& b : backing.bootstrap_strings.back())
				backing.bootstrap_ptrs.back().push_back(b.c_str());

			ag_upstream_options opt{};
			opt.address					 = backing.upstream_strings.back().c_str();
			opt.bootstrap.data			 = backing.bootstrap_ptrs.back().empty() ? nullptr : backing.bootstrap_ptrs.back().data();
			opt.bootstrap.size			 = static_cast<uint32_t>(backing.bootstrap_ptrs.back().size());
			opt.id						 = ++id;
			opt.outbound_interface_index = 0;

			backing.upstreams.push_back(opt);
		}

		backing.settings.upstreams.data = backing.upstreams.data();
		backing.settings.upstreams.size = static_cast<uint32_t>(backing.upstreams.size());

		backing.listeners.resize(2);
		backing.listeners[0] = ag_listener_settings{ backing.listen_string.c_str(), config.port, AGLP_UDP, false, 0, {} };
		backing.listeners[1] = ag_listener_settings{ backing.listen_string.c_str(), config.port, AGLP_TCP, true, 30'000, {} };

		backing.settings.listeners.data = backing.listeners.data();
		backing.settings.listeners.size = static_cast<uint32_t>(backing.listeners.size());

		backing.settings.block_ipv6							  = false;
		backing.settings.ipv6_available						  = false;
		backing.settings.enable_dnssec_ok					  = false;
		backing.settings.enable_retransmission_handling		  = true;
		backing.settings.block_ech							  = false;
		backing.settings.block_h3_alpn						  = false;
		backing.settings.enable_parallel_upstream_queries	  = true;
		backing.settings.enable_fallback_on_upstreams_failure = true;
		backing.settings.enable_servfail_on_upstreams_failure = true;
		backing.settings.enable_http3						  = false;
		backing.settings.enable_post_quantum_cryptography	  = false;
		backing.settings.optimistic_cache					  = true;

		return &backing.settings;
	}

	void writeStatus(const std::filesystem::path& path, const std::string& state, size_t upstreams)
	{
		std::ostringstream ss;
		ss << "state=" << state << "\n";
		ss << "upstreams=" << upstreams << "\n";
		ss << "queries=" << g_stats.queries.load() << "\n";
		ss << "cache_hits=" << g_stats.cache_hits.load() << "\n";
		ss << "errors=" << g_stats.errors.load() << "\n";
		ss << "last_upstream=" << g_stats.last_upstream.load() << "\n";

		writeFileAtomic(path, ss.str());
	}

	// AdGuard DnsLibs does not carry platform roots: on Windows the host must
	// verify each TLS chain against the system certificate stores (that is how
	// Chromium, which this library is derived from, works). Without a callback
	// the DLL rejects every upstream certificate, so even a reachable DoH/DoT
	// server fails the handshake.
	ag_certificate_verification_result verifyCertificate(const ag_certificate_verification_event* event)
	{
		if (!event || !event->certificate.data || event->certificate.size == 0)
			return AGCVR_ERROR_CREATE_CERT;

		constexpr DWORD encoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

		PCCERT_CONTEXT leaf = CertCreateCertificateContext(encoding, event->certificate.data, static_cast<DWORD>(event->certificate.size));
		if (!leaf)
			return AGCVR_ERROR_CREATE_CERT;

		HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
		if (!store)
		{
			CertFreeCertificateContext(leaf);
			return AGCVR_ERROR_ACCESS_TO_STORE;
		}

		bool ok = true;
		for (uint32_t i = 0; i < event->chain.size && ok; ++i)
		{
			const ag_buffer& item = event->chain.data[i];
			if (!item.data || item.size == 0)
				continue;

			PCCERT_CONTEXT cert = CertCreateCertificateContext(encoding, item.data, static_cast<DWORD>(item.size));
			if (!cert)
			{
				ok = false;
				break;
			}

			if (!CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_ALWAYS, nullptr))
				ok = false;

			CertFreeCertificateContext(cert);
		}

		ag_certificate_verification_result result = AGCVR_ERROR_CERT_VERIFICATION;

		if (ok)
		{
			CERT_CHAIN_PARA para{};
			para.cbSize = sizeof(para);

			LPSTR			 server_auth = const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);
			CERT_ENHKEY_USAGE usage{};
			usage.cUsageIdentifier		= 1;
			usage.rgpszUsageIdentifier	= &server_auth;
			para.RequestedUsage.dwType	= USAGE_MATCH_TYPE_AND;
			para.RequestedUsage.Usage	= usage;

			// No revocation flags: an online CRL/OCSP check would add latency
			// and fail under filtering; chain trust against the system roots is
			// what matters here.
			PCCERT_CHAIN_CONTEXT chain = nullptr;
			if (CertGetCertificateChain(nullptr, leaf, nullptr, store, &para, 0, nullptr, &chain))
			{
				if (chain->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR)
					result = AGCVR_OK;
				CertFreeCertificateChain(chain);
			}
		}

		CertCloseStore(store, 0);
		CertFreeCertificateContext(leaf);
		return result;
	}

	std::pair<int, std::string> runTestUpstream(AgBind& ag, const std::string& value)
	{
		auto parsed = parseUpstreamValue(value);
		if (!parsed)
			return { 2, "FAIL: bad upstream format" };

		ag_upstream_options opt{};
		opt.address = parsed->address.c_str();

		std::vector<const char*> boots;
		for (auto& b : parsed->bootstrap)
			boots.push_back(b.c_str());
		opt.bootstrap.data = boots.empty() ? nullptr : boots.data();
		opt.bootstrap.size = static_cast<uint32_t>(boots.size());

		const char* error = ag.test_upstream(&opt, 10'000, false, verifyCertificate, false);
		if (!error)
			return { 0, "OK" };

		std::string text = std::string{ "FAIL: " } + error;
		ag.str_free(error);
		return { 1, std::move(text) };
	}
}	 // namespace

int main(int argc, char** argv)
{
	std::string config_path;
	std::string test_upstream;
	std::string test_file;
	std::string result_file;
	std::string backup_path;
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
					  << "  unblock_dns --test-upstream <addr>[|<boot,csv>] [--result <path>]\n"
					  << "  unblock_dns --test-upstream-file <in> --result <out>\n"
					  << "  unblock_dns --repair --backup <path>\n";
			return 2;
		}
	}

	if (config_path.empty() && test_upstream.empty() && test_file.empty() && !repair)
	{
		std::cerr << "No mode given (need --config, --test-upstream[-file] or --repair)\n";
		return 2;
	}

	const auto dll_path = exeDir() / "AdguardDns64.dll";

	AgBind		ag;
	std::string bind_error;
	if (!ag.load(dll_path.wstring(), bind_error))
	{
		std::cerr << bind_error << ": " << dll_path.string() << "\n";
		return 3;
	}

	ag.set_log_level(AGLL_INFO);
	ag.set_log_callback(agLogCallback, nullptr);

	if (repair)
	{
		if (backup_path.empty())
		{
			std::cerr << "--repair needs --backup\n";
			return 2;
		}
		g_log.open(exeDir() / "unblock_dns_repair.log");
		return repairAdapters(ag, backup_path);
	}

	if (!test_upstream.empty() || !test_file.empty())
	{
		std::string value = test_upstream;
		if (!test_file.empty())
		{
			std::string error;
			value = readFile(test_file, error);

			const size_t begin = value.find_first_not_of(" \t\r\n");
			if (begin == std::string::npos)
				value.clear();
			else
			{
				const size_t end = value.find_last_not_of(" \t\r\n");
				value			 = value.substr(begin, end - begin + 1);
			}
		}

		auto [code, text] = runTestUpstream(ag, value);
		std::cout << text << "\n";

		if (!result_file.empty())
			writeFileAtomic(result_file, text);

		return code;
	}

	logLine(std::string{ "AdGuard C API: " } + (ag.capi_version() ? ag.capi_version() : "?"));

	std::string read_error;
	const auto	content = readFile(config_path, read_error);
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
	if (config.status_path.empty())
		config.status_path = (cfg_dir / "unblock_dns.status").string();
	if (config.backup_path.empty())
		config.backup_path = (cfg_dir / "unblock_dns.adapters").string();

	g_log.open(config.log_path);
	logLine("Starting with " + std::to_string(config.upstreams.size()) + " upstream(s)");

	ProxySettingsBacking  backing;
	ag_dnsproxy_settings* settings = buildSettings(ag, config, backing);
	if (!settings)
	{
		logLine("Couldn't get default proxy settings");
		return 5;
	}

	// Init the proxy BEFORE touching OS DNS: a failed init must leave
	// the system resolvers alone.
	ag_dnsproxy_init_result result	= AGDPIR_OK;
	const char*				message = nullptr;
	ag_dnsproxy_events		events{ requestProcessedCallback, verifyCertificate };

	ag_dnsproxy* proxy = ag.init(settings, &events, &result, &message);
	if (!proxy || result != AGDPIR_OK)
	{
		logLine(std::string{ "Proxy init failed: " } + initResultText(result) + (message ? std::string{ " (" } + message + ")" : ""));
		writeStatus(config.status_path, "init_failed", config.upstreams.size());
		return 5;
	}

	logLine("Proxy listening on " + config.listen + ":" + std::to_string(config.port));

	if (no_os_switch)
		logLine("--no-os-switch: adapters left untouched (proxy-only run)");
	else if (!switchOsDns(ag, config, config.backup_path))
		logLine("Warning: no adapters switched, serving proxy only");

	SetConsoleCtrlHandler(ctrlHandler, TRUE);

	writeStatus(config.status_path, "running", config.upstreams.size());

	auto last_tick = std::chrono::steady_clock::now();
	while (!g_stop.load())
	{
		Sleep(500);

		if (std::chrono::steady_clock::now() - last_tick > std::chrono::seconds{ 5 })
		{
			last_tick = std::chrono::steady_clock::now();
			writeStatus(config.status_path, "running", config.upstreams.size());
		}
	}

	logLine("Stopping");
	writeStatus(config.status_path, "stopping", config.upstreams.size());

	if (!no_os_switch)
		restoreOsDns(ag, config.backup_path);
	ag.deinit(proxy);

	writeStatus(config.status_path, "stopped", config.upstreams.size());
	logLine("Stopped");
	return 0;
}
