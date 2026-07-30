#include "../sysmodule/source/cast/fmp4.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t ReadU32(const uint8_t* data)
{
	return ((uint32_t)data[0] << 24) |
		((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) |
		data[3];
}

static uint64_t ReadU64(const uint8_t* data)
{
	return ((uint64_t)ReadU32(data) << 32) | ReadU32(data + 4);
}

static uint64_t ReadTfdt(const uint8_t* data, size_t size)
{
	for (size_t i = 4; i + 16 <= size; ++i) {
		if (memcmp(data + i, "tfdt", 4) == 0) {
			assert(data[i + 4] == 1);
			return ReadU64(data + i + 8);
		}
	}
	assert(!"tfdt box not found");
	return UINT64_MAX;
}

static bool ContainsBox(const uint8_t* data, size_t size, const char type[4])
{
	for (size_t i = 4; i + 4 <= size; ++i) {
		if (memcmp(data + i, type, 4) == 0)
			return true;
	}
	return false;
}

static void TestCastLowDelaySps(void)
{
	static const uint8_t accessUnit[] = {
		0, 0, 0, 1,
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
		0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE0, 0x80,
		0, 0, 0, 1,
		0x68, 0xEE, 0x3C, 0xB0,
	};
	static const uint8_t expectedSps[] = {
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
		0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE1, 0xB4,
		0x11, 0x08, 0xD4,
	};

	Fmp4Stream stream;
	Fmp4Init(&stream);
	assert(Fmp4ObserveAccessUnit(
		&stream,
		accessUnit,
		sizeof(accessUnit)));
	assert(stream.spsSize == sizeof(expectedSps));
	assert(memcmp(
		stream.sps,
		expectedSps,
		sizeof(expectedSps)) == 0);
	assert(stream.ppsSize == 4);

	uint8_t init[2048];
	const size_t initSize = Fmp4BuildInitSegment(
		&stream,
		init,
		sizeof(init),
		1280,
		720);
	assert(initSize != 0);

	bool found = false;
	for (size_t i = 0; i + sizeof(expectedSps) <= initSize; ++i) {
		if (memcmp(
				init + i,
				expectedSps,
				sizeof(expectedSps)) == 0) {
			found = true;
			break;
		}
	}
	assert(found);
}

static void TestSynthetic(void)
{
	static const uint8_t accessUnit[] = {
		0, 0, 0, 1,
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0xD9, 0x40,
		0, 0, 0, 1,
		0x68, 0xEE, 0x3C, 0x80,
		0, 0, 1,
		0x09, 0x10,
		0, 0, 0, 1,
		0x65, 0x88, 0x84, 0x21, 0xA0
	};

	Fmp4Stream stream;
	Fmp4Init(&stream);
	assert(Fmp4ObserveAccessUnit(&stream, accessUnit, sizeof(accessUnit)));
	assert(stream.spsSize == 7);
	assert(stream.ppsSize == 4);

	char codec[FMP4_CODEC_STRING_SIZE];
	assert(Fmp4GetCodecString(&stream, codec, sizeof(codec)));
	assert(strcmp(codec, "avc1.640C20") == 0);

	uint8_t init[2048];
	const size_t initSize = Fmp4BuildInitSegment(
		&stream,
		init,
		sizeof(init),
		1280,
		720);
	assert(initSize > 500);
	assert(ReadU32(init) == 28);
	assert(memcmp(init + 4, "ftyp", 4) == 0);
	assert(ContainsBox(init, initSize, "moov"));
	assert(ContainsBox(init, initSize, "avcC"));
	assert(!Fmp4BuildInitSegment(
		&stream,
		init,
		32,
		1280,
		720));

	Fmp4BeginMedia(&stream);
	uint8_t fragment[2048];
	const size_t fragmentSize = Fmp4BuildFragment(
		&stream,
		accessUnit,
		sizeof(accessUnit),
		123456,
		true,
		fragment,
		sizeof(fragment));
	assert(fragmentSize == 100 + 8 + 4 + 2 + 4 + 5);
	assert(ReadU32(fragment) == 100);
	assert(memcmp(fragment + 4, "moof", 4) == 0);
	assert(memcmp(fragment + 104, "mdat", 4) == 0);
	assert(stream.nextSequence == 2);
	assert(ReadTfdt(fragment, fragmentSize) == 0);

	// SPS and PPS are in avcC, not repeated in the AVC media sample.
	const uint8_t* sample = fragment + 108;
	assert(ReadU32(sample) == 2);
	assert((sample[4] & 0x1F) == 9);
	assert(ReadU32(sample + 6) == 5);
	assert((sample[10] & 0x1F) == 5);

	// Small GRC timestamp jitter must not create overlapping MP4 samples.
	const size_t secondSize = Fmp4BuildFragment(
		&stream,
		accessUnit,
		sizeof(accessUnit),
		156745,
		false,
		fragment,
		sizeof(fragment));
	assert(secondSize);
	assert(ReadTfdt(fragment, secondSize) == FMP4_DEFAULT_SAMPLE_DURATION);

	const size_t thirdSize = Fmp4BuildFragment(
		&stream,
		accessUnit,
		sizeof(accessUnit),
		190067,
		false,
		fragment,
		sizeof(fragment));
	assert(thirdSize);
	assert(
		ReadTfdt(fragment, thirdSize) ==
		2 * FMP4_DEFAULT_SAMPLE_DURATION);
}

static uint8_t* ReadFile(const char* path, size_t* size)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return NULL;
	assert(fseek(file, 0, SEEK_END) == 0);
	const long length = ftell(file);
	assert(length > 0);
	assert(fseek(file, 0, SEEK_SET) == 0);
	uint8_t* data = malloc((size_t)length);
	assert(data);
	assert(fread(data, 1, (size_t)length, file) == (size_t)length);
	fclose(file);
	*size = (size_t)length;
	return data;
}

static void ConvertAnnexBFile(
	const char* inputPath,
	const char* outputPath)
{
	size_t inputSize = 0;
	uint8_t* input = ReadFile(inputPath, &inputSize);
	assert(input);

	Fmp4Stream stream;
	Fmp4Init(&stream);
	assert(Fmp4ObserveAccessUnit(&stream, input, inputSize));

	uint8_t init[2048];
	const size_t initSize = Fmp4BuildInitSegment(
		&stream,
		init,
		sizeof(init),
		1280,
		720);
	assert(initSize);

	uint8_t* fragment = malloc(inputSize + 256);
	assert(fragment);
	const size_t fragmentSize = Fmp4BuildFragment(
		&stream,
		input,
		inputSize,
		0,
		true,
		fragment,
		inputSize + 256);
	assert(fragmentSize);

	FILE* output = fopen(outputPath, "wb");
	assert(output);
	assert(fwrite(init, 1, initSize, output) == initSize);
	assert(fwrite(fragment, 1, fragmentSize, output) == fragmentSize);
	fclose(output);
	free(fragment);
	free(input);
}

int main(int argc, char** argv)
{
	TestCastLowDelaySps();
	TestSynthetic();
	if (argc == 3)
		ConvertAnnexBFile(argv[1], argv[2]);
	puts("fMP4 tests passed");
	return 0;
}
