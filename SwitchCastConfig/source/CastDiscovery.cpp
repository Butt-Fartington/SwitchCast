#include "CastDiscovery.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef WIN32
	#include <WinSock2.h>
	#include <WS2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <cerrno>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

namespace
{
	constexpr std::uint16_t MdnsPort = 5353;
	constexpr auto ScanDuration = std::chrono::seconds(3);
	constexpr auto QueryInterval = std::chrono::seconds(1);

	const std::uint8_t MdnsQuery[] = {
		0x00, 0x00, // transaction ID
		0x00, 0x00, // flags
		0x00, 0x01, // questions
		0x00, 0x00, // answer RRs
		0x00, 0x00, // authority RRs
		0x00, 0x00, // additional RRs
		0x0B, '_', 'g', 'o', 'o', 'g', 'l', 'e', 'c', 'a', 's', 't',
		0x04, '_', 't', 'c', 'p',
		0x05, 'l', 'o', 'c', 'a', 'l',
		0x00,
		0x00, 0x0C, // PTR
		0x00, 0x01  // IN
	};

#ifdef WIN32
	using SocketHandle = SOCKET;
	constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
	using SocketHandle = int;
	constexpr SocketHandle InvalidSocket = -1;
#endif

	SocketHandle DiscoverySocket = InvalidSocket;
	bool Scanning = false;
	std::chrono::steady_clock::time_point ScanEnds;
	std::chrono::steady_clock::time_point NextQuery;
	std::vector<CastDiscovery::Receiver> Receivers;

	void CloseSocket()
	{
		if (DiscoverySocket == InvalidSocket)
			return;

#ifdef WIN32
		closesocket(DiscoverySocket);
#else
		close(DiscoverySocket);
#endif
		DiscoverySocket = InvalidSocket;
	}

	bool MemoryContains(
		const std::uint8_t* data,
		std::size_t size,
		const char* needle)
	{
		const std::size_t needleSize = std::strlen(needle);
		if (needleSize == 0 || needleSize > size)
			return false;

		for (std::size_t i = 0; i <= size - needleSize; ++i)
			if (std::memcmp(data + i, needle, needleSize) == 0)
				return true;
		return false;
	}

	bool SetNonBlocking(SocketHandle socket)
	{
#ifdef WIN32
		u_long enabled = 1;
		return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
		const int flags = fcntl(socket, F_GETFL, 0);
		return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
	}

	bool WouldBlock()
	{
#ifdef WIN32
		const int error = WSAGetLastError();
		return error == WSAEWOULDBLOCK;
#else
		return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
	}

	void SendQuery()
	{
		if (DiscoverySocket == InvalidSocket)
			return;

		sockaddr_in destination = {};
		destination.sin_family = AF_INET;
		destination.sin_port = htons(MdnsPort);
		inet_pton(AF_INET, "224.0.0.251", &destination.sin_addr);

		sendto(
			DiscoverySocket,
			reinterpret_cast<const char*>(MdnsQuery),
			sizeof(MdnsQuery),
			0,
			reinterpret_cast<sockaddr*>(&destination),
			sizeof(destination));
	}

	void AddReceiver(const sockaddr_in& sender, const std::uint8_t* packet, std::size_t size)
	{
		const auto name = CastDiscovery::ParseFriendlyName(packet, size);
		if (name.empty() && !MemoryContains(packet, size, "_googlecast"))
			return;

		char address[INET_ADDRSTRLEN] = {};
		if (!inet_ntop(AF_INET, &sender.sin_addr, address, sizeof(address)))
			return;

		auto existing = std::find_if(
			Receivers.begin(),
			Receivers.end(),
			[&address](const CastDiscovery::Receiver& receiver) {
				return receiver.IpAddress == address;
			});

		if (existing != Receivers.end()) {
			if (!name.empty())
				existing->Name = name;
			return;
		}

		Receivers.push_back({
			name.empty() ? "Chromecast" : name,
			address
		});

		std::sort(
			Receivers.begin(),
			Receivers.end(),
			[](const CastDiscovery::Receiver& left, const CastDiscovery::Receiver& right) {
				if (left.Name == right.Name)
					return left.IpAddress < right.IpAddress;
				return left.Name < right.Name;
			});
	}
}

