#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Copies the known GRC SPS with low-delay VUI restrictions. Returns false
 * when the source SPS is not the exact known encoder configuration.
 */
bool H264CopyKnownLowDelaySps(
	uint8_t* destination,
	size_t capacity,
	size_t* destinationSize,
	const uint8_t* source,
	size_t sourceSize);

/*
 * Rewrites exact known GRC SPS NALs inside an Annex-B access unit in place.
 * Returns the number of rewritten NALs and updates size when an SPS grows.
 */
size_t H264RewriteAnnexBLowDelaySps(
	uint8_t* data,
	size_t* size,
	size_t capacity);
