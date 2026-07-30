#include "../sysmodule/source/h264_low_delay.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t OriginalSps[] = {
	0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
	0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE0, 0x80,
};

static const uint8_t LowDelaySps[] = {
	0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
	0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE1, 0xB4,
	0x11, 0x08, 0xD4,
};

static void TestRawSps(void)
{
	uint8_t output[32];
	size_t outputSize = 0;
	assert(H264CopyKnownLowDelaySps(
		output,
		sizeof(output),
		&outputSize,
		OriginalSps,
		sizeof(OriginalSps)));
	assert(outputSize == sizeof(LowDelaySps));
	assert(memcmp(output, LowDelaySps, sizeof(LowDelaySps)) == 0);

	uint8_t unknown[sizeof(OriginalSps)];
	memcpy(unknown, OriginalSps, sizeof(unknown));
	unknown[1] ^= 1;
	assert(!H264CopyKnownLowDelaySps(
		output,
		sizeof(output),
		&outputSize,
		unknown,
		sizeof(unknown)));
	assert(!H264CopyKnownLowDelaySps(
		output,
		sizeof(LowDelaySps) - 1,
		&outputSize,
		OriginalSps,
		sizeof(OriginalSps)));
}

static void TestAnnexB(void)
{
	uint8_t accessUnit[128] = {
		0, 0, 0, 1,
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
		0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE0, 0x80,
		0, 0, 0, 1,
		0x68, 0xEE, 0x3C, 0xB0,
		0, 0, 1,
		0x65, 0x88, 0x84,
	};
	const size_t originalSize = 35;
	size_t size = originalSize;
	assert(H264RewriteAnnexBLowDelaySps(
		accessUnit,
		&size,
		sizeof(accessUnit)) == 1);
	assert(size == originalSize + 3);
	assert(memcmp(
		accessUnit + 4,
		LowDelaySps,
		sizeof(LowDelaySps)) == 0);
	assert(accessUnit[23] == 0);
	assert(accessUnit[26] == 1);
	assert(accessUnit[27] == 0x68);

	uint8_t constrained[sizeof(OriginalSps) + 4] = {
		0, 0, 0, 1,
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
		0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE0, 0x80,
	};
	uint8_t constrainedBefore[sizeof(constrained)];
	memcpy(constrainedBefore, constrained, sizeof(constrained));
	size = sizeof(constrained);
	assert(H264RewriteAnnexBLowDelaySps(
		constrained,
		&size,
		sizeof(constrained)) == 0);
	assert(size == sizeof(constrained));
	assert(memcmp(
		constrained,
		constrainedBefore,
		sizeof(constrained)) == 0);
}

int main(void)
{
	TestRawSps();
	TestAnnexB();
	puts("H.264 low-delay SPS tests passed");
	return 0;
}
