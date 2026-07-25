#include "cast_streaming.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static uint16_t ReadBe16(const uint8_t* data)
{
	return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t ReadBe32(const uint8_t* data)
{
	return ((uint32_t)data[0] << 24) |
		((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) |
		data[3];
}

static void WriteBe16(uint8_t* output, uint16_t value)
{
	output[0] = (uint8_t)(value >> 8);
	output[1] = (uint8_t)value;
}

static void WriteBe32(uint8_t* output, uint32_t value)
{
	output[0] = (uint8_t)(value >> 24);
	output[1] = (uint8_t)(value >> 16);
	output[2] = (uint8_t)(value >> 8);
	output[3] = (uint8_t)value;
}

static void WriteBe64(uint8_t* output, uint64_t value)
{
	WriteBe32(output, (uint32_t)(value >> 32));
	WriteBe32(output + 4, (uint32_t)value);
}

static bool JsonFindValue(const char* json, const char* key, const char** value)
{
	char pattern[64];
	const int length = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	if (length <= 0 || (size_t)length >= sizeof(pattern))
		return false;

	const char* cursor = strstr(json, pattern);
	if (!cursor)
		return false;
	cursor += (size_t)length;
	while (*cursor && isspace((unsigned char)*cursor))
		++cursor;
	if (*cursor++ != ':')
		return false;
	while (*cursor && isspace((unsigned char)*cursor))
		++cursor;
	*value = cursor;
	return true;
}

static bool JsonReadUnsigned(const char* json, const char* key, uint32_t* value)
{
	const char* cursor;
	if (!JsonFindValue(json, key, &cursor) || !isdigit((unsigned char)*cursor))
		return false;

	uint64_t result = 0;
	while (isdigit((unsigned char)*cursor)) {
		result = result * 10U + (uint32_t)(*cursor++ - '0');
		if (result > UINT32_MAX)
			return false;
	}
	*value = (uint32_t)result;
	return true;
}

static bool JsonStringEquals(const char* json, const char* key, const char* expected)
{
	const char* cursor;
	if (!JsonFindValue(json, key, &cursor) || *cursor++ != '"')
		return false;

	const size_t expectedLength = strlen(expected);
	return strncmp(cursor, expected, expectedLength) == 0 &&
		cursor[expectedLength] == '"';
}

static size_t JsonReadUnsignedArray(
	const char* json,
	const char* key,
	uint32_t* values,
	size_t capacity)
{
	const char* cursor;
	if (!JsonFindValue(json, key, &cursor) || *cursor++ != '[')
		return 0;

	size_t count = 0;
	for (;;) {
		while (*cursor && isspace((unsigned char)*cursor))
			++cursor;
		if (*cursor == ']')
			return count;
		if (!isdigit((unsigned char)*cursor))
			return 0;

		uint64_t result = 0;
		while (isdigit((unsigned char)*cursor)) {
			result = result * 10U + (uint32_t)(*cursor++ - '0');
			if (result > UINT32_MAX)
				return 0;
		}
		if (count < capacity)
			values[count] = (uint32_t)result;
		++count;

		while (*cursor && isspace((unsigned char)*cursor))
			++cursor;
		if (*cursor == ']')
			return count;
		if (*cursor++ != ',')
			return 0;
	}
}

static void HexEncode16(const uint8_t input[16], char output[33])
{
	static const char Hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < 16; ++i) {
		output[i * 2] = Hex[input[i] >> 4];
		output[i * 2 + 1] = Hex[input[i] & 0x0F];
	}
	output[32] = '\0';
}

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
	uint32_t targetDelayMs)
{
	if (!output || outputCapacity == 0 || !aesKey || !aesIvMask ||
		!codecParameter || width == 0 || height == 0 || maxBitRate == 0 ||
		targetDelayMs == 0)
		return 0;

	char keyHex[33];
	char ivHex[33];
	HexEncode16(aesKey, keyHex);
	HexEncode16(aesIvMask, ivHex);

	const int length = snprintf(
		output,
		outputCapacity,
		"{\"type\":\"OFFER\",\"seqNum\":%u,\"offer\":{"
		"\"castMode\":\"mirroring\",\"supportedStreams\":[{"
		"\"index\":0,\"type\":\"video_source\",\"channels\":1,"
		"\"codecName\":\"h264\",\"codecParameter\":\"%s\","
		"\"rtpProfile\":\"cast\",\"rtpPayloadType\":%u,\"ssrc\":%u,"
		"\"targetDelay\":%u,\"aesKey\":\"%s\",\"aesIvMask\":\"%s\","
		"\"receiverRtcpEventLog\":false,\"timeBase\":\"1/90000\","
		"\"maxFrameRate\":\"30000/1000\",\"maxBitRate\":%u,"
		"\"protection\":\"\",\"profile\":\"high\",\"level\":\"3.2\","
		"\"errorRecoveryMode\":\"\",\"resolutions\":["
		"{\"width\":%u,\"height\":%u}]}]}}",
		sequenceNumber,
		codecParameter,
		CAST_STREAM_RTP_PAYLOAD_TYPE,
		senderSsrc,
		targetDelayMs,
		keyHex,
		ivHex,
		maxBitRate,
		width,
		height);
	if (length <= 0 || (size_t)length >= outputCapacity) {
		output[0] = '\0';
		return 0;
	}
	return (size_t)length;
}

