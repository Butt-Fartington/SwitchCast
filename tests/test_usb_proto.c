#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../sysmodule/source/dock/usb_proto.h"

static uint32_t ReadLe32(const uint8_t* bytes)
{
	return
		(uint32_t)bytes[0] |
		((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) |
		((uint32_t)bytes[3] << 24);
}

int main(void)
{
	uint8_t request[SWITCHCAST_USB_REQUEST_SIZE] = {
		0xAA, 0xAA, 0xAA, 0xAA,
		'0', '3',
		1U << 0,
		1U << 1,
	};
	uint8_t response[SWITCHCAST_USB_RESPONSE_SIZE];
	SwitchCastUsbRequest parsed;

	assert(
		SwitchCastUsbParseRequest(request, sizeof(request), &parsed) ==
		SwitchCastUsbHandshake_Ok);
	assert(parsed.injectSpsPps);
	assert(!parsed.audioEnabled);
	assert(!parsed.keepaliveEnabled);

	request[6] |= 1U << 1;
	assert(
		SwitchCastUsbParseRequest(request, sizeof(request), &parsed) ==
		SwitchCastUsbHandshake_Ok);
	assert(parsed.audioEnabled);
	request[9] |= 1U << 2;
	assert(
		SwitchCastUsbParseRequest(request, sizeof(request), &parsed) ==
		SwitchCastUsbHandshake_Ok);
	assert(parsed.keepaliveEnabled);
	request[6] = 1U << 0;
	request[9] = 0;

	request[4] = '0';
	request[5] = '2';
	assert(
		SwitchCastUsbParseRequest(request, sizeof(request), &parsed) ==
		SwitchCastUsbHandshake_WrongVersion);
	request[5] = '3';

	request[0] = 0;
	assert(
		SwitchCastUsbParseRequest(request, sizeof(request), &parsed) ==
		SwitchCastUsbHandshake_WrongMagic);
	request[0] = 0xAA;

	assert(
		SwitchCastUsbParseRequest(request, sizeof(request) - 1, &parsed) ==
		SwitchCastUsbHandshake_InvalidSize);

	memset(response, 0xCC, sizeof(response));
	SwitchCastUsbBuildResponse(SwitchCastUsbHandshake_Ok, response);
	assert(ReadLe32(response) == SwitchCastUsbHandshake_Ok);
	assert(ReadLe32(response + 4) == UINT32_MAX);
	for (size_t i = 8; i < sizeof(response); ++i)
		assert(response[i] == 0);

	puts("USB protocol tests passed");
	return 0;
}
