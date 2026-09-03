#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <string_view>

#include "../net.h"

TEST_CASE("UdpSocket create/bind/nonBlocking on loopback", "[net][loopback]")
{
	Winsock	  winsock;
	UdpSocket sock;
	REQUIRE(sock.create());
	CHECK(sock.bind(0));
	CHECK(sock.nonBlocking());
}

TEST_CASE("UdpSocket send and receive roundtrip", "[net][loopback]")
{
	Winsock winsock;

	UdpSocket sender;
	UdpSocket receiver;
	REQUIRE(sender.create());
	REQUIRE(receiver.create());
	REQUIRE(receiver.bind(0));
	REQUIRE(receiver.nonBlocking());

	const u32 port = receiver.localPort();
	REQUIRE(port != 0);

	const std::string payload = "OK:google.com";
	sender.sendTo(payload, htonl(INADDR_LOOPBACK), port);

	char		buf[256]{};
	sockaddr_in from{};
	const auto	n = receiver.recvFrom(buf, static_cast<int>(sizeof(buf)) - 1, from);
	REQUIRE(n > 0);
	CHECK(std::string_view{ buf, static_cast<size_t>(n) } == payload);
}

TEST_CASE("UdpSocket recvFrom with no data returns <=0", "[net][loopback]")
{
	Winsock winsock;

	UdpSocket sock;
	REQUIRE(sock.create());
	REQUIRE(sock.bind(0));
	REQUIRE(sock.nonBlocking());

	sockaddr_in from{};
	CHECK(sock.recvFrom(reinterpret_cast<char*>(0x1), 1, from) <= 0);
}

TEST_CASE("UdpSocket sendTo without create is no-op", "[net][loopback]")
{
	Winsock	  winsock;
	UdpSocket sock;
	CHECK_NOTHROW(sock.sendTo("x", htonl(INADDR_LOOPBACK), 1));
}