std::string CastDiscovery::ParseFriendlyName(
	const std::uint8_t* packet,
	std::size_t size)
{
	if (!packet)
		return {};

	// Chromecast TXT records expose their friendly name as a length-prefixed
	// "fn=<name>" entry. Searching only valid TXT-sized slices avoids treating
	// arbitrary payload bytes as a receiver name.
	for (std::size_t i = 0; i < size; ++i) {
		const std::size_t entrySize = packet[i];
		if (entrySize < 4 || i + 1 + entrySize > size)
			continue;
		if (std::memcmp(packet + i + 1, "fn=", 3) != 0)
			continue;

		const auto* name = packet + i + 4;
		const std::size_t nameSize = entrySize - 3;
		for (std::size_t n = 0; n < nameSize; ++n)
			if (name[n] < 0x20 || name[n] == 0x7F)
				return {};

		return std::string(
			reinterpret_cast<const char*>(name),
			nameSize);
	}

	return {};
}

bool CastDiscovery::Start()
{
	Stop();
	Receivers.clear();

	DiscoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (DiscoverySocket == InvalidSocket)
		return false;

	int enabled = 1;
	setsockopt(
		DiscoverySocket,
		SOL_SOCKET,
		SO_REUSEADDR,
		reinterpret_cast<const char*>(&enabled),
		sizeof(enabled));
#ifdef SO_REUSEPORT
	setsockopt(
		DiscoverySocket,
		SOL_SOCKET,
		SO_REUSEPORT,
		reinterpret_cast<const char*>(&enabled),
		sizeof(enabled));
#endif

	sockaddr_in bindAddress = {};
	bindAddress.sin_family = AF_INET;
	bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	bindAddress.sin_port = htons(MdnsPort);
	if (bind(
		DiscoverySocket,
		reinterpret_cast<sockaddr*>(&bindAddress),
		sizeof(bindAddress)) != 0) {
		CloseSocket();
		return false;
	}

	ip_mreq membership = {};
	inet_pton(AF_INET, "224.0.0.251", &membership.imr_multiaddr);
	membership.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(
		DiscoverySocket,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,
		reinterpret_cast<const char*>(&membership),
		sizeof(membership)) != 0 ||
		!SetNonBlocking(DiscoverySocket)) {
		CloseSocket();
		return false;
	}

	const auto now = std::chrono::steady_clock::now();
	ScanEnds = now + ScanDuration;
	NextQuery = now + QueryInterval;
	Scanning = true;
	SendQuery();
	return true;
}

void CastDiscovery::Poll()
{
	if (!Scanning)
		return;

	const auto now = std::chrono::steady_clock::now();
	if (now >= ScanEnds) {
		Stop();
		return;
	}

	if (now >= NextQuery) {
		SendQuery();
		NextQuery = now + QueryInterval;
	}

	for (;;) {
		std::uint8_t packet[2048];
		sockaddr_in sender = {};
#ifdef WIN32
		int senderSize = sizeof(sender);
#else
		socklen_t senderSize = sizeof(sender);
#endif
		const int received = recvfrom(
			DiscoverySocket,
			reinterpret_cast<char*>(packet),
			sizeof(packet),
			0,
			reinterpret_cast<sockaddr*>(&sender),
			&senderSize);
		if (received < 0) {
			if (!WouldBlock())
				Stop();
			break;
		}
		if (received == 0)
			break;

		AddReceiver(sender, packet, static_cast<std::size_t>(received));
	}
}

void CastDiscovery::Stop()
{
	Scanning = false;
	CloseSocket();
}

bool CastDiscovery::IsScanning()
{
	return Scanning;
}

const std::vector<CastDiscovery::Receiver>& CastDiscovery::GetReceivers()
{
	return Receivers;
}
