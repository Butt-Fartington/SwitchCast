#include "h264_low_delay.h"

#include <string.h>

static const uint8_t GrcSps[] = {
	0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
	0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE0, 0x80,
};

static const uint8_t LowDelaySps[] = {
	0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
	0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE1, 0xB4,
	0x11, 0x08, 0xD4,
};

bool H264CopyKnownLowDelaySps(
	uint8_t* destination,
	size_t capacity,
	size_t* destinationSize,
	const uint8_t* source,
	size_t sourceSize)
{
	if (
		!destination ||
		!destinationSize ||
		!source ||
		sourceSize != sizeof(GrcSps) ||
		memcmp(source, GrcSps, sizeof(GrcSps)) != 0 ||
		capacity < sizeof(LowDelaySps))
		return false;

	memcpy(destination, LowDelaySps, sizeof(LowDelaySps));
	*destinationSize = sizeof(LowDelaySps);
	return true;
}

static bool HasAnnexBStartCode(
	const uint8_t* data,
	size_t nalOffset)
{
	if (
		nalOffset >= 3 &&
		data[nalOffset - 3] == 0 &&
		data[nalOffset - 2] == 0 &&
		data[nalOffset - 1] == 1)
		return true;
	return
		nalOffset >= 4 &&
		data[nalOffset - 4] == 0 &&
		data[nalOffset - 3] == 0 &&
		data[nalOffset - 2] == 0 &&
		data[nalOffset - 1] == 1;
}

static bool IsNalBoundary(
	const uint8_t* data,
	size_t size,
	size_t offset)
{
	if (offset == size)
		return true;
	if (offset + 3 > size)
		return false;
	if (
		data[offset] == 0 &&
		data[offset + 1] == 0 &&
		data[offset + 2] == 1)
		return true;
	return
		offset + 4 <= size &&
		data[offset] == 0 &&
		data[offset + 1] == 0 &&
		data[offset + 2] == 0 &&
		data[offset + 3] == 1;
}

size_t H264RewriteAnnexBLowDelaySps(
	uint8_t* data,
	size_t* size,
	size_t capacity)
{
	if (!data || !size || *size > capacity)
		return 0;

	size_t rewritten = 0;
	for (
		size_t offset = 0;
		offset + sizeof(GrcSps) <= *size;
		++offset) {
		if (
			!HasAnnexBStartCode(data, offset) ||
			memcmp(data + offset, GrcSps, sizeof(GrcSps)) != 0 ||
			!IsNalBoundary(
				data,
				*size,
				offset + sizeof(GrcSps)))
			continue;

		const size_t growth =
			sizeof(LowDelaySps) - sizeof(GrcSps);
		if (growth > capacity - *size)
			continue;
		memmove(
			data + offset + sizeof(LowDelaySps),
			data + offset + sizeof(GrcSps),
			*size - offset - sizeof(GrcSps));
		memcpy(data + offset, LowDelaySps, sizeof(LowDelaySps));
		*size += growth;
		++rewritten;
		offset += sizeof(LowDelaySps) - 1;
	}
	return rewritten;
}
