#if !defined(USB_ONLY)

#include "cast.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>
#include <switch/crypto/aes_ctr.h>
#include <switch/services/csrng.h>
#include <switch/services/ssl.h>

#include "cast_proto.h"
#include "cast_streaming.h"
#include "frame_arena.h"
#include "fmp4.h"
#include "../core.h"
#include "../modes/modes.h"
#include "../net/sockets.h"
#include "../third_party/nanoprintf.h"
#include "../util.h"

#define CAST_CONTROL_PORT 8009
#define MDNS_PORT 5353
#define CAST_TARGET_PATH "/config/switchcast/receiver_ip"
#define CAST_LATENCY_PROFILE_PATH "/config/switchcast/latency_profile"
#define CAST_DIAGNOSTIC_PATH "/config/switchcast/debug.json"
#define CAST_LAST_FAILURE_PATH "/config/switchcast/last-failure.json"
#define CAST_ERROR_PATH "/config/switchcast/error.json"

#define STREAM_HEAP_SIZE 0x200000U
#define STREAM_SLOT_COUNT 24U
#define STREAM_ULTRA_TARGET_DELAY_MS 90U
#define STREAM_STABLE_TARGET_DELAY_MS 150U
#define STREAM_ULTRA_PACING_GROUP 12U
#define STREAM_STABLE_PACING_GROUP 8U
#define STREAM_ULTRA_PACING_SLEEP_NS 100000L
#define STREAM_STABLE_PACING_SLEEP_NS 250000L
#define STREAM_ULTRA_IDLE_SLEEP_NS 250000L
#define STREAM_STABLE_IDLE_SLEEP_NS 1000000L
#define STREAM_MAX_BIT_RATE 8000000U
#define STREAM_SENDER_REPORT_INTERVAL_MS 500U
#define STREAM_DIAGNOSTIC_INTERVAL_SECONDS 5U
#define STREAM_FEEDBACK_STALL_MS 3000U

#define NS_CONNECTION "urn:x-cast:com.google.cast.tp.connection"
#define NS_HEARTBEAT "urn:x-cast:com.google.cast.tp.heartbeat"
#define NS_RECEIVER "urn:x-cast:com.google.cast.receiver"

typedef enum {
	StreamSlot_Free,
	StreamSlot_Ready,
	StreamSlot_Sending,
	StreamSlot_History
} StreamSlotState;

typedef struct {
	uint8_t* data;
	size_t size;
	size_t offset;
	size_t allocationSize;
	uint64_t order;
	uint64_t captureTick;
	uint32_t frameId;
	uint32_t rtpTimestamp;
	bool keyFrame;
	bool encrypted;
	StreamSlotState state;
} StreamSlot;

typedef enum {
	StreamSession_Stopped,
	StreamSession_GameplayStopped,
	StreamSession_Recover,
	StreamSession_Failed
} StreamSessionResult;

static atomic_bool Cast_Running = false;
atomic_bool Cast_ClientStreaming = false;
static atomic_uint CastStatus = CAST_STATUS_OFF;
static atomic_bool GameplayReady = false;
static atomic_ullong LastGameplayTick = 0;
static atomic_uint CaptureGeneration = 0;
static atomic_bool BlankScreenEnabled = false;
static atomic_bool SettingsVisible = false;
static bool ProcessMonitorInitialized;
static bool ScreenBlankingApplied;
static bool ScreenBlankedBySwitchCast;

static int CastSocket = SOCKET_INVALID;
static int UdpSocket = SOCKET_INVALID;
static struct sockaddr_in UdpDestination;
static u32 ReceiverAddress;

static SslContext TlsContext;
static SslConnection TlsConnection;
static bool TlsInitialized;
static bool TlsContextOpen;
static bool TlsConnectionOpen;
static bool CastControlError;
static u64 LastHeartbeatTick;
static char ActiveTransportId[CAST_PROTO_ID_MAX];
static char ActiveSessionId[CAST_PROTO_ID_MAX];

static Mutex StreamMutex;
static uint8_t* StreamBuffer;
static size_t StreamBufferSize;
static StreamSlot StreamSlots[STREAM_SLOT_COUNT];
static Fmp4Stream H264Config;
static bool StreamSessionAttached;
static bool StreamAwaitingIdr;
static uint64_t StreamNextOrder;

static uint8_t SessionAesKey[16];
static uint8_t SessionAesIvMask[16];
static uint32_t SenderSsrc;
static uint32_t ReceiverSsrc;
static uint16_t RtpSequenceNumber;
static uint16_t ReceiverUdpPort;
static uint32_t NextFrameId;
static uint32_t LatestFrameId;
static uint32_t LatestRtpTimestamp;
static u64 LatestFrameCaptureTick;
static u64 LastSenderReportTick;
static u64 LastDiagnosticTick;
static u64 SessionStartTick;
static u64 FirstFrameSentTick;
static u64 LastReceiverFeedbackTick;
static u64 LastCheckpointAdvanceTick;
static uint32_t OfferSequenceNumber;
static bool AnswerReceived;
static bool AnswerAccepted;
static bool ReceiverCheckpointSeen;
static bool StreamRecoveryRequested;
static uint32_t LatestReceiverCheckpointFrameId;
static const char* StreamRecoveryReason = "none";
static CastStreamAnswer SessionAnswer;

static atomic_uint QueuedFrames = 0;
static atomic_uint SentFrames = 0;
static atomic_uint DroppedFrames = 0;
static atomic_uint OversizedFrames = 0;
static atomic_uint RtpPackets = 0;
static atomic_uint RtpOctets = 0;
static atomic_uint RetransmitPackets = 0;
static atomic_uint RtcpPackets = 0;
static atomic_uint NackRequests = 0;
static atomic_uint PictureLossRequests = 0;
static atomic_uint UnrecoverableNacks = 0;
static atomic_uint HistoryEvictions = 0;
static atomic_uint RecoveryRestarts = 0;
static atomic_uint FeedbackStalls = 0;
static atomic_uint PeakRetainedFrames = 0;
static atomic_uint PeakRetainedBytes = 0;
static atomic_uint ReceiverPlayoutDelayMs = 0;
static atomic_uint TargetPlayoutDelayMs = STREAM_ULTRA_TARGET_DELAY_MS;

static const uint8_t MdnsQuery[] = {
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x01,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x0B, '_', 'g', 'o', 'o', 'g', 'l', 'e', 'c', 'a', 's', 't',
	0x04, '_', 't', 'c', 'p',
	0x05, 'l', 'o', 'c', 'a', 'l',
	0x00,
	0x00, 0x0C,
	0x00, 0x01
};

static void SaveDiagnostic(const char* reason);
static void RefreshLatencyProfile(void);

uint32_t Cast_GetStatus(void)
{
	return atomic_load(&CastStatus);
}

uint32_t Cast_GetTargetDelayMs(void)
{
	return atomic_load(&TargetPlayoutDelayMs);
}

uint32_t Cast_GetReceiverDelayMs(void)
{
	return atomic_load(&ReceiverPlayoutDelayMs);
}

bool Cast_GetBlankScreenEnabled(void)
{
	return atomic_load(&BlankScreenEnabled);
}

void Cast_SetBlankScreenEnabled(bool enabled)
{
	atomic_store(&BlankScreenEnabled, enabled);
}

void Cast_SetSettingsVisible(bool visible)
{
	atomic_store(&SettingsVisible, visible);
}

void Cast_UpdateConsoleScreenBlanking(bool videoActive)
{
	const bool shouldBlank =
		videoActive &&
		atomic_load(&BlankScreenEnabled) &&
		!atomic_load(&SettingsVisible);
	if (shouldBlank) {
		if (ScreenBlankingApplied)
			return;
		const UtilScreenModeResult result =
			UtilSetConsoleScreenMode(false);
		if (result == UtilScreenMode_Failed)
			return;
		ScreenBlankingApplied = true;
		ScreenBlankedBySwitchCast =
			result == UtilScreenMode_Changed;
		return;
	}

	if (!ScreenBlankingApplied)
		return;
	if (!ScreenBlankedBySwitchCast) {
		// The backlight was already off before SwitchCast touched it.
		ScreenBlankingApplied = false;
		return;
	}

	const UtilScreenModeResult result =
		UtilSetConsoleScreenMode(true);
	if (result == UtilScreenMode_Failed)
		return;
	ScreenBlankingApplied = false;
	ScreenBlankedBySwitchCast = false;
}

