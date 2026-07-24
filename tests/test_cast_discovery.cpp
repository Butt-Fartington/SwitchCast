#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../SwitchCastConfig/source/CastDiscovery.hpp"

namespace
{
	void ExpectName(
		const std::vector<std::uint8_t>& packet,
		const std::string& expected)
	{
		const auto actual = CastDiscovery::ParseFriendlyName(
			packet.data(),
			packet.size());
		if (actual != expected) {
			std::cerr << "expected name '" << expected
				<< "', got '" << actual << "'\n";
			std::exit(1);
		}
	}
}

int main()
{
	ExpectName({
		0x00, 0x00, 0x84, 0x00,
		0x0E, 'f', 'n', '=', 'L', 'i', 'v', 'i', 'n', 'g', ' ', 'R', 'o', 'o', 'm'
	}, "Living Room");

	ExpectName({
		0x08, 'm', 'd', '=', 'N', 'e', 's', 't', 0x00,
		0x09, 'f', 'n', '=', 'O', 'f', 'f', 'i', 'c', 'e', 0x00
	}, "Office");

	ExpectName({
		0x0A, 'f', 'n', '=', 'B', 'a', 'd', 0x01, 'N', 'a', 'm'
	}, "");

	ExpectName({
		0x12, 'f', 'n', '=', 'T', 'r', 'u', 'n', 'c', 'a', 't', 'e', 'd'
	}, "");

	std::cout << "cast discovery parser tests passed\n";
	return 0;
}
