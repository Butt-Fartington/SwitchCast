#include "cast_proto.h"

#include <string.h>

enum {
	WireTypeVarint = 0,
	WireTypeFixed64 = 1,
	WireTypeLengthDelimited = 2,
	WireTypeFixed32 = 5
};

typedef struct {
	uint8_t* current;
	uint8_t* end;
} ProtoWriter;

static bool WriteVarint(ProtoWriter* writer, uint64_t value)
{
	do {
		if (writer->current == writer->end)
			return false;
		uint8_t byte = (uint8_t)(value & 0x7F);
		value >>= 7;
		if (value)
			byte |= 0x80;
		*writer->current++ = byte;
	} while (value);

	return true;
}

static bool WriteTag(ProtoWriter* writer, uint32_t field, uint32_t wireType)
{
	return WriteVarint(writer, ((uint64_t)field << 3) | wireType);
}

static bool WriteString(ProtoWriter* writer, uint32_t field, const char* value)
{
	const size_t length = strlen(value);
	if (!WriteTag(writer, field, WireTypeLengthDelimited) ||
		!WriteVarint(writer, length) ||
		(size_t)(writer->end - writer->current) < length)
		return false;

	memcpy(writer->current, value, length);
	writer->current += length;
	return true;
}

size_t CastProtoEncode(
	uint8_t* out,
	size_t capacity,
	const char* sourceId,
	const char* destinationId,
	const char* nameSpace,
	const char* payload)
{
	if (!out || !sourceId || !destinationId || !nameSpace || !payload)
		return 0;

	ProtoWriter writer = { out, out + capacity };

	// required ProtocolVersion protocol_version = 1; CASTV2_1_0 = 0
	if (!WriteTag(&writer, 1, WireTypeVarint) || !WriteVarint(&writer, 0) ||
		!WriteString(&writer, 2, sourceId) ||
		!WriteString(&writer, 3, destinationId) ||
		!WriteString(&writer, 4, nameSpace) ||
		// required PayloadType payload_type = 5; STRING = 0
		!WriteTag(&writer, 5, WireTypeVarint) || !WriteVarint(&writer, 0) ||
		!WriteString(&writer, 6, payload))
		return 0;

	return (size_t)(writer.current - out);
}

static bool ReadVarint(
	const uint8_t** current,
	const uint8_t* end,
	uint64_t* value)
{
	uint64_t result = 0;

	for (unsigned shift = 0; shift < 64; shift += 7) {
		if (*current == end)
			return false;
		const uint8_t byte = *(*current)++;
		result |= (uint64_t)(byte & 0x7F) << shift;
		if (!(byte & 0x80)) {
			*value = result;
			return true;
		}
	}

	return false;
}

static bool CopyString(
	const uint8_t* bytes,
	size_t size,
	char* out,
	size_t capacity)
{
	if (capacity == 0 || size >= capacity)
		return false;
	memcpy(out, bytes, size);
	out[size] = '\0';
	return true;
}

bool CastProtoDecode(const uint8_t* bytes, size_t size, CastProtoMessage* out)
{
	if (!bytes || !out)
		return false;

	memset(out, 0, sizeof(*out));
	const uint8_t* current = bytes;
	const uint8_t* end = bytes + size;

	while (current < end) {
		uint64_t tag;
		if (!ReadVarint(&current, end, &tag))
			return false;

		const uint32_t field = (uint32_t)(tag >> 3);
		const uint32_t wireType = (uint32_t)(tag & 7);

		if (wireType == WireTypeVarint) {
			uint64_t ignored;
			if (!ReadVarint(&current, end, &ignored))
				return false;
		}
		else if (wireType == WireTypeLengthDelimited) {
			uint64_t length;
			if (!ReadVarint(&current, end, &length) ||
				length > (uint64_t)(end - current))
				return false;

			bool copied = true;
			switch (field) {
			case 2:
				copied = CopyString(current, (size_t)length, out->sourceId, sizeof(out->sourceId));
				break;
			case 3:
				copied = CopyString(current, (size_t)length, out->destinationId, sizeof(out->destinationId));
				break;
			case 4:
				copied = CopyString(current, (size_t)length, out->nameSpace, sizeof(out->nameSpace));
				break;
			case 6:
				copied = CopyString(current, (size_t)length, out->payload, sizeof(out->payload));
				break;
			default:
				break;
			}
			if (!copied)
				return false;
			current += length;
		}
		else if (wireType == WireTypeFixed64) {
			if ((size_t)(end - current) < 8)
				return false;
			current += 8;
		}
		else if (wireType == WireTypeFixed32) {
			if ((size_t)(end - current) < 4)
				return false;
			current += 4;
		}
		else {
			return false;
		}
	}

	return out->sourceId[0] && out->destinationId[0] &&
		out->nameSpace[0] && out->payload[0];
}