static void RestoreConsoleScreen(void)
{
	for (
		unsigned int attempt = 0;
		attempt < 3 && ScreenBlankingApplied;
		++attempt) {
		Cast_UpdateConsoleScreenBlanking(false);
		if (ScreenBlankingApplied)
			svcSleepThread(20E+6);
	}
}

static void SetCastStatus(uint32_t status)
{
	atomic_store(&CastStatus, status);
}

static bool IsControlFailureStatus(uint32_t status)
{
	switch (status) {
	case CAST_STATUS_CONTROL_ERROR:
	case CAST_STATUS_CONTROL_SEND_ERROR:
	case CAST_STATUS_CONTROL_POLL_ERROR:
	case CAST_STATUS_CONTROL_HEADER_ERROR:
	case CAST_STATUS_CONTROL_FRAME_TOO_LARGE:
	case CAST_STATUS_CONTROL_BODY_ERROR:
	case CAST_STATUS_CONTROL_DECODE_ERROR:
	case CAST_STATUS_CONTROL_ENCODE_ERROR:
	case CAST_STATUS_CONTROL_PENDING_ERROR:
	case CAST_STATUS_CONTROL_SOCKET_POLL_ERROR:
	case CAST_STATUS_CONTROL_SOCKET_CLOSED:
		return true;
	default:
		return false;
	}
}

uint32_t Cast_GetCaptureGeneration(void)
{
	return atomic_load(&CaptureGeneration);
}

void Cast_NotifyVideoStopped(void)
{
	atomic_store(&GameplayReady, false);
}

static bool HasRecentGameplay(void)
{
	if (!atomic_load(&GameplayReady))
		return false;
	const u64 lastTick = atomic_load(&LastGameplayTick);
	return lastTick != 0 &&
		armGetSystemTick() - lastTick < armGetSystemTickFreq() * 3;
}

static bool IsApplicationRunning(void)
{
	u64 processId = 0;
	return ProcessMonitorInitialized &&
		R_SUCCEEDED(pmdmntGetApplicationProcessId(&processId)) &&
		processId != 0;
}

static void FreeStreamSlotLocked(StreamSlot* slot)
{
	slot->data = NULL;
	slot->size = 0;
	slot->offset = 0;
	slot->allocationSize = 0;
	slot->order = 0;
	slot->captureTick = 0;
	slot->frameId = 0;
	slot->rtpTimestamp = 0;
	slot->keyFrame = false;
	slot->encrypted = false;
	slot->state = StreamSlot_Free;
}

static void ResetStreamQueueLocked(bool resetCodec)
{
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i)
		FreeStreamSlotLocked(&StreamSlots[i]);
	StreamNextOrder = 1;
	StreamAwaitingIdr = true;
	if (resetCodec)
		Fmp4Init(&H264Config);
}

static bool FindFreeFrameRegionLocked(
	size_t frameSize,
	size_t* offset,
	size_t* allocationSize)
{
	FrameArenaSpan spans[STREAM_SLOT_COUNT];
	size_t spanCount = 0;
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_Free)
			continue;
		spans[spanCount].offset = StreamSlots[i].offset;
		spans[spanCount].size = StreamSlots[i].allocationSize;
		++spanCount;
	}
	const size_t alignedSize = FrameArenaAlignedSize(frameSize);
	if (alignedSize == 0 ||
		!FrameArenaFindFreeRegion(
			StreamBufferSize,
			spans,
			spanCount,
			alignedSize,
			offset))
		return false;
	*allocationSize = alignedSize;
	return true;
}

static int FindFreeStreamSlotLocked(void)
{
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_Free)
			return (int)i;
	}
	return -1;
}

static bool EvictOldestHistoryLocked(void)
{
	int selected = -1;
	uint64_t oldestOrder = UINT64_MAX;
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_History &&
			StreamSlots[i].order < oldestOrder) {
			selected = (int)i;
			oldestOrder = StreamSlots[i].order;
		}
	}
	if (selected < 0)
		return false;
	FreeStreamSlotLocked(&StreamSlots[selected]);
	atomic_fetch_add(&HistoryEvictions, 1);
	return true;
}

static unsigned int DropReadyFramesLocked(void)
{
	unsigned int dropped = 0;
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_Ready) {
			FreeStreamSlotLocked(&StreamSlots[i]);
			++dropped;
		}
	}
	if (dropped)
		atomic_fetch_add(&DroppedFrames, dropped);
	return dropped;
}

static void UpdateRetainedHighWaterLocked(void)
{
	unsigned int retainedFrames = 0;
	unsigned int retainedBytes = 0;
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_Free)
			continue;
		++retainedFrames;
		retainedBytes += (unsigned int)StreamSlots[i].allocationSize;
	}
	if (retainedFrames > atomic_load(&PeakRetainedFrames))
		atomic_store(&PeakRetainedFrames, retainedFrames);
	if (retainedBytes > atomic_load(&PeakRetainedBytes))
		atomic_store(&PeakRetainedBytes, retainedBytes);
}

static void DetachStreamSession(void)
{
	mutexLock(&StreamMutex);
	StreamSessionAttached = false;
	ResetStreamQueueLocked(false);
	mutexUnlock(&StreamMutex);
	SocketClose(&UdpSocket);
}

static void ReleaseCaptureMemoryLocked(void)
{
	if (!StreamBuffer)
		return;
	void* heapAddress = NULL;
	const Result rc = svcSetHeapSize(&heapAddress, 0);
	if (R_FAILED(rc)) {
		LOG("SwitchCast heap release failed: %x\n", rc);
		return;
	}
	StreamBuffer = NULL;
	StreamBufferSize = 0;
	memset(StreamSlots, 0, sizeof(StreamSlots));
}

static void StopCaptureAndReleaseMemory(void)
{
	Cast_ClientStreaming = false;
	SocketClose(&UdpSocket);
	mutexLock(&StreamMutex);
	StreamSessionAttached = false;
	atomic_store(&GameplayReady, false);
	atomic_store(&LastGameplayTick, 0);
	atomic_fetch_add(&CaptureGeneration, 1);
	ResetStreamQueueLocked(true);
	ReleaseCaptureMemoryLocked();
	mutexUnlock(&StreamMutex);
}

static void ResetStatistics(void)
{
	atomic_store(&QueuedFrames, 0);
	atomic_store(&SentFrames, 0);
	atomic_store(&DroppedFrames, 0);
	atomic_store(&OversizedFrames, 0);
	atomic_store(&RtpPackets, 0);
	atomic_store(&RtpOctets, 0);
	atomic_store(&RetransmitPackets, 0);
	atomic_store(&RtcpPackets, 0);
	atomic_store(&NackRequests, 0);
	atomic_store(&PictureLossRequests, 0);
	atomic_store(&UnrecoverableNacks, 0);
	atomic_store(&HistoryEvictions, 0);
	atomic_store(&RecoveryRestarts, 0);
	atomic_store(&FeedbackStalls, 0);
	atomic_store(&PeakRetainedFrames, 0);
	atomic_store(&PeakRetainedBytes, 0);
	atomic_store(&ReceiverPlayoutDelayMs, 0);
}

