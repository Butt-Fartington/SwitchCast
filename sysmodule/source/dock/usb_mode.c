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
#include <switch/runtime/devices/usb_comms.h>

#include "../capture.h"
#include "../cast/cast.h"
#include "../modes/modes.h"
#include "usb_proto.h"

#define SWITCHCAST_USB_VENDOR_ID UINT16_C(0x18D1)
#define SWITCHCAST_USB_PRODUCT_ID UINT16_C(0x4EE0)

static atomic_uint UsbStatus = USB_STATUS_OFF;
static bool UsbInitialized;

uint32_t UsbModeGetStatus(void)
{
	return atomic_load(&UsbStatus);
}

static bool UsbWriteExact(const void* data, size_t size)
{
	return
		atomic_load(&IsThreadRunning) &&
		usbCommsWrite(data, size) == size;
}

static bool UsbConnectClient(void)
{
	uint8_t requestBytes[SWITCHCAST_USB_REQUEST_SIZE];
	uint8_t response[SWITCHCAST_USB_RESPONSE_SIZE];
	SwitchCastUsbRequest request;

	atomic_store(&UsbStatus, USB_STATUS_WAITING_DOCK);
	if (!UsbWriteExact(
		SWITCHCAST_USB_HELLO,
		SWITCHCAST_USB_HELLO_SIZE))
		return false;
	if (
		!atomic_load(&IsThreadRunning) ||
		usbCommsRead(requestBytes, sizeof(requestBytes)) !=
			sizeof(requestBytes))
		return false;

	const SwitchCastUsbHandshakeResult result =
		SwitchCastUsbParseRequest(
			requestBytes,
			sizeof(requestBytes),
			&request);
	SwitchCastUsbBuildResponse(result, response);
	if (!UsbWriteExact(response, sizeof(response)))
		return false;
	if (result != SwitchCastUsbHandshake_Ok)
		return false;

	CaptureConfigResetDefault();
	CaptureSetNalHashing(false, false);
	CaptureSetPPSSPSInject(request.injectSpsPps);
	CaptureSetPPSSPSInterval(1);
	CaptureVideoConnected();
	atomic_store(&UsbStatus, USB_STATUS_WAITING_GAME);
	return true;
}

static void UsbDisconnectClient(void)
{
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

static void UsbVideoThread(void* unused)
{
	(void)unused;
	if (!UsbInitialized) {
		while (atomic_load(&IsThreadRunning))
			svcSleepThread(500E+6L);
		return;
	}

	while (atomic_load(&IsThreadRunning)) {
		if (!UsbConnectClient()) {
			if (atomic_load(&IsThreadRunning))
				svcSleepThread(500E+6L);
			continue;
		}

		while (atomic_load(&IsThreadRunning)) {
			const bool captured = CaptureReadVideo();
			if (!atomic_load(&IsThreadRunning))
				break;

			const size_t packetSize =
				sizeof(PacketHeader) + VPkt.Header.DataSize;
			if (!UsbWriteExact(&VPkt, packetSize)) {
				atomic_store(&UsbStatus, USB_STATUS_IO_ERROR);
				break;
			}

			if (!captured) {
				atomic_store(&UsbStatus, USB_STATUS_CAPTURE_ERROR);
				break;
			}

			atomic_store(&UsbStatus, USB_STATUS_STREAMING);
			Cast_UpdateConsoleScreenBlanking(true);
		}

		UsbDisconnectClient();
		if (atomic_load(&IsThreadRunning))
			svcSleepThread(500E+6L);
	}
	UsbDisconnectClient();
}

static void UsbInitializeMode(void)
{
	const UsbCommsInterfaceInfo interfaceInfo = {
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceProtocol = USB_CLASS_VENDOR_SPEC,
	};

	UsbInitialized = false;
	atomic_store(&UsbStatus, USB_STATUS_INITIALIZING);
	usbCommsSetErrorHandling(false);
	const Result rc = usbCommsInitializeEx(
		1,
		&interfaceInfo,
		SWITCHCAST_USB_VENDOR_ID,
		SWITCHCAST_USB_PRODUCT_ID);
	if (R_FAILED(rc)) {
		LOG("usbCommsInitializeEx failed: %x\n", rc);
		atomic_store(&UsbStatus, USB_STATUS_INIT_ERROR);
		return;
	}

	UsbInitialized = true;
	atomic_store(&UsbStatus, USB_STATUS_WAITING_DOCK);
}

static void UsbExitMode(void)
{
	Cast_UpdateConsoleScreenBlanking(false);
	if (UsbInitialized)
		usbCommsExit();
	UsbInitialized = false;
	atomic_store(&UsbStatus, USB_STATUS_OFF);
}

const StreamMode USB_MODE = {
	UsbInitializeMode,
	UsbExitMode,
	UsbVideoThread,
	NULL
};
