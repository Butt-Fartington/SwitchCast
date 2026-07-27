/*
 * SwitchCast timeout-aware USB transport.
 *
 * This is an adaptation of SysDVR's UsbComms.c and Serial.c by exelix11 and
 * contributors. The important behavioral property is retained: every USB
 * transfer has a finite timeout and a timed-out endpoint request is cancelled.
 * That prevents an old gameplay packet from surviving a physical unplug and
 * being delivered as the first packet of a new host session.
 *
 * GPL-2.0-only; see SYSDVR-ATTRIBUTION.md.
 */
#include "usb_transport.h"

#include <stdint.h>
#include <string.h>

#include "../modes/modes.h"

#define SWITCHCAST_USB_VENDOR_ID UINT16_C(0x18D1)
#define SWITCHCAST_USB_PRODUCT_ID UINT16_C(0x4EE0)
#define USB_TRANSFER_TIMEOUT_NS UINT64_C(1000000000)
#define USB_CANCEL_DRAIN_TIMEOUT_NS UINT64_C(3000000000)
#define USB_PAGE_SIZE UINT32_C(0x1000)

typedef struct {
	UsbDsInterface* Interface;
	UsbDsEndpoint* EndpointIn;
	UsbDsEndpoint* EndpointOut;
	bool ServiceInitialized;
	bool InterfaceInitialized;
} UsbTransportState;

static UsbTransportState Transport;

static Result AppendConfiguration(
	UsbDeviceSpeed speed,
	struct usb_interface_descriptor* interfaceDescriptor,
	struct usb_endpoint_descriptor* endpointIn,
	struct usb_endpoint_descriptor* endpointOut,
	bool superSpeed)
{
	Result rc = usbDsInterface_AppendConfigurationData(
		Transport.Interface,
		speed,
		interfaceDescriptor,
		USB_DT_INTERFACE_SIZE);
	if (R_FAILED(rc))
		return rc;

	rc = usbDsInterface_AppendConfigurationData(
		Transport.Interface,
		speed,
		endpointIn,
		USB_DT_ENDPOINT_SIZE);
	if (R_FAILED(rc))
		return rc;

	if (superSpeed) {
		const struct usb_ss_endpoint_companion_descriptor companion = {
			.bLength = sizeof(companion),
			.bDescriptorType = USB_DT_SS_ENDPOINT_COMPANION,
			.bMaxBurst = 0x0F,
			.bmAttributes = 0,
			.wBytesPerInterval = 0,
		};
		rc = usbDsInterface_AppendConfigurationData(
			Transport.Interface,
			speed,
			&companion,
			USB_DT_SS_ENDPOINT_COMPANION_SIZE);
		if (R_FAILED(rc))
			return rc;
	}

	rc = usbDsInterface_AppendConfigurationData(
		Transport.Interface,
		speed,
		endpointOut,
		USB_DT_ENDPOINT_SIZE);
	if (R_FAILED(rc))
		return rc;

	if (superSpeed) {
		const struct usb_ss_endpoint_companion_descriptor companion = {
			.bLength = sizeof(companion),
			.bDescriptorType = USB_DT_SS_ENDPOINT_COMPANION,
			.bMaxBurst = 0x0F,
			.bmAttributes = 0,
			.wBytesPerInterval = 0,
		};
		rc = usbDsInterface_AppendConfigurationData(
			Transport.Interface,
			speed,
			&companion,
			USB_DT_SS_ENDPOINT_COMPANION_SIZE);
	}
	return rc;
}

static Result ConfigureDeviceDescriptors(void)
{
	u8 manufacturer;
	u8 product;
	u8 serial;
	static const u16 languages[] = { UINT16_C(0x0409) };

	Result rc = usbDsAddUsbLanguageStringDescriptor(
		NULL,
		languages,
		sizeof(languages) / sizeof(languages[0]));
	if (R_SUCCEEDED(rc))
		rc = usbDsAddUsbStringDescriptor(
			&manufacturer,
			"SwitchCast / SysDVR");
	if (R_SUCCEEDED(rc))
		rc = usbDsAddUsbStringDescriptor(
			&product,
			"SwitchCast Dock");
	if (R_SUCCEEDED(rc))
		rc = usbDsAddUsbStringDescriptor(
			&serial,
			"SwitchCast");
	if (R_FAILED(rc))
		return rc;

	struct usb_device_descriptor descriptor = {
		.bLength = USB_DT_DEVICE_SIZE,
		.bDescriptorType = USB_DT_DEVICE,
		.bcdUSB = UINT16_C(0x0110),
		.bDeviceClass = 0,
		.bDeviceSubClass = 0,
		.bDeviceProtocol = 0,
		.bMaxPacketSize0 = 0x40,
		.idVendor = SWITCHCAST_USB_VENDOR_ID,
		.idProduct = SWITCHCAST_USB_PRODUCT_ID,
		.bcdDevice = UINT16_C(0x0100),
		.iManufacturer = manufacturer,
		.iProduct = product,
		.iSerialNumber = serial,
		.bNumConfigurations = 1,
	};
	rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Full, &descriptor);
	if (R_FAILED(rc))
		return rc;

	descriptor.bcdUSB = UINT16_C(0x0200);
	rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_High, &descriptor);
	if (R_FAILED(rc))
		return rc;

	descriptor.bcdUSB = UINT16_C(0x0300);
	descriptor.bMaxPacketSize0 = 0x09;
	rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Super, &descriptor);
	if (R_FAILED(rc))
		return rc;

	const u8 bos[] = {
		0x05, USB_DT_BOS, 0x16, 0x00, 0x02,
		0x07, USB_DT_DEVICE_CAPABILITY, 0x02,
		0x02, 0x00, 0x00, 0x00,
		0x0A, USB_DT_DEVICE_CAPABILITY, 0x03,
		0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00,
	};
	return usbDsSetBinaryObjectStore(bos, sizeof(bos));
}