static bool PrepareCaptureForGame(void)
{
	Cast_ClientStreaming = false;
	RefreshLatencyProfile();
	ResetStatistics();
	CoreWriteSdFile(CAST_DIAGNOSTIC_PATH, NULL, 0);
	CoreWriteSdFile(CAST_ERROR_PATH, NULL, 0);

	mutexLock(&StreamMutex);
	atomic_store(&GameplayReady, false);
	atomic_store(&LastGameplayTick, 0);
	atomic_fetch_add(&CaptureGeneration, 1);

	if (!StreamBuffer) {
		void* heapAddress = NULL;
		const Result rc = svcSetHeapSize(&heapAddress, STREAM_HEAP_SIZE);
		if (R_FAILED(rc) || !heapAddress) {
			LOG("SwitchCast heap allocation failed: %x\n", rc);
			mutexUnlock(&StreamMutex);
			SetCastStatus(CAST_STATUS_MEMORY_ERROR);
			return false;
		}
		StreamBuffer = heapAddress;
		StreamBufferSize = STREAM_HEAP_SIZE;
	}

	StreamSessionAttached = false;
	ResetStreamQueueLocked(true);
	mutexUnlock(&StreamMutex);

	Cast_ClientStreaming = true;
	return true;
}

static bool ReadTrimmedFile(const char* path, char* output, size_t capacity)
{
	if (!output || capacity < 2)
		return false;
	u64 length = 0;
	if (!CoreReadSdFile(path, output, capacity - 1, &length))
		return false;
	output[length] = '\0';
	while (length && (
		output[length - 1] == '\r' ||
		output[length - 1] == '\n' ||
		output[length - 1] == ' ' ||
		output[length - 1] == '\t'))
		output[--length] = '\0';
	return length != 0;
}

static void RefreshLatencyProfile(void)
{
	char profile[24];
	const bool stable =
		ReadTrimmedFile(
			CAST_LATENCY_PROFILE_PATH,
			profile,
			sizeof(profile)) &&
		strcmp(profile, "stable") == 0;
	atomic_store(
		&TargetPlayoutDelayMs,
		stable
			? STREAM_STABLE_TARGET_DELAY_MS
			: STREAM_ULTRA_TARGET_DELAY_MS);
}

static bool IsUltraLowLatency(void)
{
	return Cast_GetTargetDelayMs() == STREAM_ULTRA_TARGET_DELAY_MS;
}

static bool ReadConfiguredTarget(u32* output)
{
	char configuredIp[64];
	if (!ReadTrimmedFile(
		CAST_TARGET_PATH,
		configuredIp,
		sizeof(configuredIp)))
		return false;
	const u32 address = inet_addr(configuredIp);
	if (address == INADDR_NONE)
		return false;
	*output = address;
	LOG("SwitchCast target configured as %s\n", configuredIp);
	return true;
}

static bool MemoryContains(
	const uint8_t* haystack,
	size_t haystackSize,
	const char* needle)
{
	const size_t needleSize = strlen(needle);
	if (needleSize == 0 || needleSize > haystackSize)
		return false;
	for (size_t i = 0; i <= haystackSize - needleSize; ++i) {
		if (memcmp(haystack + i, needle, needleSize) == 0)
			return true;
	}
	return false;
}

static bool DiscoverTarget(u32* output)
{
	if (ReadConfiguredTarget(output))
		return true;

	int socket = SocketUdp();
	if (socket == SOCKET_INVALID)
		return false;
	SocketSetReuseAddress(socket, true);

	struct sockaddr_in bindAddress;
	memset(&bindAddress, 0, sizeof(bindAddress));
	bindAddress.sin_family = AF_INET;
	bindAddress.sin_addr.s_addr = INADDR_ANY;
	bindAddress.sin_port = htons(MDNS_PORT);
	const u32 multicastAddress = inet_addr("224.0.0.251");
	if (!SocketBind(socket, (struct sockaddr*)&bindAddress, sizeof(bindAddress)) ||
		!SocketJoinMulticast(socket, multicastAddress)) {
		SocketClose(&socket);
		return false;
	}

	struct sockaddr_in destination;
	memset(&destination, 0, sizeof(destination));
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = multicastAddress;
	destination.sin_port = htons(MDNS_PORT);
	if (!SocketUDPSendTo(
		socket,
		MdnsQuery,
		sizeof(MdnsQuery),
		(struct sockaddr*)&destination,
		sizeof(destination))) {
		SocketClose(&socket);
		return false;
	}

	for (int i = 0; i < 12 && Cast_Running; ++i) {
		if (!SocketWaitReadable(socket, 250))
			continue;
		struct sockaddr_in sender;
		socklen_t senderLength = sizeof(sender);
		const s32 received = SocketUDPRecvFrom(
			socket,
			Buffers.CastMode.MdnsResponse,
			sizeof(Buffers.CastMode.MdnsResponse),
			(struct sockaddr*)&sender,
			&senderLength);
		if (received > 0 && MemoryContains(
			Buffers.CastMode.MdnsResponse,
			(size_t)received,
			"_googlecast")) {
			*output = sender.sin_addr.s_addr;
			SocketClose(&socket);
			return true;
		}
	}
	SocketClose(&socket);
	return false;
}

static bool TlsWriteAll(const void* data, u32 size)
{
	const uint8_t* current = data;
	while (size) {
		u32 written = 0;
		const Result rc = sslConnectionWrite(
			&TlsConnection,
			current,
			size,
			&written);
		if (R_FAILED(rc) || written == 0) {
			CastControlError = true;
			SetCastStatus(CAST_STATUS_CONTROL_SEND_ERROR);
			return false;
		}
		current += written;
		size -= written;
	}
	return true;
}

static bool TlsReadExact(void* data, u32 size)
{
	uint8_t* current = data;
	while (size && Cast_Running) {
		u32 read = 0;
		const Result rc = sslConnectionRead(
			&TlsConnection,
			current,
			size,
			&read);
		if (R_FAILED(rc) || read == 0)
			return false;
		current += read;
		size -= read;
	}
	return size == 0;
}

static bool SendCastMessage(
	const char* destinationId,
	const char* nameSpace,
	const char* payload)
{
	const size_t messageSize = CastProtoEncode(
		Buffers.CastMode.ControlTx + 4,
		sizeof(Buffers.CastMode.ControlTx) - 4,
		"sender-0",
		destinationId,
		nameSpace,
		payload);
	if (messageSize == 0) {
		CastControlError = true;
		SetCastStatus(CAST_STATUS_CONTROL_ENCODE_ERROR);
		return false;
	}
	Buffers.CastMode.ControlTx[0] = (uint8_t)(messageSize >> 24);
	Buffers.CastMode.ControlTx[1] = (uint8_t)(messageSize >> 16);
	Buffers.CastMode.ControlTx[2] = (uint8_t)(messageSize >> 8);
	Buffers.CastMode.ControlTx[3] = (uint8_t)messageSize;
	return TlsWriteAll(Buffers.CastMode.ControlTx, (u32)messageSize + 4);
}

static bool ReceiveCastMessage(CastProtoMessage* message, u32 timeoutMs)
{
	s32 pending = 0;
	const Result rc = sslConnectionPending(&TlsConnection, &pending);
	if (R_FAILED(rc) || pending < 0) {
		CastControlError = true;
		SetCastStatus(CAST_STATUS_CONTROL_PENDING_ERROR);
		return false;
	}
	if (pending == 0) {
		const SocketWaitResult waitResult =
			SocketWaitReadableEx(CastSocket, timeoutMs);
		if (waitResult == SocketWaitResult_Timeout)
			return false;
		if (waitResult == SocketWaitResult_Closed) {
			CastControlError = true;
			SetCastStatus(CAST_STATUS_CONTROL_SOCKET_CLOSED);
			return false;
		}
		if (waitResult != SocketWaitResult_Readable) {
			CastControlError = true;
			SetCastStatus(CAST_STATUS_CONTROL_SOCKET_POLL_ERROR);
			return false;
		}
	}

	uint8_t sizeBytes[4];
	if (!TlsReadExact(sizeBytes, sizeof(sizeBytes))) {
		CastControlError = true;
		SetCastStatus(CAST_STATUS_CONTROL_HEADER_ERROR);
		return false;
	}
	const u32 messageSize =
		((u32)sizeBytes[0] << 24) |
		((u32)sizeBytes[1] << 16) |
		((u32)sizeBytes[2] << 8) |
		sizeBytes[3];
	if (messageSize == 0 || messageSize > sizeof(Buffers.CastMode.ControlRx)) {
		CastControlError = true;
		SetCastStatus(CAST_STATUS_CONTROL_FRAME_TOO_LARGE);
		return false;
	}
	if (!TlsReadExact(Buffers.CastMode.ControlRx, messageSize)) {
		CastControlError = true;
		SetCastStatus(CAST_STATUS_CONTROL_BODY_ERROR);
		return false;
	}
	if (!CastProtoDecode(Buffers.CastMode.ControlRx, messageSize, message)) {
		CastControlError = true;
		SetCastStatus(CAST_STATUS_CONTROL_DECODE_ERROR);
		return false;
	}
	return true;
}

