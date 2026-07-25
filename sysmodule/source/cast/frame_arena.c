#include "frame_arena.h"

#include <stdint.h>

#define FRAME_ARENA_ALIGNMENT 16U

size_t FrameArenaAlignedSize(size_t size)
{
	if (size == 0 || size > SIZE_MAX - (FRAME_ARENA_ALIGNMENT - 1U))
		return 0;
	return (size + FRAME_ARENA_ALIGNMENT - 1U) &
		~(size_t)(FRAME_ARENA_ALIGNMENT - 1U);
}

bool FrameArenaFindFreeRegion(
	size_t capacity,
	const FrameArenaSpan* spans,
	size_t spanCount,
	size_t requestedSize,
	size_t* offset)
{
	if (!offset || (spanCount && !spans))
		return false;
	const size_t allocationSize = FrameArenaAlignedSize(requestedSize);
	if (allocationSize == 0 || allocationSize > capacity)
		return false;

	size_t candidate = 0;
	while (candidate <= capacity - allocationSize) {
		const size_t candidateEnd = candidate + allocationSize;
		size_t nextCandidate = candidate;
		bool overlaps = false;

		for (size_t i = 0; i < spanCount; ++i) {
			if (spans[i].size == 0)
				continue;
			if (spans[i].offset > capacity ||
				spans[i].size > capacity - spans[i].offset)
				return false;
			const size_t spanEnd = spans[i].offset + spans[i].size;
			if (candidate < spanEnd &&
				spans[i].offset < candidateEnd) {
				overlaps = true;
				if (spanEnd > nextCandidate)
					nextCandidate = spanEnd;
			}
		}

		if (!overlaps) {
			*offset = candidate;
			return true;
		}
		const size_t alignedCandidate =
			FrameArenaAlignedSize(nextCandidate);
		if (alignedCandidate == 0 || alignedCandidate <= candidate)
			return false;
		candidate = alignedCandidate;
	}
	return false;
}
