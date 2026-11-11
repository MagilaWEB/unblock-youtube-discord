module common.net_state;

import std;

import ext.windows;

namespace net
{
	namespace
	{
		// IF_TYPE_* from ipifcons.h, kept as literals so the pure part below
		// does not depend on Win32 headers.
		inline constexpr std::uint32_t c_if_type_tunnel		  = 131;
		inline constexpr std::uint32_t c_if_type_prop_virtual = 53;
		inline constexpr std::uint32_t c_if_type_ppp		  = 23;

		// Service pseudo-interfaces carry a tunnel type on every Windows box
		// (Teredo, ISATAP, 6to4) while carrying no user traffic. Matching them
		// would make the DNS hand the adapters back on a clean system.
		inline constexpr std::string_view c_pseudo_keywords[]{ "teredo", "isatap", "6to4", "pseudo-interface" };

		// Substrings (ASCII, lower-case) that mark a VPN/tunnel adapter. Bare
		// "virtual" is deliberately absent: a Hyper-V virtual Ethernet adapter
		// is physical and must keep our DNS.
		inline constexpr std::string_view c_keywords[]{
			"tunnel",	 "vpn",		   "wireguard",	 "wintun",	  "openvpn", "tap-windows", "amnezia", "outline",	"shadowsocks",
			"clash",	 "sing-box",   "sing-tun",	 "singbox",	  "xray",	 "v2ray",		"hiddify", "tailscale", "zerotier",
			"nebula",	 "hamachi",	   "radmin",	 "softether", "l2tp",	 "ipsec",		"ikev2",   "nordvpn",	"proton",
			"surfshark", "windscribe", "expressvpn", "nekobox",	  "nekoray", "tun2socks",
		};

		std::string toLowerAscii(std::string_view s)
		{
			std::string out{ s };
			for (char& c : out)
				if (c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');
			return out;
		}

		bool containsAny(const std::string& text, const std::string_view* keywords, size_t count)
		{
			for (size_t i = 0; i < count; ++i)
				if (text.contains(keywords[i]))
					return true;

			return false;
		}

		// PWCHAR (FriendlyName/Description) to UTF-8 for matching.
		std::string narrow(const wchar_t* wide)
		{
			if (!wide || !*wide)
				return {};

			const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
			if (size <= 1)
				return {};

			std::string out(static_cast<size_t>(size), '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), size, nullptr, nullptr);
			out.resize(static_cast<size_t>(size - 1));

			return out;
		}
	}	 // namespace

	bool isTunnelAdapter(std::uint32_t if_type, std::string_view name, std::string_view description)
	{
		const std::string lower_name		= toLowerAscii(name);
		const std::string lower_description = toLowerAscii(description);

		constexpr auto pseudo_count = std::size(c_pseudo_keywords);
		if (containsAny(lower_name, c_pseudo_keywords, pseudo_count) || containsAny(lower_description, c_pseudo_keywords, pseudo_count))
			return false;

		if (if_type == c_if_type_tunnel || if_type == c_if_type_prop_virtual || if_type == c_if_type_ppp)
			return true;

		constexpr auto keyword_count = std::size(c_keywords);
		return containsAny(lower_name, c_keywords, keyword_count) || containsAny(lower_description, c_keywords, keyword_count);
	}

	bool activeTunnel(const std::vector<AdapterInfo>& adapters, std::string* which)
	{
		for (const auto& adapter : adapters)
		{
			if (!adapter.up || !isTunnelAdapter(adapter.if_type, adapter.name, adapter.description))
				continue;

			if (which)
				*which = adapter.name.empty() ? adapter.description : adapter.name;

			return true;
		}

		if (which)
			which->clear();

		return false;
	}

	bool vpnTunnelActive(std::string& which)
	{
		std::vector<AdapterInfo> adapters;

		ULONG buf_size = 0;
		GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, nullptr, &buf_size);
		if (buf_size > 0)
		{
			std::vector<std::uint8_t> buffer(buf_size);
			IP_ADAPTER_ADDRESSES*	  list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
			if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, list, &buf_size) == NO_ERROR)
				for (auto* a = list; a; a = a->Next)
					adapters.push_back(
						{ static_cast<std::uint32_t>(a->IfIndex),
						  static_cast<std::uint32_t>(a->IfType),
						  narrow(a->FriendlyName),
						  narrow(a->Description),
						  a->OperStatus == IfOperStatusUp }
					);
		}

		return net::activeTunnel(adapters, &which);
	}
}	 // namespace net
