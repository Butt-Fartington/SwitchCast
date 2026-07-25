#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../sysmodule/source/cast/cast_streaming.h"

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

static void TestOfferAndAnswer(void)
{
	uint8_t key[16];
	uint8_t iv[16];
	for (size_t i = 0; i < 16; ++i) {
		key[i] = (uint8_t)i;
		iv[i] = (uint8_t)(0xF0U + i);
	}

	char offer[2048];
	const size_t size = CastStreamBuildOffer(
		offer, sizeof(offer), 7, UINT32_C(0x12345678), key, iv,
		"avc1.640C20", 1280, 720, 8000000, 90);
	assert(size > 0);
	assert(strstr(offer, "\"type\":\"OFFER\""));
	assert(strstr(offer, "\"castMode\":\"mirroring\""));
	assert(strstr(offer, "\"codecName\":\"h264\""));
	assert(strstr(offer, "\"aesKey\":\"000102030405060708090A0B0C0D0E0F\""));
	assert(strstr(offer, "\"targetDelay\":90"));

	assert(CastStreamBuildOffer(
		offer, sizeof(offer), 8, UINT32_C(0x12345678), key, iv,
		"avc1.640C20", 1280, 720, 8000000, 150) > 0);
	assert(strstr(offer, "\"targetDelay\":150"));

	const char answerJson[] =
		"{\"type\":\"ANSWER\",\"seqNum\":7,\"result\":\"ok\","
		"\"answer\":{\"udpPort\":2345,\"sendIndexes\":[0],"
		"\"ssrcs\":[305419896],\"constraints\":{\"video\":{"
		"\"maxBitRate\":12000000,\"maxDelay\":400}}}}";
	CastStreamAnswer answer;
	assert(CastStreamParseAnswer(answerJson, 7, &answer));
	assert(answer.accepted);
	assert(answer.udpPort == 2345);
	assert(answer.receiverSsrc == UINT32_C(0x12345678));
	assert(answer.maxBitRate == 12000000);
	assert(answer.maxDelayMs == 400);
	assert(!CastStreamParseAnswer(answerJson, 8, &answer));
}

static void TestRtp(void)
{
	uint8_t frame[3000];
	for (size_t i = 0; i < sizeof(frame); ++i)
		frame[i] = (uint8_t)i;

	assert(CastStreamPacketCount(sizeof(frame)) == 3);
	uint8_t packet[CAST_STREAM_MAX_PACKET_SIZE];
	uint16_t sequence = 42;
	const size_t first = CastStreamBuildRtpPacket(
		packet, &sequence, UINT32_C(0x10203040), 9, 90000, true,
		frame, sizeof(frame), 0);
	assert(first == CAST_STREAM_HEADER_SIZE + CAST_STREAM_MAX_PAYLOAD_SIZE);
	assert(packet[0] == 0x80);
	assert(packet[1] == CAST_STREAM_RTP_PAYLOAD_TYPE);
	assert(ReadBe16(packet + 2) == 42);
	assert(ReadBe32(packet + 4) == 90000);
	assert(ReadBe32(packet + 8) == UINT32_C(0x10203040));
	assert(packet[12] == 0xC0);
	assert(packet[13] == 9);
	assert(ReadBe16(packet + 14) == 0);
	assert(ReadBe16(packet + 16) == 2);
	assert(packet[18] == 9);
	assert(memcmp(packet + CAST_STREAM_HEADER_SIZE, frame,
		CAST_STREAM_MAX_PAYLOAD_SIZE) == 0);

	const size_t last = CastStreamBuildRtpPacket(
		packet, &sequence, UINT32_C(0x10203040), 10, 93000, false,
		frame, sizeof(frame), 2);
	assert(last == CAST_STREAM_HEADER_SIZE +
		sizeof(frame) - 2 * CAST_STREAM_MAX_PAYLOAD_SIZE);
	assert(packet[1] == (0x80U | CAST_STREAM_RTP_PAYLOAD_TYPE));
	assert(packet[12] == 0x40);
	assert(packet[18] == 9);
	assert(sequence == 44);
}