bool CastStreamParseAnswer(
	const char* json,
	uint32_t expectedSequenceNumber,
	CastStreamAnswer* answer)
{
	if (!json || !answer)
		return false;
	memset(answer, 0, sizeof(*answer));

	uint32_t sequenceNumber;
	uint32_t udpPort;
	if (!JsonStringEquals(json, "type", "ANSWER") ||
		!JsonStringEquals(json, "result", "ok") ||
		!JsonReadUnsigned(json, "seqNum", &sequenceNumber) ||
		sequenceNumber != expectedSequenceNumber ||
		!JsonReadUnsigned(json, "udpPort", &udpPort) ||
		udpPort == 0 || udpPort > UINT16_MAX)
		return false;

	uint32_t indexes[8];
	uint32_t ssrcs[8];
	const size_t indexCount =
		JsonReadUnsignedArray(json, "sendIndexes", indexes, 8);
	const size_t ssrcCount = JsonReadUnsignedArray(json, "ssrcs", ssrcs, 8);
	if (indexCount == 0 || indexCount > 8 || indexCount != ssrcCount)
		return false;

	for (size_t i = 0; i < indexCount; ++i) {
		if (indexes[i] == 0) {
			answer->udpPort = (uint16_t)udpPort;
			answer->receiverSsrc = ssrcs[i];
			answer->accepted = answer->receiverSsrc != 0;
			(void)JsonReadUnsigned(json, "maxBitRate", &answer->maxBitRate);
			(void)JsonReadUnsigned(json, "maxDelay", &answer->maxDelayMs);
			return answer->accepted;
		}
	}
	return false;
}

size_t CastStreamPacketCount(size_t encryptedFrameSize)
{
	if (encryptedFrameSize == 0)
		return 1;
	return (encryptedFrameSize + CAST_STREAM_MAX_PAYLOAD_SIZE - 1) /
		CAST_STREAM_MAX_PAYLOAD_SIZE;
}

size_t CastStreamBuildRtpPacket(
	uint8_t output[CAST_STREAM_MAX_PACKET_SIZE],
	uint16_t* sequenceNumber,
	uint32_t senderSsrc,
	uint32_t frameId,
	uint32_t rtpTimestamp,
	bool keyFrame,
	const uint8_t* encryptedFrame,
	size_t encryptedFrameSize,
	uint16_t packetId)
{
	if (!output || !sequenceNumber ||
		(encryptedFrameSize != 0 && !encryptedFrame))
		return 0;

	const size_t packetCount = CastStreamPacketCount(encryptedFrameSize);
	if (packetCount > UINT16_MAX || packetId >= packetCount)
		return 0;

	const size_t payloadOffset = (size_t)packetId * CAST_STREAM_MAX_PAYLOAD_SIZE;
	size_t payloadSize = 0;
	if (payloadOffset < encryptedFrameSize) {
		payloadSize = encryptedFrameSize - payloadOffset;
		if (payloadSize > CAST_STREAM_MAX_PAYLOAD_SIZE)
			payloadSize = CAST_STREAM_MAX_PAYLOAD_SIZE;
	}

	const bool isLast = (size_t)packetId + 1 == packetCount;
	output[0] = 0x80;
	output[1] = (uint8_t)(CAST_STREAM_RTP_PAYLOAD_TYPE |
		(isLast ? 0x80U : 0U));
	WriteBe16(output + 2, (*sequenceNumber)++);
	WriteBe32(output + 4, rtpTimestamp);
	WriteBe32(output + 8, senderSsrc);
	output[12] = (uint8_t)(0x40U | (keyFrame ? 0x80U : 0U));
	output[13] = (uint8_t)frameId;
	WriteBe16(output + 14, packetId);
	WriteBe16(output + 16, (uint16_t)(packetCount - 1));
	output[18] = (uint8_t)(keyFrame ? frameId : frameId - 1U);

	if (payloadSize)
		memcpy(output + CAST_STREAM_HEADER_SIZE,
			encryptedFrame + payloadOffset, payloadSize);
	return CAST_STREAM_HEADER_SIZE + payloadSize;
}