static bool JsonFindString(
	const char* json,
	const char* key,
	char* output,
	size_t capacity)
{
	char pattern[80];
	if (npf_snprintf(pattern, sizeof(pattern), "\"%s\"", key) <= 0)
		return false;
	const char* position = strstr(json, pattern);
	if (!position)
		return false;
	position += strlen(pattern);
	while (*position == ' ' || *position == '\t')
		++position;
	if (*position++ != ':')
		return false;
	while (*position == ' ' || *position == '\t')
		++position;
	if (*position++ != '"')
		return false;

	size_t length = 0;
	while (position[length] && position[length] != '"') {
		if (position[length] == '\\')
			return false;
		++length;
	}
	if (position[length] != '"' || length == 0 || length >= capacity)
		return false;
	memcpy(output, position, length);
	output[length] = '\0';
	return true;
}

static bool PumpCastMessage(
	char* transportId,
	size_t transportIdCapacity,
	u32 timeoutMs)
{
	const u64 now = armGetSystemTick();
	if (LastHeartbeatTick == 0 ||
		now - LastHeartbeatTick >= armGetSystemTickFreq() * 5) {
		if (!SendCastMessage(
			"receiver-0",
			NS_HEARTBEAT,
			"{\"type\":\"PING\"}"))
			return false;
		LastHeartbeatTick = now;
	}

	CastProtoMessage message;
	if (!ReceiveCastMessage(&message, timeoutMs))
		return Cast_Running && !CastControlError;

	if (strcmp(message.nameSpace, NS_HEARTBEAT) == 0 &&
		strstr(message.payload, "\"PING\""))
		return SendCastMessage(
			message.sourceId,
			NS_HEARTBEAT,
			"{\"type\":\"PONG\"}");

	if (strcmp(message.nameSpace, NS_CONNECTION) == 0 &&
		strstr(message.payload, "\"CLOSE\"")) {
		SetCastStatus(CAST_STATUS_RECEIVER_CLOSED);
		return false;
	}

	if (strcmp(message.nameSpace, NS_RECEIVER) == 0 &&
		strstr(message.payload, "\"LAUNCH_ERROR\"")) {
		CoreWriteSdFile(
			CAST_ERROR_PATH,
			message.payload,
			strlen(message.payload));
		SetCastStatus(CAST_STATUS_LOAD_FAILED);
		return false;
	}

	if (transportId &&
		strcmp(message.nameSpace, NS_RECEIVER) == 0 &&
		strstr(message.payload, "\"RECEIVER_STATUS\"") &&
		strstr(message.payload, CAST_STREAMING_APP_ID)) {
		JsonFindString(
			message.payload,
			"transportId",
			transportId,
			transportIdCapacity);
		JsonFindString(
			message.payload,
			"sessionId",
			ActiveSessionId,
			sizeof(ActiveSessionId));
	}

	if (strcmp(message.nameSpace, CAST_STREAMING_NAMESPACE) == 0 &&
		strstr(message.payload, "\"ANSWER\"")) {
		AnswerReceived = true;
		AnswerAccepted = CastStreamParseAnswer(
			message.payload,
			OfferSequenceNumber,
			&SessionAnswer);
		if (!AnswerAccepted) {
			CoreWriteSdFile(
				CAST_ERROR_PATH,
				message.payload,
				strlen(message.payload));
			SetCastStatus(CAST_STATUS_ANSWER_REJECTED);
			return false;
		}
	}
	return true;
}

static bool OpenCastConnection(u32 targetAddress)
{
	CastControlError = false;
	LastHeartbeatTick = 0;
	ActiveTransportId[0] = '\0';
	ActiveSessionId[0] = '\0';
	CastSocket = SocketTcpConnect(targetAddress, CAST_CONTROL_PORT);
	if (CastSocket == SOCKET_INVALID)
		return false;

	Result rc = sslInitialize(1);
	if (R_FAILED(rc))
		goto fail;
	TlsInitialized = true;
	rc = sslCreateContext(&TlsContext, SslVersion_TlsV12);
	if (R_FAILED(rc))
		goto fail;
	TlsContextOpen = true;
	rc = sslContextCreateConnection(&TlsContext, &TlsConnection);
	if (R_FAILED(rc))
		goto fail;
	TlsConnectionOpen = true;
	rc = sslConnectionSetOption(
		&TlsConnection,
		SslOptionType_DoNotCloseSocket,
		true);
	if (R_FAILED(rc))
		goto fail;
	rc = sslConnectionSetOption(
		&TlsConnection,
		SslOptionType_SkipDefaultVerify,
		true);
	if (R_FAILED(rc))
		goto fail;
	rc = sslConnectionSetVerifyOption(&TlsConnection, 0);
	if (R_FAILED(rc))
		goto fail;

	int returnedSocket = SOCKET_INVALID;
	rc = sslConnectionSetSocketDescriptor(
		&TlsConnection,
		CastSocket,
		&returnedSocket);
	if (R_FAILED(rc))
		goto fail;
	if (returnedSocket != SOCKET_INVALID)
		bsdClose(returnedSocket);
	rc = sslConnectionDoHandshake(&TlsConnection, NULL, NULL, NULL, 0);
	if (R_FAILED(rc))
		goto fail;
	return true;

fail:
	LOG("SwitchCast TLS setup failed: %x\n", rc);
	return false;
}

static void StopReceiver(void)
{
	if (!TlsConnectionOpen || !ActiveSessionId[0])
		return;
	char stopRequest[256];
	const int length = npf_snprintf(
		stopRequest,
		sizeof(stopRequest),
		"{\"type\":\"STOP\",\"requestId\":90,\"sessionId\":\"%s\"}",
		ActiveSessionId);
	if (length > 0 && (size_t)length < sizeof(stopRequest))
		SendCastMessage("receiver-0", NS_RECEIVER, stopRequest);
}

static void CloseCastConnection(void)
{
	DetachStreamSession();
	if (TlsConnectionOpen) {
		sslConnectionClose(&TlsConnection);
		TlsConnectionOpen = false;
	}
	if (TlsContextOpen) {
		sslContextClose(&TlsContext);
		TlsContextOpen = false;
	}
	if (TlsInitialized) {
		sslExit();
		TlsInitialized = false;
	}
	SocketClose(&CastSocket);
	ActiveTransportId[0] = '\0';
	ActiveSessionId[0] = '\0';
}

