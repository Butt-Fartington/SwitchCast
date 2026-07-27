/*
 * SysDVR-compatible USB video handshake used by SwitchCast.
 *
 * The wire format originates in SysDVR by Exelix11 and contributors.
 * SwitchCast remains GPL-2.0-only; see SYSDVR-ATTRIBUTION.md.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWITCHCAST_USB_HELLO "SysDVR|03"
#define SWITCHCAST_USB_HELLO_SIZE 10U
#define SWITCHCAST_USB_REQUEST_SIZE 16U
#define SWITCHCAST_USB_RESPONSE_SIZE 72U

typedef enum {
	SwitchCastUsbHandshake_UnknownFailure = 0,
	SwitchCastUsbHandshake_WrongVersion = 1,
	SwitchCastUsbHandshake_InvalidArg = 2,
	SwitchCastUsbHandshake_InvalidSize = 3,
	SwitchCastUsbHandshake_InvalidMeta = 4,
	SwitchCastUsbHandshake_WrongMagic = 5,
	SwitchCastUsbHandshake_Ok = 6,
	SwitchCastUsbHandshake_InvalidChannel = 7,
} SwitchCastUsbHandshakeResult;

typedef struct {
	bool injectSpsPps;
} SwitchCastUsbRequest;

SwitchCastUsbHandshakeResult SwitchCastUsbParseRequest(
	const uint8_t* bytes,
	size_t size,
	SwitchCastUsbRequest* request);

void SwitchCastUsbBuildResponse(
	SwitchCastUsbHandshakeResult result,
	uint8_t response[SWITCHCAST_USB_RESPONSE_SIZE]);
