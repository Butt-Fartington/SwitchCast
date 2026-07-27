/*
 * SwitchCast USB Dock transport.
 *
 * The packet protocol and original USB transport design come from SysDVR by
 * Exelix11 and contributors. SwitchCast remains GPL-2.0-only; see
 * SYSDVR-ATTRIBUTION.md.
 */
#include "usb_mode.h"

#include <stdatomic.h>
#include <string.h>
#include <switch.h>

#include "../capture.h"
#include "../cast/cast.h"
#include "../modes/modes.h"
#include "usb_proto.h"
#include "usb_transport.h"

#define USB_KEEPALIVE_INTERVAL_NS UINT64_C(500000000)
#define USB_VIDEO_FRESHNESS_NS UINT64_C(1500000000)
#define USB_SUPERVISOR_INTERVAL_NS UINT64_C(100000000)

static atomic_uint UsbStatus = USB_STATUS_OFF;
static atomic_uint UsbSessionGeneration;
static atomic_ullong LastUsbVideoTick;
static atomic_bool UsbClientConnected;
static atomic_bool UsbAudioEnabled;
static atomic_bool UsbKeepaliveEnabled;
static bool UsbAudioInitialized;
static bool UsbInitialized;
static Mutex UsbIoMutex;
static Thread UsbSupervisorThread;

uint32_t UsbModeGetStatus(void)
{
	return atomic_load(&UsbStatus);
}

static bool UsbWriteRaw(const void* data, size_t size)
{
	bool success;

	mutexLock(&UsbIoMutex);
	success =
		atomic_load(&IsThreadRunning) &&
		UsbTransportWrite(data, size);
	mutexUnlock(&UsbIoMutex);
	return success;
}

static bool UsbReadRaw(void* data, size_t size)
{
	bool success;

	mutexLock(&UsbIoMutex);
	success =
		atomic_load(&IsThreadRunning) &&
		UsbTransportRead(data, size);
	mutexUnlock(&UsbIoMutex);
	return success;
}

static void UsbDisconnectClient(void)
{
	atomic_store(&UsbClientConnected, false);
	atomic_store(&UsbAudioEnabled, false);
	atomic_store(&UsbKeepaliveEnabled, false);
	atomic_store(&LastUsbVideoTick, 0);
	Cast_UpdateConsoleScreenBlanking(false);

	if (!atomic_load(&IsThreadRunning)) {
		atomic_store(&UsbStatus, USB_STATUS_OFF);
		return;
	}

	const uint32_t status = atomic_load(&UsbStatus);
	if (
		status != USB_STATUS_IO_ERROR &&
		status != USB_STATUS_CAPTURE_ERROR)
		atomic_store(&UsbStatus, USB_STATUS_WAITING_DOCK);
}

static bool UsbWriteConnected(const void* data, size_t size)
{
	bool success;

	mutexLock(&UsbIoMutex);
	success =
		atomic_load(&IsThreadRunning) &&
		atomic_load(&UsbClientConnected) &&
		UsbTransportWrite(data, size);
	mutexUnlock(&UsbIoMutex);
	if (!success && atomic_load(&UsbClientConnected)) {
		atomic_store(&UsbStatus, USB_STATUS_IO_ERROR);
		UsbDisconnectClient();
	}
	return success;
}