static uint32_t ReadRandomWord(const uint8_t* data)
{
	return ((uint32_t)data[0] << 24) |
		((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) |
		data[3];
}

static void FillFallbackRandom(uint8_t* output, size_t size)
{
	uint64_t state = armGetSystemTick() ^
		(uint64_t)(uintptr_t)output ^
		UINT64_C(0x9E3779B97F4A7C15);
	for (size_t i = 0; i < size; ++i) {
		state ^= state >> 12;
		state ^= state << 25;
		state ^= state >> 27;
		output[i] = (uint8_t)(state * UINT64_C(2685821657736338717) >> 56);
	}
}

static void InitializeSessionSecrets(void)
{
	uint8_t random[38];
	bool generated = false;
	const Result initializeResult = csrngInitialize();
	if (R_SUCCEEDED(initializeResult)) {
		generated = R_SUCCEEDED(csrngGetRandomBytes(random, sizeof(random)));
		csrngExit();
	}
	if (!generated)
		FillFallbackRandom(random, sizeof(random));

	memcpy(SessionAesKey, random, sizeof(SessionAesKey));
	memcpy(SessionAesIvMask, random + 16, sizeof(SessionAesIvMask));
	SenderSsrc = ReadRandomWord(random + 32);
	RtpSequenceNumber =
		(uint16_t)(((uint16_t)random[36] << 8) | random[37]);
	if (SenderSsrc == 0)
		SenderSsrc = 1;

	ReceiverSsrc = 0;
	ReceiverUdpPort = 0;
	NextFrameId = 0;
	LatestFrameId = 0;
	LatestRtpTimestamp = 0;
	LatestFrameCaptureTick = 0;
	LastSenderReportTick = 0;
	LastDiagnosticTick = 0;
	SessionStartTick = armGetSystemTick();
	FirstFrameSentTick = 0;
	LastReceiverFeedbackTick = 0;
	LastCheckpointAdvanceTick = 0;
	OfferSequenceNumber = 1;
	AnswerReceived = false;
	AnswerAccepted = false;
	ReceiverCheckpointSeen = false;
	StreamRecoveryRequested = false;
	LatestReceiverCheckpointFrameId = 0;
	StreamRecoveryReason = "none";
	memset(&SessionAnswer, 0, sizeof(SessionAnswer));
}

static bool LaunchReceiver(const char* codecParameter)
{
	if (!SendCastMessage(
		"receiver-0",
		NS_CONNECTION,
		"{\"type\":\"CONNECT\",\"origin\":{}}") ||
		!SendCastMessage(
			"receiver-0",
			NS_RECEIVER,
			"{\"type\":\"GET_STATUS\",\"requestId\":1}"))
		return false;

	char launchRequest[192];
	const int launchLength = npf_snprintf(
		launchRequest,
		sizeof(launchRequest),
		"{\"type\":\"LAUNCH\",\"appId\":\"%s\",\"requestId\":2}",
		CAST_STREAMING_APP_ID);
	if (launchLength <= 0 ||
		(size_t)launchLength >= sizeof(launchRequest) ||
		!SendCastMessage("receiver-0", NS_RECEIVER, launchRequest))
		return false;

	char transportId[CAST_PROTO_ID_MAX] = {0};
	for (int i = 0; i < 40 && Cast_Running && !transportId[0]; ++i) {
		if (!PumpCastMessage(transportId, sizeof(transportId), 500))
			return false;
	}
	if (!transportId[0]) {
		SetCastStatus(CAST_STATUS_RECEIVER_STATUS_TIMEOUT);
		return false;
	}
	strncpy(
		ActiveTransportId,
		transportId,
		sizeof(ActiveTransportId) - 1);
	ActiveTransportId[sizeof(ActiveTransportId) - 1] = '\0';

	if (!SendCastMessage(
		transportId,
		NS_CONNECTION,
		"{\"type\":\"CONNECT\",\"origin\":{}}"))
		return false;

	InitializeSessionSecrets();
	char offer[1536];
	if (!CastStreamBuildOffer(
		offer,
		sizeof(offer),
		OfferSequenceNumber,
		SenderSsrc,
		SessionAesKey,
		SessionAesIvMask,
		codecParameter,
		1280,
		720,
		STREAM_MAX_BIT_RATE,
		Cast_GetTargetDelayMs())) {
		SetCastStatus(CAST_STATUS_OFFER_ERROR);
		return false;
	}

	SetCastStatus(CAST_STATUS_NEGOTIATING);
	if (!SendCastMessage(transportId, CAST_STREAMING_NAMESPACE, offer))
		return false;

	for (int i = 0; i < 20 && Cast_Running && !AnswerReceived; ++i) {
		if (!PumpCastMessage(NULL, 0, 500))
			return false;
	}
	if (!AnswerReceived) {
		SetCastStatus(CAST_STATUS_ANSWER_TIMEOUT);
		return false;
	}
	if (!AnswerAccepted)
		return false;

	ReceiverSsrc = SessionAnswer.receiverSsrc;
	ReceiverUdpPort = SessionAnswer.udpPort;
	UdpSocket = SocketUdp();
	if (UdpSocket == SOCKET_INVALID) {
		SetCastStatus(CAST_STATUS_NETWORK_ERROR);
		return false;
	}
	memset(&UdpDestination, 0, sizeof(UdpDestination));
	UdpDestination.sin_family = AF_INET;
	UdpDestination.sin_addr.s_addr = ReceiverAddress;
	UdpDestination.sin_port = htons(ReceiverUdpPort);

	mutexLock(&StreamMutex);
	StreamSessionAttached = true;
	StreamAwaitingIdr = true;
	mutexUnlock(&StreamMutex);
	SetCastStatus(CAST_STATUS_UDP_READY);
	SaveDiagnostic("negotiated");
	return true;
}

static uint64_t CurrentNtpTimestamp(void)
{
	const uint64_t ticks = armGetSystemTick();
	const uint64_t frequency = armGetSystemTickFreq();
	const uint64_t seconds = ticks / frequency + UINT64_C(2208988800);
	const uint64_t remainder = ticks % frequency;
	const uint64_t fraction = (remainder << 32) / frequency;
	return (seconds << 32) | fraction;
}

static bool SendUdpPacket(const uint8_t* packet, size_t size)
{
	return UdpSocket != SOCKET_INVALID &&
		SocketUDPSendTo(
			UdpSocket,
			packet,
			(u32)size,
			(struct sockaddr*)&UdpDestination,
			sizeof(UdpDestination));
}

static bool SendSenderReport(uint32_t rtpTimestamp)
{
	uint8_t report[28];
	CastStreamBuildSenderReport(
		report,
		SenderSsrc,
		CurrentNtpTimestamp(),
		rtpTimestamp,
		atomic_load(&RtpPackets),
		atomic_load(&RtpOctets));
	if (!SendUdpPacket(report, sizeof(report)))
		return false;
	LastSenderReportTick = armGetSystemTick();
	return true;
}

static int AcquireReadySlot(void)
{
	mutexLock(&StreamMutex);
	int selected = -1;
	uint64_t selectedOrder = UINT64_MAX;
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_Ready &&
			StreamSlots[i].order < selectedOrder) {
			selected = (int)i;
			selectedOrder = StreamSlots[i].order;
		}
	}
	if (selected >= 0) {
		StreamSlot* slot = &StreamSlots[selected];
		slot->state = StreamSlot_Sending;
		slot->frameId = NextFrameId++;
		LatestFrameId = slot->frameId;
		LatestRtpTimestamp = slot->rtpTimestamp;
		LatestFrameCaptureTick = slot->captureTick;
	}
	mutexUnlock(&StreamMutex);
	return selected;
}

static void ReleaseSlotToHistory(int index)
{
	mutexLock(&StreamMutex);
	if (StreamSlots[index].state == StreamSlot_Sending)
		StreamSlots[index].state = StreamSlot_History;
	mutexUnlock(&StreamMutex);
}

static void EncryptSlot(StreamSlot* slot)
{
	if (slot->encrypted)
		return;
	uint8_t nonce[16];
	CastStreamBuildNonce(SessionAesIvMask, slot->frameId, nonce);
	Aes128CtrContext context;
	aes128CtrContextCreate(&context, SessionAesKey, nonce);
	aes128CtrCrypt(&context, slot->data, slot->data, slot->size);
	slot->encrypted = true;
}

static bool SendRtpPacket(
	StreamSlot* slot,
	uint16_t packetId,
	bool retransmission)
{
	const size_t packetSize = CastStreamBuildRtpPacket(
		Buffers.CastMode.UdpTx,
		&RtpSequenceNumber,
		SenderSsrc,
		slot->frameId,
		slot->rtpTimestamp,
		slot->keyFrame,
		slot->data,
		slot->size,
		packetId);
	if (packetSize == 0 ||
		!SendUdpPacket(Buffers.CastMode.UdpTx, packetSize))
		return false;
	atomic_fetch_add(&RtpPackets, 1);
	atomic_fetch_add(
		&RtpOctets,
		(unsigned int)(packetSize - CAST_STREAM_HEADER_SIZE));
	if (retransmission)
		atomic_fetch_add(&RetransmitPackets, 1);
	return true;
}