static void TestSenderReportAndNonce(void)
{
	uint8_t report[28];
	assert(CastStreamBuildSenderReport(
		report, UINT32_C(0x01020304), UINT64_C(0x1122334455667788),
		UINT32_C(0x90ABCDEF), 17, 12345) == sizeof(report));
	assert(report[0] == 0x80);
	assert(report[1] == 200);
	assert(ReadBe16(report + 2) == 6);
	assert(ReadBe32(report + 4) == UINT32_C(0x01020304));
	assert(ReadBe32(report + 8) == UINT32_C(0x11223344));
	assert(ReadBe32(report + 12) == UINT32_C(0x55667788));

	uint8_t iv[16];
	memset(iv, 0xA5, sizeof(iv));
	uint8_t nonce[16];
	CastStreamBuildNonce(iv, UINT32_C(0x12345678), nonce);
	assert(nonce[8] == (uint8_t)(0xA5 ^ 0x12));
	assert(nonce[9] == (uint8_t)(0xA5 ^ 0x34));
	assert(nonce[10] == (uint8_t)(0xA5 ^ 0x56));
	assert(nonce[11] == (uint8_t)(0xA5 ^ 0x78));
}

static void TestFeedback(void)
{
	uint8_t compound[40];
	memset(compound, 0, sizeof(compound));

	// PLI.
	compound[0] = 0x81;
	compound[1] = 206;
	WriteBe16(compound + 2, 2);
	WriteBe32(compound + 4, UINT32_C(0x11112222));
	WriteBe32(compound + 8, UINT32_C(0x33334444));

	// CAST feedback with two NACK words represented by one loss field.
	uint8_t* cast = compound + 12;
	cast[0] = 0x8F;
	cast[1] = 206;
	WriteBe16(cast + 2, 5);
	WriteBe32(cast + 4, UINT32_C(0x11112222));
	WriteBe32(cast + 8, UINT32_C(0x33334444));
	WriteBe32(cast + 12, UINT32_C(0x43415354));
	cast[16] = 0x01; // latest 0x101 -> checkpoint 0x101
	cast[17] = 1;
	WriteBe16(cast + 18, 125);
	cast[20] = 0x02; // frame 0x102
	WriteBe16(cast + 21, 3);
	cast[23] = 0x05; // packet IDs 4 and 6 are also missing

	CastStreamFeedback feedback;
	assert(CastStreamParseRtcp(
		compound, 36, UINT32_C(0x33334444), UINT32_C(0x11112222),
		UINT32_C(0x101), &feedback));
	assert(feedback.valid);
	assert(feedback.pictureLoss);
	assert(feedback.checkpointFrameId == UINT32_C(0x101));
	assert(feedback.playoutDelayMs == 125);
	assert(feedback.nackCount == 3);
	assert(feedback.nacks[0].frameId == UINT32_C(0x102));
	assert(feedback.nacks[0].packetId == 3);
	assert(feedback.nacks[1].packetId == 4);
	assert(feedback.nacks[2].packetId == 6);
	assert(!feedback.nackOverflow);
}

static void TestFeedbackOverflow(void)
{
	enum { LossCount = CAST_STREAM_MAX_NACKS + 1 };
	uint8_t packet[20 + LossCount * 4];
	memset(packet, 0, sizeof(packet));
	packet[0] = 0x8F;
	packet[1] = 206;
	WriteBe16(packet + 2, (uint16_t)(sizeof(packet) / 4 - 1));
	WriteBe32(packet + 4, UINT32_C(0x11112222));
	WriteBe32(packet + 8, UINT32_C(0x33334444));
	WriteBe32(packet + 12, UINT32_C(0x43415354));
	packet[16] = 0;
	packet[17] = LossCount;
	WriteBe16(packet + 18, 90);
	for (size_t i = 0; i < LossCount; ++i) {
		packet[20 + i * 4] = (uint8_t)(i + 1);
		WriteBe16(packet + 21 + i * 4, 0);
	}

	CastStreamFeedback feedback;
	assert(CastStreamParseRtcp(
		packet, sizeof(packet), UINT32_C(0x33334444),
		UINT32_C(0x11112222), UINT32_C(0x100), &feedback));
	assert(feedback.valid);
	assert(feedback.nackCount == CAST_STREAM_MAX_NACKS);
	assert(feedback.nackOverflow);
}

int main(void)
{
	TestOfferAndAnswer();
	TestRtp();
	TestSenderReportAndNonce();
	TestFeedback();
	TestFeedbackOverflow();
	puts("cast_streaming tests passed");
	return 0;
}
