#include "dns_adapters.h"

#include "dns_telemetry.h"
#include "dns_util.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <algorithm>
#include <cstring>
#include <ranges>
#include <set>
#include <string>
#include <vector>

namespace dns
{
	namespace
	{
		// IPv6 loopback the proxy also listens on. Windows prefers IPv6 DNS
		// (router RDNSS/advertised) over IPv4, so switching only 127.0.0.1 left
		// the real resolver pointing at the ISP and swept every query past us.
		inline constexpr const char* c_listen_ipv6{ "::1" };

		struct AdapterDns
		{
			std::string guid;
			std::string nameserver;		// empty = automatic
			std::string nameserver6;	// IPv6, empty = automatic
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

				out.push_back({ a->AdapterName ? a->AdapterName : "", {}, {} });
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

			std::string line;
			for (auto line_range : content | std::views::split('\n'))
			{
				line = trimConfigLine(std::string_view{ std::ranges::data(line_range), std::ranges::size(line_range) });
				if (line.empty() || line.starts_with('#'))
					continue;

				// guid|ipv4|ipv6
				const size_t p1 = line.find('|');
				if (p1 == std::string::npos)
					continue;
				const size_t p2 = line.find('|', p1 + 1);

				AdapterDns entry;
				entry.guid		  = trimConfigLine(line.substr(0, p1));
				entry.nameserver  = trimConfigLine(line.substr(p1 + 1, p2 == std::string::npos ? std::string::npos : p2 - p1 - 1));
				entry.nameserver6 = (p2 == std::string::npos) ? std::string{} : trimConfigLine(line.substr(p2 + 1));
				out.push_back(std::move(entry));
			}

			return out;
		}

		void writeBackup(const std::filesystem::path& path, const std::vector<AdapterDns>& adapters, const std::string& listen)
		{
			std::string content	 = "# listen=" + listen + "\n";
			content				+= "# guid|ipv4|ipv6 (empty = automatic), written by unblock_dns\n";
			for (auto& a : adapters)
				content += a.guid + "|" + a.nameserver + "|" + a.nameserver6 + "\n";

			writeFileAtomic(path, content);
		}

		std::string readBackupListen(const std::filesystem::path& path)
		{
			std::string error;
			const auto	content = readFile(path, error);

			for (auto line_range : content | std::views::split('\n'))
			{
				const std::string line = trimConfigLine(std::string_view{ std::ranges::data(line_range), std::ranges::size(line_range) });
				if (line.starts_with("# listen="))
					return trimConfigLine(line.substr(9));
			}

			return {};
		}

		uint32_t setAdapterDns(AgBind& ag, const std::string& guid, const std::string& nameserver, bool ipv6)
		{
			return ag.set_if_nameserver(nameserver.c_str(), guid.c_str(), ipv6);
		}

		std::string currentAdapterDns(AgBind& ag, const std::string& guid, bool ipv6)
		{
			char* raw = ag.get_if_nameserver(guid.c_str(), ipv6);
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
			using FlushFn		   = BOOL(WINAPI*)();
			static FlushFn s_flush = nullptr;
			static bool	   s_tried = false;

			if (!s_tried)
			{
				s_tried = true;
				if (HMODULE dnsapi = LoadLibraryW(L"dnsapi.dll"))
				{
					FARPROC proc = GetProcAddress(dnsapi, "DnsFlushResolverCache");
					if (proc)
						std::memcpy(&s_flush, &proc, sizeof(proc));
				}
			}

			if (s_flush)
				s_flush();
		}
	}	 // namespace

	bool ipv6LoopbackAvailable()
	{
		SOCKET s = socket(AF_INET6, SOCK_DGRAM, 0);
		if (s == INVALID_SOCKET)
			return false;

		sockaddr_in6 addr{};
		addr.sin6_family = AF_INET6;
		addr.sin6_addr	 = in6addr_loopback;
		addr.sin6_port	 = 0;

		const bool ok = bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR;
		closesocket(s);
		return ok;
	}

	bool switchOsDns(AgBind& ag, const DnsProxyConfig& config, const std::filesystem::path& backup_path, bool ipv6)
	{
		auto adapters = listUpAdapters();

		std::vector<AdapterDns> stale = readBackup(backup_path);
		std::string				note  = stale.empty() ? "" : " (stale backup found, recovering originals)";

		logLine(
			"Switching " + std::to_string(adapters.size()) + " adapter(s) to " + config.listen
			+ (ipv6 ? " and " + std::string{ c_listen_ipv6 } : std::string{}) + note
		);

		for (auto& a : adapters)
		{
			std::string current = currentAdapterDns(ag, a.guid, false);

			if (current == config.listen && !stale.empty())
			{
				for (auto& s : stale)
					if (s.guid == a.guid)
					{
						current = s.nameserver;
						break;
					}
			}
			// Adapter already points at us but no backup survived (hard kill
			// after a manual stop): fall back to automatic instead of
			// restoring our own address as if it were the original.
			if (current == config.listen)
				current.clear();

			a.nameserver = current;

			if (uint32_t err = setAdapterDns(ag, a.guid, config.listen, false))
				logLine("Couldn't set IPv4 DNS on [" + a.guid + "], error " + std::to_string(err));

			// IPv6 too: Windows prefers router-advertised IPv6 DNS over our
			// IPv4 127.0.0.1 and would sweep every query past the proxy.
			if (ipv6)
			{
				std::string current6 = currentAdapterDns(ag, a.guid, true);

				if (current6 == c_listen_ipv6 && !stale.empty())
				{
					for (auto& s : stale)
						if (s.guid == a.guid)
						{
							current6 = s.nameserver6;
							break;
						}
				}
				if (current6 == c_listen_ipv6)
					current6.clear();

				a.nameserver6 = current6;

				if (uint32_t err = setAdapterDns(ag, a.guid, c_listen_ipv6, true))
					logLine("Couldn't set IPv6 DNS on [" + a.guid + "], error " + std::to_string(err));
			}
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
		{
			if (uint32_t err = setAdapterDns(ag, a.guid, a.nameserver, false))
			{
				logLine("Couldn't restore IPv4 DNS on [" + a.guid + "], error " + std::to_string(err));
				failed = true;
			}
			if (uint32_t err = setAdapterDns(ag, a.guid, a.nameserver6, true))
			{
				logLine("Couldn't restore IPv6 DNS on [" + a.guid + "], error " + std::to_string(err));
				failed = true;
			}
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

	int repairAdapters(AgBind& ag, const std::filesystem::path& backup_path, bool ipv6)
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
			if (!listen.empty() && currentAdapterDns(ag, a.guid, false) != listen)
				continue;

			if (uint32_t err = setAdapterDns(ag, a.guid, a.nameserver, false))
			{
				logLine("Couldn't restore IPv4 DNS on [" + a.guid + "], error " + std::to_string(err));
				failed = true;
			}
			else
				restored = true;

			// IPv6 was switched alongside IPv4 (empty = back to automatic).
			if (ipv6)
				setAdapterDns(ag, a.guid, a.nameserver6, true);
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
}	 // namespace dns