static bool SendFrame(int index)
{
	StreamSlot* slot = &StreamSlots[index];
	EncryptSlot(slot);

	const u64 now = armGetSystemTick();
	if (LastSenderReportTick == 0 ||
		now - LastSenderReportTick >=
			armGetSystemTickFreq() * STREAM_SENDER_REPORT_INTERVAL_MS / 1000) {
		uint32_t reportRtpTimestamp = slot->rtpTimestamp;
		if (slot->captureTick && now > slot->captureTick) {
			reportRtpTimestamp += (uint32_t)(
				(now - slot->captureTick) * UINT64_C(90000) /
				armGetSystemTickFreq());
		}
		if (!SendSenderReport(reportRtpTimestamp))
			return false;
	}

	const size_t packetCount = CastStreamPacketCount(slot->size);
	if (packetCount > UINT16_MAX)
		return false;
	const size_t pacingGroup = IsUltraLowLatency()
		? STREAM_ULTRA_PACING_GROUP
		: STREAM_STABLE_PACING_GROUP;
	const s64 pacingSleep = IsUltraLowLatency()
		? STREAM_ULTRA_PACING_SLEEP_NS
		: STREAM_STABLE_PACING_SLEEP_NS;
	for (size_t i = 0; i < packetCount; ++i) {
		if (!SendRtpPacket(slot, (uint16_t)i, false))
			return false;
		if ((i + 1U) % pacingGroup == 0U)
			svcSleepThread(pacingSleep);
	}
	atomic_fetch_add(&SentFrames, 1);
	return true;
}

static int AcquireHistorySlot(uint32_t frameId)
{
	mutexLock(&StreamMutex);
	int selected = -1;
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_History &&
			StreamSlots[i].frameId == frameId) {
			StreamSlots[i].state = StreamSlot_Sending;
			selected = (int)i;
			break;
		}
	}
	mutexUnlock(&StreamMutex);
	return selected;
}

static void RequestStreamRecovery(const char* reason)
{
	if (StreamRecoveryRequested)
		return;
	StreamRecoveryRequested = true;
	StreamRecoveryReason = reason ? reason : "unknown";
	atomic_fetch_add(&RecoveryRestarts, 1);
	SetCastStatus(CAST_STATUS_RECOVERING);
}

static void EnterKeyframeRecovery(void)
{
	mutexLock(&StreamMutex);
	StreamAwaitingIdr = true;
	DropReadyFramesLocked();
	mutexUnlock(&StreamMutex);
}

static void ReleaseAcknowledgedHistory(uint32_t checkpoint)
{
	mutexLock(&StreamMutex);
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		if (StreamSlots[i].state == StreamSlot_History &&
			StreamSlots[i].frameId <= checkpoint) {
			FreeStreamSlotLocked(&StreamSlots[i]);
		}
	}
	mutexUnlock(&StreamMutex);
}

static bool Retransmit(const CastStreamNack* nack)
{
	const int index = AcquireHistorySlot(nack->frameId);
	if (index < 0) {
		atomic_fetch_add(&UnrecoverableNacks, 1);
		RequestStreamRecovery("evicted_nack");
		return true;
	}

	StreamSlot* slot = &StreamSlots[index];
	const size_t packetCount = CastStreamPacketCount(slot->size);
	bool success = true;
	if (nack->packetId == UINT16_MAX) {
		for (size_t i = 0; i < packetCount; ++i) {
			if (!SendRtpPacket(slot, (uint16_t)i, true)) {
				success = false;
				break;
			}
		}
	} else if (nack->packetId < packetCount) {
		success = SendRtpPacket(slot, nack->packetId, true);
	} else {
		atomic_fetch_add(&UnrecoverableNacks, 1);
		RequestStreamRecovery("invalid_nack");
	}
	ReleaseSlotToHistory(index);
	return success;
}

static bool PumpRtcp(void)
{
	for (;;) {
		struct sockaddr_in sender;
		socklen_t senderLength = sizeof(sender);
		const s32 received = SocketUDPRecvFrom(
			UdpSocket,
			Buffers.CastMode.UdpRx,
			sizeof(Buffers.CastMode.UdpRx),
			(struct sockaddr*)&sender,
			&senderLength);
		if (received == 0)
			return true;
		if (received < 0)
			return false;
		if (sender.sin_addr.s_addr != ReceiverAddress)
			continue;

		atomic_fetch_add(&RtcpPackets, 1);
		CastStreamFeedback feedback;
		if (!CastStreamParseRtcp(
			Buffers.CastMode.UdpRx,
			(size_t)received,
			SenderSsrc,
			ReceiverSsrc,
			LatestFrameId,
			&feedback))
			continue;

		if (feedback.valid) {
			const u64 feedbackTick = armGetSystemTick();
			LastReceiverFeedbackTick = feedbackTick;
			if (!ReceiverCheckpointSeen ||
				feedback.checkpointFrameId >
					LatestReceiverCheckpointFrameId) {
				ReceiverCheckpointSeen = true;
				LatestReceiverCheckpointFrameId =
					feedback.checkpointFrameId;
				LastCheckpointAdvanceTick = feedbackTick;
			}
			atomic_store(
				&ReceiverPlayoutDelayMs,
				feedback.playoutDelayMs);
			ReleaseAcknowledgedHistory(feedback.checkpointFrameId);
			atomic_fetch_add(
				&NackRequests,
				(unsigned int)feedback.nackCount);
			for (size_t i = 0; i < feedback.nackCount; ++i) {
				if (!Retransmit(&feedback.nacks[i]))
					return false;
			}
			if (feedback.nackOverflow) {
				atomic_fetch_add(&UnrecoverableNacks, 1);
				RequestStreamRecovery("nack_overflow");
			}
		}
		if (feedback.pictureLoss) {
			atomic_fetch_add(&PictureLossRequests, 1);
			EnterKeyframeRecovery();
		}
	}
}

static unsigned int TickAgeMs(u64 tick, u64 now)
{
	if (tick == 0 || now <= tick)
		return 0;
	const uint64_t milliseconds =
		(now - tick) * UINT64_C(1000) / armGetSystemTickFreq();
	return milliseconds > UINT32_MAX
		? UINT32_MAX
		: (unsigned int)milliseconds;
}

static void SnapshotRetainedQueue(
	unsigned int* retainedFrames,
	unsigned int* retainedBytes,
	unsigned int* historyFrames,
	unsigned int* historyBytes)
{
	*retainedFrames = 0;
	*retainedBytes = 0;
	*historyFrames = 0;
	*historyBytes = 0;
	mutexLock(&StreamMutex);
	for (size_t i = 0; i < STREAM_SLOT_COUNT; ++i) {
		const StreamSlot* slot = &StreamSlots[i];
		if (slot->state == StreamSlot_Free)
			continue;
		++*retainedFrames;
		*retainedBytes += (unsigned int)slot->allocationSize;
		if (slot->state == StreamSlot_History) {
			++*historyFrames;
			*historyBytes += (unsigned int)slot->allocationSize;
		}
	}
	mutexUnlock(&StreamMutex);
}

