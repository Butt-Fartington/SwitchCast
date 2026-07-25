#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../sysmodule/source/cast/frame_arena.h"

static void TestAlignment(void)
{
	assert(FrameArenaAlignedSize(0) == 0);
	assert(FrameArenaAlignedSize(1) == 16);
	assert(FrameArenaAlignedSize(16) == 16);
	assert(FrameArenaAlignedSize(17) == 32);
	assert(FrameArenaAlignedSize(SIZE_MAX) == 0);
}

static void TestEmptyArena(void)
{
	size_t offset = SIZE_MAX;
	assert(FrameArenaFindFreeRegion(1024, NULL, 0, 100, &offset));
	assert(offset == 0);
	assert(!FrameArenaFindFreeRegion(64, NULL, 0, 65, &offset));
}

static void TestFragmentedArena(void)
{
	const FrameArenaSpan spans[] = {
		{256, 128},
		{0, 128},
		{512, 256}
	};
	size_t offset = SIZE_MAX;
	assert(FrameArenaFindFreeRegion(
		1024, spans, sizeof(spans) / sizeof(spans[0]), 100, &offset));
	assert(offset == 128);

	assert(FrameArenaFindFreeRegion(
		1024, spans, sizeof(spans) / sizeof(spans[0]), 200, &offset));
	assert(offset == 768);

	assert(!FrameArenaFindFreeRegion(
		1024, spans, sizeof(spans) / sizeof(spans[0]), 300, &offset));
}

static void TestAdjacentAndInvalidSpans(void)
{
	const FrameArenaSpan adjacent[] = {
		{0, 64},
		{64, 64}
	};
	size_t offset = SIZE_MAX;
	assert(FrameArenaFindFreeRegion(
		256, adjacent, sizeof(adjacent) / sizeof(adjacent[0]), 128, &offset));
	assert(offset == 128);

	const FrameArenaSpan invalid[] = {{240, 32}};
	assert(!FrameArenaFindFreeRegion(
		256, invalid, sizeof(invalid) / sizeof(invalid[0]), 16, &offset));
}

int main(void)
{
	TestAlignment();
	TestEmptyArena();
	TestFragmentedArena();
	TestAdjacentAndInvalidSpans();
	puts("frame_arena tests passed");
	return 0;
}