size_t CastStreamBuildSenderReport(
	uint8_t output[28],
	uint32_t senderSsrc,
	uint64_t ntpTimestamp,
	uint32_t rtpTimestamp,
	uint32_t packetCount,
	uint32_t octetCount)
{
	if (!output)
		return 0;
	output[0] = 0x80;
	output[1] = 200;
	WriteBe16(output + 2, 6);
	WriteBe32(output + 4, senderSsrc);
	WriteBe64(output + 8, ntpTimestamp);
	WriteBe32(output + 16, rtpTimestamp);
	WriteBe32(output + 20, packetCount);
	WriteBe32(output + 24, octetCount);
	return 28;
}

static uint32_t ExpandLessThanOrEqual(uint32_t maximum, uint8_t truncated)
{
	int64_t expanded = (int64_t)(maximum & ~UINT32_C(0xFF)) | truncated;
	if (expanded > maximum)
		expanded -= 256;
	return expanded < 0 ? 0 : (uint32_t)expanded;
}

static uint32_t ExpandGreaterThan(uint32_t minimum, uint8_t truncated)
{
	uint64_t expanded = ((uint64_t)minimum & ~UINT64_C(0xFF)) | truncated;
	if (expanded <= minimum)
		expanded += 256;
	return expanded > UINT32_MAX ? UINT32_MAX : (uint32_t)expanded;
}

static void AddNack(
	CastStreamFeedback* feedback,
	uint32_t frameId,
	uint16_t packetId)
{
	if (feedback->nackCount >= CAST_STREAM_MAX_NACKS) {
		feedback->nackOverflow = true;
		return;
	}
	feedback->nacks[feedback->nackCount].frameId = frameId;
	feedback->nacks[feedback->nackCount].packetId = packetId;
	++feedback->nackCount;
}

bool CastStreamParseRtcp(
	const uint8_t* packet,
	size_t packetSize,
	uint32_t senderSsrc,
	uint32_t receiverSsrc,
	uint32_t latestFrameId,
	CastStreamFeedback* feedback)
{
	if (!packet || !feedback)
		return false;
	memset(feedback, 0, sizeof(*feedback));

	size_t offset = 0;
	while (offset < packetSize) {
		if (packetSize - offset < 4)
			return false;
		const uint8_t* current = packet + offset;
		if ((current[0] & 0xC0U) != 0x80U)
			return false;
		const size_t payloadSize = (size_t)ReadBe16(current + 2) * 4U;
		const size_t currentSize = 4U + payloadSize;
		if (currentSize > packetSize - offset)
			return false;

		const uint8_t type = current[1];
		const uint8_t subtype = current[0] & 0x1FU;
		const uint8_t* payload = current + 4;
		if (type == 206 && subtype == 1 && payloadSize >= 8) {
			if (ReadBe32(payload) == receiverSsrc &&
				ReadBe32(payload + 4) == senderSsrc)
				feedback->pictureLoss = true;
		} else if (type == 206 && subtype == 15 && payloadSize >= 16) {
			if (ReadBe32(payload) == receiverSsrc &&
				ReadBe32(payload + 4) == senderSsrc &&
				ReadBe32(payload + 8) == UINT32_C(0x43415354)) {
				const uint32_t checkpoint =
					ExpandLessThanOrEqual(latestFrameId, payload[12]);
				const size_t lossCount = payload[13];
				const size_t required = 16U + lossCount * 4U;
				if (required > payloadSize)
					return false;

				feedback->valid = true;
				feedback->checkpointFrameId = checkpoint;
				feedback->playoutDelayMs = ReadBe16(payload + 14);
				for (size_t i = 0; i < lossCount; ++i) {
					const uint8_t* loss = payload + 16U + i * 4U;
					const uint32_t frameId =
						ExpandGreaterThan(checkpoint, loss[0]);
					uint16_t packetId = ReadBe16(loss + 1);
					AddNack(feedback, frameId, packetId);
					uint8_t bitVector = loss[3];
					if (packetId != UINT16_MAX) {
						while (bitVector) {
							++packetId;
							if (bitVector & 1U)
								AddNack(feedback, frameId, packetId);
							bitVector >>= 1;
						}
					}
				}
			}
		}
		offset += currentSize;
	}
	return true;
}

void CastStreamBuildNonce(
	const uint8_t aesIvMask[16],
	uint32_t frameId,
	uint8_t nonce[16])
{
	memset(nonce, 0, 16);
	WriteBe32(nonce + 8, frameId);
	for (size_t i = 0; i < 16; ++i)
		nonce[i] ^= aesIvMask[i];
}