static void SaveDiagnosticToPath(const char* path, const char* reason)
{
	char json[2304];
	const uint8_t* octets = (const uint8_t*)&ReceiverAddress;
	const u64 now = armGetSystemTick();
	unsigned int retainedFrames;
	unsigned int retainedBytes;
	unsigned int historyFrames;
	unsigned int historyBytes;
	SnapshotRetainedQueue(
		&retainedFrames,
		&retainedBytes,
		&historyFrames,
		&historyBytes);
	const int length = npf_snprintf(
		json,
		sizeof(json),
		"{\n"
		"  \"version\":\"%s\",\n"
		"  \"reason\":\"%s\",\n"
		"  \"status\":%u,\n"
		"  \"transport\":\"cast-streaming-udp\",\n"
		"  \"appId\":\"%s\",\n"
		"  \"receiver\":\"%u.%u.%u.%u\",\n"
		"  \"udpPort\":%u,\n"
		"  \"latencyProfile\":\"%s\",\n"
		"  \"targetDelayMs\":%u,\n"
		"  \"receiverPlayoutDelayMs\":%u,\n"
		"  \"sessionAgeMs\":%u,\n"
		"  \"heartbeatAgeMs\":%u,\n"
		"  \"receiverFeedbackAgeMs\":%u,\n"
		"  \"checkpointAgeMs\":%u,\n"
		"  \"lastCheckpointFrameId\":%u,\n"
		"  \"recoveryReason\":\"%s\",\n"
		"  \"senderSsrc\":%u,\n"
		"  \"receiverSsrc\":%u,\n"
		"  \"retainedFrames\":%u,\n"
		"  \"retainedBytes\":%u,\n"
		"  \"historyFrames\":%u,\n"
		"  \"historyBytes\":%u,\n"
		"  \"peakRetainedFrames\":%u,\n"
		"  \"peakRetainedBytes\":%u,\n"
		"  \"historyEvictions\":%u,\n"
		"  \"recoveryRestarts\":%u,\n"
		"  \"feedbackStalls\":%u,\n"
		"  \"queuedFrames\":%u,\n"
		"  \"sentFrames\":%u,\n"
		"  \"droppedFrames\":%u,\n"
		"  \"oversizedFrames\":%u,\n"
		"  \"rtpPackets\":%u,\n"
		"  \"rtpOctets\":%u,\n"
		"  \"retransmitPackets\":%u,\n"
		"  \"rtcpPackets\":%u,\n"
		"  \"nackRequests\":%u,\n"
		"  \"pictureLossRequests\":%u,\n"
		"  \"unrecoverableNacks\":%u\n"
		"}\n",
		SWITCHCAST_VERSION_STRING,
		reason ? reason : "unknown",
		atomic_load(&CastStatus),
		CAST_STREAMING_APP_ID,
		octets[0],
		octets[1],
		octets[2],
		octets[3],
		(unsigned int)ReceiverUdpPort,
		IsUltraLowLatency() ? "ultra" : "stable",
		Cast_GetTargetDelayMs(),
		atomic_load(&ReceiverPlayoutDelayMs),
		TickAgeMs(SessionStartTick, now),
		TickAgeMs(LastHeartbeatTick, now),
		TickAgeMs(LastReceiverFeedbackTick, now),
		TickAgeMs(LastCheckpointAdvanceTick, now),
		LatestReceiverCheckpointFrameId,
		StreamRecoveryReason,
		SenderSsrc,
		ReceiverSsrc,
		retainedFrames,
		retainedBytes,
		historyFrames,
		historyBytes,
		atomic_load(&PeakRetainedFrames),
		atomic_load(&PeakRetainedBytes),
		atomic_load(&HistoryEvictions),
		atomic_load(&RecoveryRestarts),
		atomic_load(&FeedbackStalls),
		atomic_load(&QueuedFrames),
		atomic_load(&SentFrames),
		atomic_load(&DroppedFrames),
		atomic_load(&OversizedFrames),
		atomic_load(&RtpPackets),
		atomic_load(&RtpOctets),
		atomic_load(&RetransmitPackets),
		atomic_load(&RtcpPackets),
		atomic_load(&NackRequests),
		atomic_load(&PictureLossRequests),
		atomic_load(&UnrecoverableNacks));
	if (length > 0 && (size_t)length < sizeof(json))
		CoreWriteSdFile(path, json, (u64)length);
}

static void SaveDiagnostic(const char* reason)
{
	SaveDiagnosticToPath(CAST_DIAGNOSTIC_PATH, reason);
}

static void SaveFailureDiagnostic(const char* reason)
{
	SaveDiagnosticToPath(CAST_DIAGNOSTIC_PATH, reason);
	SaveDiagnosticToPath(CAST_LAST_FAILURE_PATH, reason);
}

static bool ReceiverFeedbackStalled(u64 now)
{
	if (FirstFrameSentTick == 0)
		return false;
	const bool hasOutstandingFrames =
		!ReceiverCheckpointSeen ||
		LatestReceiverCheckpointFrameId < LatestFrameId;
	if (!hasOutstandingFrames)
		return false;
	const u64 progressTick = ReceiverCheckpointSeen
		? LastCheckpointAdvanceTick
		: FirstFrameSentTick;
	return progressTick != 0 &&
		now - progressTick >=
			armGetSystemTickFreq() * STREAM_FEEDBACK_STALL_MS / 1000;
}

static StreamSessionResult RunStreamSession(void)
{
	bool sentFirstFrame = false;
	StreamSessionResult result = StreamSession_Stopped;
	LastDiagnosticTick = armGetSystemTick();
	while (Cast_Running) {
		if (!IsApplicationRunning()) {
			result = StreamSession_GameplayStopped;
			break;
		}

		if (!PumpCastMessage(NULL, 0, 0) || !PumpRtcp()) {
			// Cast receivers commonly close their application channel as the
			// source disappears. If capture stopped at the same time, this is
			// a normal game transition rather than a permanent media failure.
			if (!IsApplicationRunning() || !HasRecentGameplay())
				result = StreamSession_GameplayStopped;
			else
				result = StreamSession_Failed;
			break;
		}
		if (StreamRecoveryRequested) {
			result = StreamSession_Recover;
			break;
		}

		const int index = AcquireReadySlot();
		if (index >= 0) {
			if (!SendFrame(index)) {
				ReleaseSlotToHistory(index);
				SetCastStatus(CAST_STATUS_PACKET_ERROR);
				result = StreamSession_Failed;
				break;
			}
			ReleaseSlotToHistory(index);
			if (!sentFirstFrame) {
				sentFirstFrame = true;
				FirstFrameSentTick = armGetSystemTick();
				SetCastStatus(CAST_STATUS_STREAMING);
				SaveDiagnostic("streaming_started");
			}
		} else {
			const u64 now = armGetSystemTick();
			if (LastSenderReportTick &&
				now - LastSenderReportTick >=
					armGetSystemTickFreq() *
					STREAM_SENDER_REPORT_INTERVAL_MS / 1000) {
				uint32_t reportRtpTimestamp = LatestRtpTimestamp;
				if (LatestFrameCaptureTick &&
					now > LatestFrameCaptureTick) {
					reportRtpTimestamp += (uint32_t)(
						(now - LatestFrameCaptureTick) *
						UINT64_C(90000) /
						armGetSystemTickFreq());
				}
				if (!SendSenderReport(reportRtpTimestamp)) {
					result = StreamSession_Failed;
					break;
				}
			}
			svcSleepThread(
				IsUltraLowLatency()
					? STREAM_ULTRA_IDLE_SLEEP_NS
					: STREAM_STABLE_IDLE_SLEEP_NS);
		}

		// Only blank after the first gameplay frame is transmitted. If capture stops
		// producing frames (for example, after HOME or a title transition),
		// restore the console within the gameplay freshness window.
		Cast_UpdateConsoleScreenBlanking(
			sentFirstFrame && HasRecentGameplay());

		const u64 now = armGetSystemTick();
		if (ReceiverFeedbackStalled(now)) {
			atomic_fetch_add(&FeedbackStalls, 1);
			RequestStreamRecovery(
				ReceiverCheckpointSeen
					? "checkpoint_stalled"
					: "feedback_missing");
			result = StreamSession_Recover;
			break;
		}
		if (now - LastDiagnosticTick >=
			armGetSystemTickFreq() * STREAM_DIAGNOSTIC_INTERVAL_SECONDS) {
			LastDiagnosticTick = now;
			SaveDiagnostic("streaming");
		}
	}
	RestoreConsoleScreen();
	return result;
}

