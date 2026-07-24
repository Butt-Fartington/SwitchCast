#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CastDiscovery
{
	struct Receiver
	{
		std::string Name;
		std::string IpAddress;
	};

	// Starts a three-second multicast DNS scan. Poll() is non-blocking so the
	// settings UI remains responsive while receivers reply.
	bool Start();
	void Poll();
	void Stop();
	bool IsScanning();
	const std::vector<Receiver>& GetReceivers();

	// Exposed for the host-side parser test.
	std::string ParseFriendlyName(const std::uint8_t* packet, std::size_t size);
}
