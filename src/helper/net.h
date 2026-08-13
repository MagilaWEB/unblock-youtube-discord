#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <string_view>

#include "types.h"

/** Winsock init/cleanup RAII. */
class Winsock
{
	WSADATA _data{};

public:
	Winsock();
	~Winsock();
	Winsock(const Winsock&)			   = delete;
	Winsock& operator=(const Winsock&) = delete;
};

/** UDP socket with RAII lifecycle management. */
class UdpSocket
{
	SOCKET _fd{ INVALID_SOCKET };

public:
	UdpSocket() = default;
	~UdpSocket();
	UdpSocket(const UdpSocket&)			   = delete;
	UdpSocket& operator=(const UdpSocket&) = delete;

	/** Create the socket. */
	bool create();
	/** Bind the socket to a port on INADDR_ANY. */
	bool bind(u32 port) const;
	/** Switch the socket to non-blocking mode. */
	bool nonBlocking() const;
	/** Send a datagram to the given address. */
	void sendTo(std::string_view message, u32 ip, u32 port) const;
	/** Receive a datagram, returns its length (<=0 - no data). */
	int	 recvFrom(char* buffer, int buffer_size, sockaddr_in& from) const;
};