static Result ConfigureStreamingInterface(void)
{
	struct usb_interface_descriptor interfaceDescriptor = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 4,
		.bNumEndpoints = 2,
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceProtocol = USB_CLASS_VENDOR_SPEC,
	};
	struct usb_endpoint_descriptor endpointIn = {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = USB_ENDPOINT_IN,
		.bmAttributes = USB_TRANSFER_TYPE_BULK,
		.wMaxPacketSize = 0x40,
	};
	struct usb_endpoint_descriptor endpointOut = {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = USB_ENDPOINT_OUT,
		.bmAttributes = USB_TRANSFER_TYPE_BULK,
		.wMaxPacketSize = 0x40,
	};

	Result rc = usbDsRegisterInterface(&Transport.Interface);
	if (R_FAILED(rc))
		return rc;

	interfaceDescriptor.bInterfaceNumber =
		Transport.Interface->interface_index;
	endpointIn.bEndpointAddress +=
		interfaceDescriptor.bInterfaceNumber + 1;
	endpointOut.bEndpointAddress +=
		interfaceDescriptor.bInterfaceNumber + 1;

	rc = AppendConfiguration(
		UsbDeviceSpeed_Full,
		&interfaceDescriptor,
		&endpointIn,
		&endpointOut,
		false);
	if (R_FAILED(rc))
		return rc;

	endpointIn.wMaxPacketSize = 0x200;
	endpointOut.wMaxPacketSize = 0x200;
	rc = AppendConfiguration(
		UsbDeviceSpeed_High,
		&interfaceDescriptor,
		&endpointIn,
		&endpointOut,
		false);
	if (R_FAILED(rc))
		return rc;

	endpointIn.wMaxPacketSize = 0x400;
	endpointOut.wMaxPacketSize = 0x400;
	rc = AppendConfiguration(
		UsbDeviceSpeed_Super,
		&interfaceDescriptor,
		&endpointIn,
		&endpointOut,
		true);
	if (R_FAILED(rc))
		return rc;

	rc = usbDsInterface_RegisterEndpoint(
		Transport.Interface,
		&Transport.EndpointIn,
		endpointIn.bEndpointAddress);
	if (R_FAILED(rc))
		return rc;
	rc = usbDsInterface_RegisterEndpoint(
		Transport.Interface,
		&Transport.EndpointOut,
		endpointOut.bEndpointAddress);
	if (R_FAILED(rc))
		return rc;
	rc = usbDsInterface_EnableInterface(Transport.Interface);
	if (R_SUCCEEDED(rc))
		Transport.InterfaceInitialized = true;
	return rc;
}

Result UsbTransportInitialize(void)
{
	memset(&Transport, 0, sizeof(Transport));
	memset(Buffers.UsbMode.EndpointPages, 0,
		sizeof(Buffers.UsbMode.EndpointPages));

	Result rc = usbDsInitialize();
	if (R_FAILED(rc))
		return rc;
	Transport.ServiceInitialized = true;

	/*
	 * SwitchCast targets current Atmosphere/HOS installations. The modern
	 * usb:ds descriptor API is required for the standard 18d1:4ee0 identity.
	 */
	if (!hosversionAtLeast(5, 0, 0)) {
		rc = MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
		goto fail;
	}

	rc = ConfigureDeviceDescriptors();
	if (R_FAILED(rc))
		goto fail;
	rc = ConfigureStreamingInterface();
	if (R_FAILED(rc))
		goto fail;
	rc = usbDsEnable();
	if (R_FAILED(rc))
		goto fail;
	return 0;

fail:
	UsbTransportExit();
	return rc;
}

