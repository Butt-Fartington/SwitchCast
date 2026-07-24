#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FMP4_TIMESCALE 90000U
#define FMP4_DEFAULT_SAMPLE_DURATION 3000U
#define FMP4_MAX_SPS_SIZE 128U
#define FMP4_MAX_PPS_SIZE 128U
#define FMP4_CODEC_STRING_SIZE 16U

typedef struct {
	uint8_t sps[FMP4_MAX_SPS_SIZE];
	size_t spsSize;
	uint8_t pps[FMP4_MAX_PPS_SIZE];
	size_t ppsSize;
	uint32_t nextSequence;
	uint64_t mediaStartTimestampUs;
	uint64_t nextDecodeTime;
	bool mediaStarted;
} Fmp4Stream;

void Fmp4Init(Fmp4Stream* stream);

// Saves SPS/PPS NAL units carried in an Annex-B access unit. Returns true once
// both parameter sets are available for an initialization segment.
bool Fmp4ObserveAccessUnit(
	Fmp4Stream* stream,
	const uint8_t* annexB,
	size_t annexBSize);

bool Fmp4HasConfiguration(const Fmp4Stream* stream);

// Produces the RFC 6381 AVC codec string corresponding to the observed SPS.
bool Fmp4GetCodecString(
	const Fmp4Stream* stream,
	char* out,
	size_t capacity);

// Resets fragment sequence/timeline state while retaining SPS/PPS.
void Fmp4BeginMedia(Fmp4Stream* stream);

// Builds ftyp+moov suitable for a video-only fragmented MP4 MediaSource.
size_t Fmp4BuildInitSegment(
	const Fmp4Stream* stream,
	uint8_t* out,
	size_t capacity,
	uint16_t width,
	uint16_t height);

// Builds one moof+mdat fragment containing a single H.264 access unit.
// Annex-B start codes are replaced with four-byte AVC sample lengths and
// in-band SPS/PPS NALs are omitted because they are carried in avcC.
size_t Fmp4BuildFragment(
	Fmp4Stream* stream,
	const uint8_t* annexB,
	size_t annexBSize,
	uint64_t timestampUs,
	bool randomAccess,
	uint8_t* out,
	size_t capacity);