bool Cast_WriteH264(
	const uint8_t* annexB,
	size_t annexBSize,
	uint64_t timestampUs,
	bool randomAccess)
{
	if (!Cast_ClientStreaming)
		return false;

	mutexLock(&StreamMutex);
	if (!Cast_ClientStreaming || !StreamBuffer) {
		mutexUnlock(&StreamMutex);
		return false;
	}

	const bool configured = Fmp4ObserveAccessUnit(
		&H264Config,
		annexB,
		annexBSize);
	atomic_store(&LastGameplayTick, armGetSystemTick());
	if (configured)
		atomic_store(&GameplayReady, true);
	if (!configured || !StreamSessionAttached) {
		mutexUnlock(&StreamMutex);
		return true;
	}

	if (StreamAwaitingIdr && !randomAccess) {
		atomic_fetch_add(&DroppedFrames, 1);
		mutexUnlock(&StreamMutex);
		return true;
	}
	if (annexBSize == 0 || annexBSize > StreamBufferSize) {
		atomic_fetch_add(&OversizedFrames, 1);
		atomic_fetch_add(&DroppedFrames, 1);
		StreamAwaitingIdr = true;
		mutexUnlock(&StreamMutex);
		return true;
	}

	int selected = -1;
	size_t frameOffset = 0;
	size_t allocationSize = 0;
	bool droppedReadyForPressure = false;
	for (;;) {
		selected = FindFreeStreamSlotLocked();
		if (selected >= 0 &&
			FindFreeFrameRegionLocked(
				annexBSize,
				&frameOffset,
				&allocationSize))
			break;
		if (EvictOldestHistoryLocked())
			continue;
		if (!droppedReadyForPressure) {
			droppedReadyForPressure = true;
			DropReadyFramesLocked();
			StreamAwaitingIdr = true;
			if (!randomAccess) {
				atomic_fetch_add(&DroppedFrames, 1);
				mutexUnlock(&StreamMutex);
				return true;
			}
			continue;
		}
		atomic_fetch_add(&DroppedFrames, 1);
		StreamAwaitingIdr = true;
		mutexUnlock(&StreamMutex);
		return true;
	}

	StreamSlot* slot = &StreamSlots[selected];
	slot->data = StreamBuffer + frameOffset;
	memcpy(slot->data, annexB, annexBSize);
	slot->size = annexBSize;
	slot->offset = frameOffset;
	slot->allocationSize = allocationSize;
	slot->order = StreamNextOrder++;
	slot->captureTick = armGetSystemTick();
	slot->rtpTimestamp =
		(uint32_t)((timestampUs * UINT64_C(9)) / UINT64_C(100));
	slot->keyFrame = randomAccess;
	slot->encrypted = false;
	slot->state = StreamSlot_Ready;
	StreamAwaitingIdr = false;
	atomic_fetch_add(&QueuedFrames, 1);
	UpdateRetainedHighWaterLocked();
	mutexUnlock(&StreamMutex);
	return true;
}

static void ArmCaptureForNextGame(void)
{
	StopCaptureAndReleaseMemory();
	SetCastStatus(CAST_STATUS_WAITING_GAME);
}

static bool WaitForGameplay(void)
{
	while (Cast_Running && !HasRecentGameplay()) {
		if (!IsApplicationRunning()) {
			if (Cast_ClientStreaming || StreamBuffer)
				ArmCaptureForNextGame();
		}
		else if (!Cast_ClientStreaming) {
			if (Cast_GetStatus() == CAST_STATUS_MEMORY_ERROR)
				return false;
			if (!PrepareCaptureForGame())
				return false;
		}
		svcSleepThread(100E+6);
	}
	return Cast_Running;
}

static void HoldFatalStatus(void)
{
	StopCaptureAndReleaseMemory();
	while (Cast_Running)
		svcSleepThread(100E+6);
}

void Cast_ServerThread(void* unused)
{
	(void)unused;
	mutexInit(&StreamMutex);
	Cast_Running = true;
	RestoreConsoleScreen();
	RefreshLatencyProfile();

	Result rc = pmdmntInitialize();
	if (R_FAILED(rc)) {
		LOG("pm:dmnt initialization failed: %x\n", rc);
		SetCastStatus(CAST_STATUS_GAME_MONITOR_ERROR);
		while (Cast_Running)
			svcSleepThread(100E+6);
		goto finished;
	}
	ProcessMonitorInitialized = true;
	ArmCaptureForNextGame();

	while (Cast_Running) {
		if (!WaitForGameplay()) {
			if (Cast_Running)
				HoldFatalStatus();
			break;
		}
		SetCastStatus(CAST_STATUS_DISCOVERING);

		if (!DiscoverTarget(&ReceiverAddress)) {
			if (!HasRecentGameplay())
				ArmCaptureForNextGame();
			svcSleepThread(1E+9);
			continue;
		}
		if (!HasRecentGameplay()) {
			ArmCaptureForNextGame();
			continue;
		}

		char codec[FMP4_CODEC_STRING_SIZE];
		mutexLock(&StreamMutex);
		const bool hasCodec = Fmp4GetCodecString(
			&H264Config,
			codec,
			sizeof(codec));
		mutexUnlock(&StreamMutex);
		if (!hasCodec) {
			SetCastStatus(CAST_STATUS_CODEC_TIMEOUT);
			svcSleepThread(500E+6);
			continue;
		}

		SetCastStatus(CAST_STATUS_CONNECTING);
		if (!OpenCastConnection(ReceiverAddress)) {
			SetCastStatus(CAST_STATUS_NETWORK_ERROR);
			CloseCastConnection();
			if (!HasRecentGameplay())
				ArmCaptureForNextGame();
			svcSleepThread(1E+9);
			continue;
		}

		if (!LaunchReceiver(codec)) {
			const uint32_t status = Cast_GetStatus();
			if (status != CAST_STATUS_LOAD_FAILED &&
				status != CAST_STATUS_RECEIVER_CLOSED &&
				status != CAST_STATUS_RECEIVER_STATUS_TIMEOUT &&
				status != CAST_STATUS_ANSWER_TIMEOUT &&
				status != CAST_STATUS_ANSWER_REJECTED &&
				status != CAST_STATUS_OFFER_ERROR &&
				!IsControlFailureStatus(status))
				SetCastStatus(CAST_STATUS_NETWORK_ERROR);
			SaveDiagnostic("receiver_launch_failed");
			StopReceiver();
			CloseCastConnection();
			if (!HasRecentGameplay()) {
				ArmCaptureForNextGame();
				continue;
			}
			// Keep the working capture session and retry the receiver. This
			// handles transient launch/negotiation failures without a reboot.
			svcSleepThread(2E+9);
			continue;
		}

		const StreamSessionResult sessionResult = RunStreamSession();
		if (sessionResult == StreamSession_GameplayStopped) {
			SaveDiagnostic("gameplay_stopped");
		} else if (sessionResult == StreamSession_Recover) {
			SaveFailureDiagnostic(StreamRecoveryReason);
		} else if (sessionResult == StreamSession_Failed) {
			const uint32_t status = Cast_GetStatus();
			if (!IsControlFailureStatus(status) &&
				status != CAST_STATUS_RECEIVER_CLOSED &&
				status != CAST_STATUS_PACKET_ERROR)
				SetCastStatus(CAST_STATUS_NETWORK_ERROR);
			SaveFailureDiagnostic("stream_failed");
		}
		DetachStreamSession();
		StopReceiver();
		CloseCastConnection();

		if (!Cast_Running || sessionResult == StreamSession_Stopped)
			break;
		if (sessionResult == StreamSession_GameplayStopped) {
			ArmCaptureForNextGame();
			continue;
		}
		if (sessionResult == StreamSession_Recover) {
			svcSleepThread(250E+6);
			continue;
		}
		if (sessionResult == StreamSession_Failed) {
			if (!IsApplicationRunning() || !HasRecentGameplay())
				ArmCaptureForNextGame();
			else
				svcSleepThread(1E+9);
			continue;
		}
	}

finished:
	RestoreConsoleScreen();
	StopCaptureAndReleaseMemory();
	StopReceiver();
	CloseCastConnection();
	if (ProcessMonitorInitialized) {
		pmdmntExit();
		ProcessMonitorInitialized = false;
	}
	SetCastStatus(CAST_STATUS_OFF);
}

void Cast_StopServer(void)
{
	Cast_Running = false;
	Cast_ClientStreaming = false;
	SocketClose(&UdpSocket);
	SocketClose(&CastSocket);
}

#endif
