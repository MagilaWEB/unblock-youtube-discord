export module common.net_state;

import std;

// Pure, Win32-free helpers answering one question: is a VPN/tunnel carrying
// the user's traffic right now? When it is, our DNS proxy must hand the
// adapters back their original resolvers: the tunnel resolves on its own, and
// our upstreams are pinned to the physical interface. Being up is not enough —
// Radmin VPN and similar LAN-overlay clients keep their adapter up with no
// route at all (user logs showed exactly that, with the bypass stopped for
// nothing). The signal is the routing table: the tunnel has to be the
// interface the OS picks for public traffic. The predicate pair below is pure
// so it is unit-tested directly; the Win32 query (vpnTunnelActive) lives here
// too because both the DNS service and the zapret helper need to ask the
// system the same question.

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

	// True when an up tunnel adapter is also on the path to the internet:
	// routed holds the interface indexes the OS picks for public traffic (see
	// routedIndexes in the implementation). On true, which receives that
	// adapter's name.
	bool activeTunnel(const std::vector<AdapterInfo>& adapters, std::span<const std::uint32_t> routed, std::string* which = nullptr);

	// Asks the system whether a tunnel adapter is up right now (GetAdapters
	// followed by activeTunnel). which receives the tunnel adapter's name for
	// the log. Shared by the DNS service and the zapret helper: both must step
	// aside while a VPN carries the traffic.
	bool vpnTunnelActive(std::string& which);
}
