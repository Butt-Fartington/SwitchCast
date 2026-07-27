/*
 * SysDVR-compatible USB video handshake used by SwitchCast.
 *
 * The wire format originates in SysDVR by Exelix11 and contributors.
 * SwitchCast remains GPL-2.0-only; see SYSDVR-ATTRIBUTION.md.
 */
#include "usb_proto.h"

#include <limits.h>
#include <string.h>

#define REQUEST_MAGIC UINT32_C(0xAAAAAAAA)
#define META_VIDEO (UINT8_C(1) << 0)
#define META_AUDIO (UINT8_C(1) << 1)
#define VIDEO_INJECT_SPS_PPS (UINT8_C(1) << 1)
#define FEATURE_KEEPALIVE (UINT8_C(1) << 2)

static uint32_t ReadLe32(const uint8_t* bytes)
{
	return
		(uint32_t)bytes[0] |
		((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) |
		((uint32_t)bytes[3] << 24);
}

static void WriteLe32(uint8_t* bytes, uint32_t value)
{
	bytes[0] = (uint8_t)value;
	bytes[1] = (uint8_t)(value >> 8);
	bytes[2] = (uint8_t)(value >> 16);
	bytes[3] = (uint8_t)(value >> 24);
}

SwitchCastUsbHandshakeResult SwitchCastUsbParseRequest(
	const uint8_t* bytes,
	size_t size,
	SwitchCastUsbRequest* request)
{
	if (!bytes || !request || size != SWITCHCAST_USB_REQUEST_SIZE)
		return SwitchCastUsbHandshake_InvalidSize;
	memset(request, 0, sizeof(*request));

	if (ReadLe32(bytes) != REQUEST_MAGIC)
		return SwitchCastUsbHandshake_WrongMagic;
	if (bytes[4] != '0' || bytes[5] != '3')
		return SwitchCastUsbHandshake_WrongVersion;
	if ((bytes[6] & (META_VIDEO | META_AUDIO)) == 0)
		return SwitchCastUsbHandshake_InvalidMeta;
	if ((bytes[6] & META_VIDEO) == 0)
		return SwitchCastUsbHandshake_InvalidMeta;

	request->injectSpsPps =
		(bytes[7] & VIDEO_INJECT_SPS_PPS) != 0;
	request->audioEnabled = (bytes[6] & META_AUDIO) != 0;
	request->keepaliveEnabled =
		(bytes[9] & FEATURE_KEEPALIVE) != 0;
	return SwitchCastUsbHandshake_Ok;
}

void SwitchCastUsbBuildResponse(
	SwitchCastUsbHandshakeResult result,
	uint8_t response[SWITCHCAST_USB_RESPONSE_SIZE])
{
	memset(response, 0, SWITCHCAST_USB_RESPONSE_SIZE);
	WriteLe32(response, (uint32_t)result);

	/*
	 * Protocol 03 reserves the remaining response for a memory diagnostic.
	 * UINT32_MAX means the client did not request that diagnostic.
	 */
	WriteLe32(response + 4, UINT32_MAX);
}