static bool UsbConnectClient(void)
{
	uint8_t requestBytes[SWITCHCAST_USB_REQUEST_SIZE];
	uint8_t response[SWITCHCAST_USB_RESPONSE_SIZE];
	SwitchCastUsbRequest request;

	atomic_store(&UsbStatus, USB_STATUS_WAITING_DOCK);
	if (!UsbWriteRaw(
		SWITCHCAST_USB_HELLO,
		SWITCHCAST_USB_HELLO_SIZE))
		return false;
	if (!UsbReadRaw(requestBytes, sizeof(requestBytes)))
		return false;

	SwitchCastUsbHandshakeResult result =
		SwitchCastUsbParseRequest(
			requestBytes,
			sizeof(requestBytes),
			&request);
	if (
		result == SwitchCastUsbHandshake_Ok &&
		request.audioEnabled &&
		!UsbAudioInitialized)
		result = SwitchCastUsbHandshake_InvalidChannel;
	SwitchCastUsbBuildResponse(result, response);
	if (!UsbWriteRaw(response, sizeof(response)))
		return false;
	if (result != SwitchCastUsbHandshake_Ok)
		return false;

	CaptureConfigResetDefault();
	CaptureSetNalHashing(false, false);
	CaptureSetPPSSPSInject(request.injectSpsPps);
	CaptureSetPPSSPSInterval(1);
	CaptureSetAudioBatching(0);
	CaptureVideoConnected();

	atomic_fetch_add(&UsbSessionGeneration, 1);
	atomic_store(&LastUsbVideoTick, 0);
	atomic_store(&UsbAudioEnabled, request.audioEnabled);
	atomic_store(&UsbKeepaliveEnabled, request.keepaliveEnabled);
	atomic_store(&UsbClientConnected, true);
	atomic_store(&UsbStatus, USB_STATUS_WAITING_GAME);
	return true;
}

static void UsbVideoThread(void* unused)
{
	(void)unused;
	if (!UsbInitialized) {
		while (atomic_load(&IsThreadRunning))
			svcSleepThread(500E+6L);
		return;
	}

	while (atomic_load(&IsThreadRunning)) {
		if (!atomic_load(&UsbClientConnected)) {
			if (!UsbConnectClient()) {
				if (atomic_load(&IsThreadRunning))
					svcSleepThread(250E+6L);
				continue;
			}
		}

		const uint32_t generation =
			atomic_load(&UsbSessionGeneration);
		const bool captured = CaptureReadVideo();
		if (!atomic_load(&IsThreadRunning))
			break;

		/*
		 * A supervisor keepalive can detect a cable loss while grc:d is
		 * blocked. If a new USB session replaced this one, discard the
		 * stale capture result instead of leaking it into the new session.
		 */
		if (
			!atomic_load(&UsbClientConnected) ||
			generation != atomic_load(&UsbSessionGeneration))
			continue;

		const size_t packetSize =
			sizeof(PacketHeader) + VPkt.Header.DataSize;
		if (!UsbWriteConnected(&VPkt, packetSize))
			continue;

		if (!captured) {
			atomic_store(&UsbStatus, USB_STATUS_CAPTURE_ERROR);
			UsbDisconnectClient();
			continue;
		}

		atomic_store(&LastUsbVideoTick, armGetSystemTick());
		atomic_store(&UsbStatus, USB_STATUS_STREAMING);
		Cast_UpdateConsoleScreenBlanking(true);
	}
	UsbDisconnectClient();
}

static void UsbAudioThread(void* unused)
{
	(void)unused;
	uint32_t captureGeneration = 0;

	while (atomic_load(&IsThreadRunning)) {
		if (
			!UsbInitialized ||
			!atomic_load(&UsbClientConnected) ||
			!atomic_load(&UsbAudioEnabled)) {
			svcSleepThread(100E+6L);
			continue;
		}

		const uint32_t generation =
			atomic_load(&UsbSessionGeneration);
		if (generation != captureGeneration) {
			CaptureAudioConnected();
			captureGeneration = generation;
		}

		const bool captured = CaptureReadAudio();
		if (!atomic_load(&IsThreadRunning))
			break;
		if (
			!atomic_load(&UsbClientConnected) ||
			!atomic_load(&UsbAudioEnabled) ||
			generation != atomic_load(&UsbSessionGeneration))
			continue;

		const size_t packetSize =
			sizeof(PacketHeader) + APkt.Header.DataSize;
		if (!UsbWriteConnected(&APkt, packetSize))
			continue;
		if (!captured) {
			atomic_store(&UsbStatus, USB_STATUS_CAPTURE_ERROR);
			UsbDisconnectClient();
		}
	}
}

