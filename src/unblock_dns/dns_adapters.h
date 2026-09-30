#pragma once

#include "ag_bind.h"
#include "dns_config.h"

#include <filesystem>

namespace dns
{
	// True when the IPv6 loopback is usable (socket() can bind ::1). Guards
	// both the extra listener and the IPv6 DNS switch on IPv4-only hosts.
	bool ipv6LoopbackAvailable();

	// Switches all up adapters to the proxy. ipv6 also points adapter IPv6
	// DNS at ::1. A stale backup (previous run killed hard) is used to
	// recover true originals when the adapters still point at our own listen
	// address. Returns false when no adapter was switched.
	bool switchOsDns(AgBind& ag, const DnsProxyConfig& config, const std::filesystem::path& backup_path, bool ipv6);

	// Restores the adapters recorded in the backup and drops it on success.
	void restoreOsDns(AgBind& ag, const std::filesystem::path& backup_path);

	// Boot recovery: a hard-killed run left adapters on our listen address.
	// Only adapters still pointing at that address are restored. Returns 0 on
	// success, 1 when a restore failed.
	int repairAdapters(AgBind& ag, const std::filesystem::path& backup_path, bool ipv6);
}	 // namespace dns