void UsbTransportExit(void)
{
	if (Transport.InterfaceInitialized && Transport.Interface != NULL)
		usbDsInterface_Close(Transport.Interface);
	if (Transport.ServiceInitialized)
		usbDsExit();
	memset(&Transport, 0, sizeof(Transport));
}

static Result WaitForTransfer(
	UsbDsEndpoint* endpoint,
	Event* event)
{
	Result rc = eventWait(event, USB_TRANSFER_TIMEOUT_NS);
	if (R_FAILED(rc)) {
		usbDsEndpoint_Cancel(endpoint);
		rc = eventWait(event, USB_CANCEL_DRAIN_TIMEOUT_NS);
	}
	if (R_SUCCEEDED(rc))
		eventClear(event);
	return rc;
}

static size_t ReadTransfer(void* data, size_t size)
{
	if (
		!Transport.InterfaceInitialized ||
		data == NULL ||
		size == 0)
		return 0;

	Result rc = usbDsWaitReady(USB_TRANSFER_TIMEOUT_NS);
	if (R_FAILED(rc))
		return 0;

	u8* destination = data;
	size_t total = 0;
	while (size != 0) {
		u8* transferBuffer;
		u32 chunk;
		const uintptr_t offset =
			(uintptr_t)destination & (USB_PAGE_SIZE - 1);

		if (offset != 0) {
			transferBuffer = Buffers.UsbMode.EndpointPages[1];
			chunk = USB_PAGE_SIZE - (u32)offset;
			if (size < chunk)
				chunk = (u32)size;
			memset(transferBuffer, 0, USB_PAGE_SIZE);
		} else {
			transferBuffer = destination;
			chunk = (u32)size;
		}

		u32 urbId = 0;
		rc = usbDsEndpoint_PostBufferAsync(
			Transport.EndpointOut,
			transferBuffer,
			chunk,
			&urbId);
		if (R_FAILED(rc))
			break;
		rc = WaitForTransfer(
			Transport.EndpointOut,
			&Transport.EndpointOut->CompletionEvent);
		if (R_FAILED(rc))
			break;

		UsbDsReportData report;
		rc = usbDsEndpoint_GetReportData(
			Transport.EndpointOut,
			&report);
		if (R_FAILED(rc))
			break;
		u32 transferred = 0;
		rc = usbDsParseReportData(
			&report,
			urbId,
			NULL,
			&transferred);
		if (R_FAILED(rc))
			break;
		if (transferred > chunk)
			transferred = chunk;
		if (transferBuffer != destination)
			memcpy(destination, transferBuffer, transferred);

		total += transferred;
		destination += transferred;
		size -= transferred;
		if (transferred < chunk)
			break;
	}
	return total;
}

static size_t WriteTransfer(const void* data, size_t size)
{
	if (
		!Transport.InterfaceInitialized ||
		data == NULL ||
		size == 0)
		return 0;

	Result rc = usbDsWaitReady(USB_TRANSFER_TIMEOUT_NS);
	if (R_FAILED(rc))
		return 0;

	const u8* source = data;
	size_t total = 0;
	while (size != 0) {
		u8* transferBuffer;
		u32 chunk;
		const uintptr_t offset =
			(uintptr_t)source & (USB_PAGE_SIZE - 1);

		if (offset != 0) {
			transferBuffer = Buffers.UsbMode.EndpointPages[0];
			chunk = USB_PAGE_SIZE - (u32)offset;
			if (size < chunk)
				chunk = (u32)size;
			memset(transferBuffer, 0, USB_PAGE_SIZE);
			memcpy(transferBuffer, source, chunk);
		} else {
			transferBuffer = (u8*)source;
			chunk = (u32)size;
		}

		u32 urbId = 0;
		rc = usbDsEndpoint_PostBufferAsync(
			Transport.EndpointIn,
			transferBuffer,
			chunk,
			&urbId);
		if (R_FAILED(rc))
			break;
		rc = WaitForTransfer(
			Transport.EndpointIn,
			&Transport.EndpointIn->CompletionEvent);
		if (R_FAILED(rc))
			break;

		UsbDsReportData report;
		rc = usbDsEndpoint_GetReportData(
			Transport.EndpointIn,
			&report);
		if (R_FAILED(rc))
			break;
		u32 transferred = 0;
		rc = usbDsParseReportData(
			&report,
			urbId,
			NULL,
			&transferred);
		if (R_FAILED(rc))
			break;
		if (transferred > chunk)
			transferred = chunk;

		total += transferred;
		source += transferred;
		size -= transferred;
		if (transferred < chunk)
			break;
	}
	return total;
}

bool UsbTransportRead(void* data, size_t size)
{
	return ReadTransfer(data, size) == size;
}

bool UsbTransportWrite(const void* data, size_t size)
{
	return WriteTransfer(data, size) == size;
}
