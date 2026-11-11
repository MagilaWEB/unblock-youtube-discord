#include <catch2/catch_test_macros.hpp>

import std;
import common.net_state;

TEST_CASE("isTunnelAdapter flags tunnel interface types", "[net][dns]")
{
	// IF_TYPE_TUNNEL / IF_TYPE_PROP_VIRTUAL / IF_TYPE_PPP
	CHECK(net::isTunnelAdapter(131, "", ""));
	CHECK(net::isTunnelAdapter(53, "", ""));
	CHECK(net::isTunnelAdapter(23, "", ""));
}

TEST_CASE("isTunnelAdapter matches VPN adapter names", "[net][dns]")
{
	CHECK(net::isTunnelAdapter(6, "WireGuard Tunnel", ""));
	CHECK(net::isTunnelAdapter(6, "vEthernet", "Wintun Userspace Tunnel"));
	CHECK(net::isTunnelAdapter(6, "TAP-Windows Adapter V9", ""));
	CHECK(net::isTunnelAdapter(6, "AmneziaVPN", ""));
	CHECK(net::isTunnelAdapter(6, "", "OpenVPN Data Channel Offload"));
	// sing-box tun: the type is already virtual, the name only confirms it.
	CHECK(net::isTunnelAdapter(53, "tun_hglwuyxdy", "sing-tun Tunnel"));
}

TEST_CASE("isTunnelAdapter leaves physical adapters alone", "[net][dns]")
{
	CHECK_FALSE(net::isTunnelAdapter(6, "Ethernet", "Intel(R) Ethernet Connection I219-V"));
	CHECK_FALSE(net::isTunnelAdapter(6, "Wi-Fi", "Intel(R) Wireless-AC 9560"));
	// Bare "virtual" must not count: Hyper-V's adapter is physical.
	CHECK_FALSE(net::isTunnelAdapter(6, "vEthernet (Default Switch)", "Hyper-V Virtual Ethernet Adapter"));
	CHECK_FALSE(net::isTunnelAdapter(6, "Bluetooth Device (Personal Area Network)", "Bluetooth Device (PAN)"));
}

TEST_CASE("isTunnelAdapter ignores service pseudo-tunnels", "[net][dns]")
{
	// Teredo/ISATAP/6to4 are always present with a tunnel type on Windows and
	// carry no user traffic: matching them would disable our DNS everywhere.
	CHECK_FALSE(net::isTunnelAdapter(131, "Teredo Tunneling Pseudo-Interface", "Microsoft Teredo Tunneling Adapter"));
	CHECK_FALSE(net::isTunnelAdapter(131, "isatap.example.com", "Microsoft ISATAP Adapter"));
	CHECK_FALSE(net::isTunnelAdapter(131, "6to4 Adapter", "Microsoft 6to4 Adapter"));
}

TEST_CASE("activeTunnel needs a tunnel adapter that is up", "[net][dns]")
{
	const std::vector<net::AdapterInfo> up_tunnel{
		{ 9,  6,	"Ethernet 2", "Realtek PCIe GbE Family Controller", true },
		{ 4, 53, "tun_hglwuyxdy",					 "sing-tun Tunnel", true },
	};

	std::string which;
	CHECK(net::activeTunnel(up_tunnel, &which));
	CHECK(which == "tun_hglwuyxdy");
}

TEST_CASE("activeTunnel skips down tunnels and physical-only systems", "[net][dns]")
{
	// Idle VPN adapters sit in the list as disconnected; they must not make
	// the DNS step aside.
	const std::vector<net::AdapterInfo> down_tunnels{
		{  9,  6,					"Ethernet 2", "Realtek PCIe GbE Family Controller",	 true },
		{ 10, 53,		 "Local Area Connection",			  "TAP-Windows Adapter V9", false },
		{ 16, 53, "OpenVPN Data Channel Offload",		"OpenVPN Data Channel Offload", false },
	};

	std::string which;
	CHECK_FALSE(net::activeTunnel(down_tunnels, &which));
	CHECK(which.empty());

	const std::vector<net::AdapterInfo> physical_only{
		{ 9,  6,				  "Ethernet 2", "Realtek PCIe GbE Family Controller", true },
		{ 1, 24, "Loopback Pseudo-Interface 1",		 "Software Loopback Interface 1", true },
	};

	CHECK_FALSE(net::activeTunnel(physical_only, &which));
	CHECK(which.empty());
}

TEST_CASE("activeTunnel on empty input never matches", "[net][dns]")
{
	std::string which{ "stale" };
	CHECK_FALSE(net::activeTunnel({}, &which));
	CHECK(which.empty());
}
