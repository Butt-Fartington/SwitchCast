#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
	size_t offset;
	size_t size;
} FrameArenaSpan;

// Returns the 16-byte-aligned allocation size, or zero on overflow/empty input.
size_t FrameArenaAlignedSize(size_t size);

// Finds the first contiguous region that does not overlap an active span.
// Spans may be supplied in any order.
bool FrameArenaFindFreeRegion(
	size_t capacity,
	const FrameArenaSpan* spans,
	size_t spanCount,
	size_t requestedSize,
	size_t* offset);