static void UsbSupervisor(void* unused)
{
	(void)unused;
	PacketHeader keepalive = {
		.Magic = STREAM_PACKET_HEADER,
		.DataSize = 0,
		.Timestamp = 0,
		.MetaData = 0,
		.ReplaySlot = 0xFF,
	};
	u64 lastKeepaliveTick = 0;

	while (atomic_load(&IsThreadRunning)) {
		if (atomic_load(&UsbClientConnected)) {
			const u64 now = armGetSystemTick();
			const u64 frequency = armGetSystemTickFreq();
			const u64 lastVideo =
				atomic_load(&LastUsbVideoTick);

			if (
				lastVideo != 0 &&
				now - lastVideo >=
					frequency * USB_VIDEO_FRESHNESS_NS /
						UINT64_C(1000000000)) {
				Cast_UpdateConsoleScreenBlanking(false);
				if (atomic_load(&UsbStatus) == USB_STATUS_STREAMING)
					atomic_store(
						&UsbStatus,
						USB_STATUS_WAITING_GAME);
			}

			if (
				atomic_load(&UsbKeepaliveEnabled) &&
				(lastKeepaliveTick == 0 ||
				 now - lastKeepaliveTick >=
					frequency * USB_KEEPALIVE_INTERVAL_NS /
						UINT64_C(1000000000))) {
				keepalive.Timestamp = now;
				if (UsbWriteConnected(
					&keepalive,
					sizeof(keepalive)))
					lastKeepaliveTick = now;
			}
		} else {
			lastKeepaliveTick = 0;
			Cast_UpdateConsoleScreenBlanking(false);
		}
		svcSleepThread(USB_SUPERVISOR_INTERVAL_NS);
	}
	Cast_UpdateConsoleScreenBlanking(false);
}

static void UsbInitializeMode(void)
{
	UsbInitialized = false;
	UsbAudioInitialized = false;
	mutexInit(&UsbIoMutex);
	atomic_store(&UsbClientConnected, false);
	atomic_store(&UsbAudioEnabled, false);
	atomic_store(&UsbKeepaliveEnabled, false);
	atomic_store(&LastUsbVideoTick, 0);
	atomic_store(&UsbStatus, USB_STATUS_INITIALIZING);

	Result rc = CaptureInitializeAudio();
	if (R_FAILED(rc)) {
		LOG("CaptureInitializeAudio failed: %x\n", rc);
		atomic_store(&UsbStatus, USB_STATUS_INIT_ERROR);
		return;
	}
	UsbAudioInitialized = true;

	rc = UsbTransportInitialize();
	if (R_FAILED(rc)) {
		LOG("UsbTransportInitialize failed: %x\n", rc);
		CaptureFinalizeAudio();
		UsbAudioInitialized = false;
		atomic_store(&UsbStatus, USB_STATUS_INIT_ERROR);
		return;
	}

	UsbInitialized = true;
	atomic_store(&UsbStatus, USB_STATUS_WAITING_DOCK);
	LaunchThread(
		&UsbSupervisorThread,
		UsbSupervisor,
		NULL,
		Buffers.UsbMode.SupervisorThreadStackArea,
		sizeof(Buffers.UsbMode.SupervisorThreadStackArea),
		0x2D);
}

static void UsbExitMode(void)
{
	UsbDisconnectClient();
	if (UsbInitialized)
		UsbTransportExit();
	UsbInitialized = false;
	if (UsbAudioInitialized)
		CaptureFinalizeAudio();
	UsbAudioInitialized = false;
	JoinThread(&UsbSupervisorThread);
	atomic_store(&UsbStatus, USB_STATUS_OFF);
}

const StreamMode USB_MODE = {
	UsbInitializeMode,
	UsbExitMode,
	UsbVideoThread,
	NULL,
	UsbAudioThread,
	NULL
};
