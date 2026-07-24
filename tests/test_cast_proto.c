#include "../sysmodule/source/cast/cast_proto.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	uint8_t bytes[1024];
	const char* payload = "{\"type\":\"PING\"}";
	size_t size = CastProtoEncode(
		bytes,
		sizeof(bytes),
		"sender-0",
		"receiver-0",
		"urn:x-cast:com.google.cast.tp.heartbeat",
		payload);
	assert(size > 0);

	CastProtoMessage decoded;
	assert(CastProtoDecode(bytes, size, &decoded));
	assert(strcmp(decoded.sourceId, "sender-0") == 0);
	assert(strcmp(decoded.destinationId, "receiver-0") == 0);
	assert(strcmp(decoded.nameSpace, "urn:x-cast:com.google.cast.tp.heartbeat") == 0);
	assert(strcmp(decoded.payload, payload) == 0);

	static char largePayload[5001];
	static uint8_t largeBytes[8192];
	memset(largePayload, 'A', sizeof(largePayload) - 1);
	largePayload[sizeof(largePayload) - 1] = '\0';
	size = CastProtoEncode(
		largeBytes,
		sizeof(largeBytes),
		"receiver-0",
		"sender-0",
		"urn:x-cast:com.google.cast.receiver",
		largePayload);
	assert(size > 4096);
	assert(CastProtoDecode(largeBytes, size, &decoded));
	assert(strcmp(decoded.payload, largePayload) == 0);

	assert(CastProtoEncode(
		bytes,
		4,
		"sender-0",
		"receiver-0",
		"urn:x-cast:test",
		payload) == 0);
	assert(!CastProtoDecode(bytes, 1, &decoded));

	puts("cast protobuf tests passed");
	return 0;
}
