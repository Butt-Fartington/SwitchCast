#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAST_STREAMING_APP_ID "0F5096E8"
#define CAST_STREAMING_NAMESPACE "urn:x-cast:com.google.cast.webrtc"

#define CAST_STREAM_RTP_PAYLOAD_TYPE 101U
#define CAST_STREAM_MAX_PACKET_SIZE 1472U
#define CAST_STREAM_HEADER_SIZE 19U
#define CAST_STREAM_MAX_HEADER_SIZE 23U
#define CAST_STREAM_MAX_PAYLOAD_SIZE \
	(CAST_STREAM_MAX_PACKET_SIZE - CAST_STREAM_MAX_HEADER_SIZE)
#define CAST_STREAM_MAX_NACKS 40U

typedef struct {
	uint16_t udpPort;
	uint32_t receiverSsrc;
	uint32_t maxBitRate;
	uint32_t maxDelayMs;
	bool accepted;
} CastStreamAnswer;

typedef struct {
	uint32_t frameId;
	uint16_t packetId;
} CastStreamNack;

typedef struct {
	bool valid;
	bool pictureLoss;
	uint32_t checkpointFrameId;
	uint16_t playoutDelayMs;
	size_t nackCount;
	CastStreamNack nacks[CAST_STREAM_MAX_NACKS];
} CastStreamFeedback;

// Builds the Cast Streaming OFFER for one Annex-B H.264 video stream.
size_t CastStreamBuildOffer(
	char* output,
	size_t outputCapacity,
	uint32_t sequenceNumber,
	uint32_t senderSsrc,
	const uint8_t aesKey[16],
	const uint8_t aesIvMask[16],
	const char* codecParameter,
	uint32_t width,
	uint32_t height,
	uint32_t maxBitRate,
	uint32_t targetDelayMs);

// Parses a Cast Streaming ANSWER and selects offered stream index zero.
bool CastStreamParseAnswer(
	const char* json,
	uint32_t expectedSequenceNumber,
	CastStreamAnswer* answer);

size_t CastStreamPacketCount(size_t encryptedFrameSize);

// Builds one Cast-specific RTP packet. sequenceNumber is incremented on success.
size_t CastStreamBuildRtpPacket(
	uint8_t output[CAST_STREAM_MAX_PACKET_SIZE],
	uint16_t* sequenceNumber,
	uint32_t senderSsrc,
	uint32_t frameId,
	uint32_t rtpTimestamp,
	bool keyFrame,
	const uint8_t* encryptedFrame,
	size_t encryptedFrameSize,
	uint16_t packetId);

// Builds a 28-byte RTCP Sender Report.
size_t CastStreamBuildSenderReport(
	uint8_t output[28],
	uint32_t senderSsrc,
	uint64_t ntpTimestamp,
	uint32_t rtpTimestamp,
	uint32_t packetCount,
	uint32_t octetCount);

// Parses compound RTCP and extracts Cast feedback, NACKs, and PLI.
bool CastStreamParseRtcp(
	const uint8_t* packet,
	size_t packetSize,
	uint32_t senderSsrc,
	uint32_t receiverSsrc,
	uint32_t latestFrameId,
	CastStreamFeedback* feedback);

// Derives the AES-CTR nonce required by Cast Streaming for a frame.
void CastStreamBuildNonce(
	const uint8_t aesIvMask[16],
	uint32_t frameId,
	uint8_t nonce[16]);
