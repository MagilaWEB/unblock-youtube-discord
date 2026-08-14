#include "net.h"

Winsock::Winsock()
{
	WSAStartup(MAKEWORD(2, 2), &_data);
}

Winsock::~Winsock()
{
	WSACleanup();
}

UdpSocket::~UdpSocket()
{
	if (_fd != INVALID_SOCKET)
		closesocket(_fd);
}

bool UdpSocket::create()
{
	_fd = socket(AF_INET, SOCK_DGRAM, 0);
	return _fd != INVALID_SOCKET;
}

bool UdpSocket::bind(u32 port) const
{
	sockaddr_in addr{};
	addr.sin_family		 = AF_INET;
	addr.sin_port		 = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	return ::bind(_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR;
}

bool UdpSocket::nonBlocking() const
{
	u_long mode = 1;
	return ioctlsocket(_fd, FIONBIO, &mode) != SOCKET_ERROR;
}

u32 UdpSocket::localPort() const
{
	if (_fd == INVALID_SOCKET)
		return 0;

	sockaddr_in addr{};
	int			addr_len = sizeof(addr);
	if (getsockname(_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0)
		return 0;

	return ntohs(addr.sin_port);
}

void UdpSocket::sendTo(std::string_view message, u32 ip, u32 port) const
{
	sockaddr_in addr{};
	addr.sin_family		 = AF_INET;
	addr.sin_port		 = htons(port);
	addr.sin_addr.s_addr = ip;
	sendto(_fd, message.data(), static_cast<int>(message.size()), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

int UdpSocket::recvFrom(char* buffer, int buffer_size, sockaddr_in& from) const
{
	int from_len = sizeof(from);
	return ::recvfrom(_fd, buffer, buffer_size, 0, reinterpret_cast<sockaddr*>(&from), &from_len);
}
