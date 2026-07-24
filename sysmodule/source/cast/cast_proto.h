#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAST_PROTO_ID_MAX 96
#define CAST_PROTO_NAMESPACE_MAX 128
#define CAST_PROTO_PAYLOAD_MAX 6144

typedef struct {
	char sourceId[CAST_PROTO_ID_MAX];
	char destinationId[CAST_PROTO_ID_MAX];
	char nameSpace[CAST_PROTO_NAMESPACE_MAX];
	char payload[CAST_PROTO_PAYLOAD_MAX];
} CastProtoMessage;

// Encodes a Cast V2 CastMessage using protocol version CASTV2_1_0 and a UTF-8
// payload. Returns zero when the destination buffer is too small.
size_t CastProtoEncode(
	uint8_t* out,
	size_t capacity,
	const char* sourceId,
	const char* destinationId,
	const char* nameSpace,
	const char* payload);

// Decodes the fields used by the sender. Unknown protobuf fields are skipped.
bool CastProtoDecode(const uint8_t* bytes, size_t size, CastProtoMessage* out);
