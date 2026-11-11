export module common.net_state;

import std;

// Pure, Win32-free helpers answering one question: is a VPN/tunnel carrying
// the user's traffic right now? When it is, our DNS proxy must hand the
// adapters back their original resolvers: the tunnel resolves on its own, and
// our upstreams are pinned to the physical interface. The signal is the live
// tunnel adapter itself, not the default route: tun-mode VPNs (sing-box,
// WireGuard with a ruleset) often leave 0.0.0.0/0 on the physical adapter and
// capture only selected prefixes, so the default route says nothing. Kept free
// of Win32 types so it is unit-tested directly; the bridge in
// unblock_dns.dns_adapters feeds it real data.

export namespace net
{
	struct AdapterInfo
	{
		std::uint32_t if_index;
		std::uint32_t if_type;
		std::string	  name;
		std::string	  description;
		bool		  up{ false };
	};

	// True for a tunnel/virtual/point-to-point interface or one whose name
	// mentions a known VPN product. The type check alone misses WireGuard or
	// Wintun adapters that report an ordinary Ethernet type, and the service
	// pseudo-tunnels (Teredo, ISATAP, 6to4) are excluded explicitly.
	bool isTunnelAdapter(std::uint32_t if_type, std::string_view name, std::string_view description);

	// True when an up tunnel adapter exists: the tunnel is live and our DNS
	// must stay out of its way. On true, which receives that adapter's name.
	bool activeTunnel(const std::vector<AdapterInfo>& adapters, std::string* which = nullptr);
}
