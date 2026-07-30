#include "fmp4.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	uint8_t* data;
	size_t capacity;
	size_t position;
	bool failed;
} Mp4Writer;

typedef struct {
	const uint8_t* data;
	size_t size;
	uint8_t type;
} AnnexBNal;

static void WriteBytes(Mp4Writer* writer, const void* data, size_t size)
{
	if (writer->failed || size > writer->capacity - writer->position) {
		writer->failed = true;
		return;
	}
	memcpy(writer->data + writer->position, data, size);
	writer->position += size;
}

static void WriteU8(Mp4Writer* writer, uint8_t value)
{
	WriteBytes(writer, &value, 1);
}

static void WriteU16(Mp4Writer* writer, uint16_t value)
{
	const uint8_t bytes[] = {
		(uint8_t)(value >> 8),
		(uint8_t)value
	};
	WriteBytes(writer, bytes, sizeof(bytes));
}

static void WriteU24(Mp4Writer* writer, uint32_t value)
{
	const uint8_t bytes[] = {
		(uint8_t)(value >> 16),
		(uint8_t)(value >> 8),
		(uint8_t)value
	};
	WriteBytes(writer, bytes, sizeof(bytes));
}

static void WriteU32(Mp4Writer* writer, uint32_t value)
{
	const uint8_t bytes[] = {
		(uint8_t)(value >> 24),
		(uint8_t)(value >> 16),
		(uint8_t)(value >> 8),
		(uint8_t)value
	};
	WriteBytes(writer, bytes, sizeof(bytes));
}

static void WriteU64(Mp4Writer* writer, uint64_t value)
{
	WriteU32(writer, (uint32_t)(value >> 32));
	WriteU32(writer, (uint32_t)value);
}

static size_t BeginBox(Mp4Writer* writer, const char type[4])
{
	const size_t start = writer->position;
	WriteU32(writer, 0);
	WriteBytes(writer, type, 4);
	return start;
}

static void PatchU32(Mp4Writer* writer, size_t offset, uint32_t value)
{
	if (writer->failed || offset > writer->position ||
		writer->position - offset < 4) {
		writer->failed = true;
		return;
	}
	writer->data[offset] = (uint8_t)(value >> 24);
	writer->data[offset + 1] = (uint8_t)(value >> 16);
	writer->data[offset + 2] = (uint8_t)(value >> 8);
	writer->data[offset + 3] = (uint8_t)value;
}

static void EndBox(Mp4Writer* writer, size_t start)
{
	if (writer->failed || writer->position < start ||
		writer->position - start > UINT32_MAX) {
		writer->failed = true;
		return;
	}
	PatchU32(writer, start, (uint32_t)(writer->position - start));
}

static void WriteZeros(Mp4Writer* writer, size_t size)
{
	static const uint8_t zeros[64];
	while (size) {
		const size_t chunk = size < sizeof(zeros) ? size : sizeof(zeros);
		WriteBytes(writer, zeros, chunk);
		size -= chunk;
	}
}

static bool FindStartCode(
	const uint8_t* data,
	size_t size,
	size_t from,
	size_t* offset,
	size_t* length)
{
	for (size_t i = from; i + 3 <= size; ++i) {
		if (data[i] != 0 || data[i + 1] != 0)
			continue;
		if (data[i + 2] == 1) {
			*offset = i;
			*length = 3;
			return true;
		}
		if (i + 4 <= size && data[i + 2] == 0 && data[i + 3] == 1) {
			*offset = i;
			*length = 4;
			return true;
		}
	}
	return false;
}

static bool NextNal(
	const uint8_t* data,
	size_t size,
	size_t* cursor,
	AnnexBNal* nal)
{
	size_t start = 0;
	size_t startCodeSize = 0;
	if (!FindStartCode(data, size, *cursor, &start, &startCodeSize))
		return false;

	const size_t nalStart = start + startCodeSize;
	size_t next = 0;
	size_t nextStartCodeSize = 0;
	const bool hasNext = FindStartCode(
		data,
		size,
		nalStart,
		&next,
		&nextStartCodeSize);
	(void)nextStartCodeSize;
	size_t nalEnd = hasNext ? next : size;
	while (nalEnd > nalStart && data[nalEnd - 1] == 0)
		--nalEnd;

	*cursor = hasNext ? next : size;
	if (nalEnd <= nalStart) {
		if (*cursor >= size)
			return false;
		return NextNal(data, size, cursor, nal);
	}

	nal->data = data + nalStart;
	nal->size = nalEnd - nalStart;
	nal->type = nal->data[0] & 0x1F;
	return true;
}

static bool SaveParameterSet(
	uint8_t* destination,
	size_t* destinationSize,
	size_t capacity,
	const AnnexBNal* nal)
{
	if (nal->size == 0 || nal->size > capacity)
		return false;
	memcpy(destination, nal->data, nal->size);
	*destinationSize = nal->size;
	return true;
}

static bool SaveCastSps(
	uint8_t* destination,
	size_t* destinationSize,
	size_t capacity,
	const AnnexBNal* nal)
{
	/*
	 * GRC's fixed 720p30 SPS omits VUI bitstream restrictions. For High
	 * Profile Level 3.2 that makes a decoder infer the level-sized DPB and
	 * frame-reordering allowance, even though GRC emits no B-frames and uses
	 * only one reference picture.
	 *
	 * The Cast fMP4 path carries SPS/PPS only in avcC, so substitute an
	 * otherwise identical SPS with:
	 *   max_num_reorder_frames = 0
	 *   max_dec_frame_buffering = 1
	 *
	 * The value 1 is the legal minimum because max_num_ref_frames is 1.
	 * Match the complete known SPS so an unknown firmware encoder is never
	 * rewritten speculatively. USB Dock mode does not use this module.
	 */
	static const uint8_t grcSps[] = {
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
		0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE0, 0x80,
	};
	static const uint8_t lowDelaySps[] = {
		0x67, 0x64, 0x0C, 0x20, 0xAC, 0x2B, 0x40, 0x28,
		0x02, 0xDD, 0x35, 0x01, 0x0D, 0x01, 0xE1, 0xB4,
		0x11, 0x08, 0xD4,
	};

	if (
		nal->size == sizeof(grcSps) &&
		memcmp(nal->data, grcSps, sizeof(grcSps)) == 0) {
		if (sizeof(lowDelaySps) > capacity)
			return false;
		memcpy(destination, lowDelaySps, sizeof(lowDelaySps));
		*destinationSize = sizeof(lowDelaySps);
		return true;
	}
	return SaveParameterSet(
		destination,
		destinationSize,
		capacity,
		nal);
}

void Fmp4Init(Fmp4Stream* stream)
{
	memset(stream, 0, sizeof(*stream));
	stream->nextSequence = 1;
}

bool Fmp4ObserveAccessUnit(
	Fmp4Stream* stream,
	const uint8_t* annexB,
	size_t annexBSize)
{
	if (!stream || !annexB || annexBSize == 0)
		return false;

	size_t cursor = 0;
	AnnexBNal nal;
	while (NextNal(annexB, annexBSize, &cursor, &nal)) {
		if (nal.type == 7)
			SaveCastSps(
				stream->sps,
				&stream->spsSize,
				sizeof(stream->sps),
				&nal);
		else if (nal.type == 8)
			SaveParameterSet(
				stream->pps,
				&stream->ppsSize,
				sizeof(stream->pps),
				&nal);
	}
	return Fmp4HasConfiguration(stream);
}

bool Fmp4HasConfiguration(const Fmp4Stream* stream)
{
	return stream &&
		stream->spsSize >= 4 &&
		stream->ppsSize >= 2;
}

bool Fmp4GetCodecString(
	const Fmp4Stream* stream,
	char* out,
	size_t capacity)
{
	if (!Fmp4HasConfiguration(stream) || !out || capacity == 0)
		return false;
	const int length = snprintf(
		out,
		capacity,
		"avc1.%02X%02X%02X",
		stream->sps[1],
		stream->sps[2],
		stream->sps[3]);
	return length > 0 && (size_t)length < capacity;
}

void Fmp4BeginMedia(Fmp4Stream* stream)
{
	if (!stream)
		return;
	stream->nextSequence = 1;
	stream->mediaStartTimestampUs = 0;
	stream->nextDecodeTime = 0;
	stream->mediaStarted = false;
}

static void WriteMatrix(Mp4Writer* writer)
{
	static const uint32_t matrix[] = {
		0x00010000, 0, 0,
		0, 0x00010000, 0,
		0, 0, 0x40000000
	};
	for (size_t i = 0; i < sizeof(matrix) / sizeof(matrix[0]); ++i)
		WriteU32(writer, matrix[i]);
}

static void WriteMvhd(Mp4Writer* writer)
{
	const size_t box = BeginBox(writer, "mvhd");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteU32(writer, FMP4_TIMESCALE);
	WriteU32(writer, 0);
	WriteU32(writer, 0x00010000);
	WriteU16(writer, 0x0100);
	WriteZeros(writer, 10);
	WriteMatrix(writer);
	WriteZeros(writer, 24);
	WriteU32(writer, 2);
	EndBox(writer, box);
}

static void WriteTkhd(Mp4Writer* writer, uint16_t width, uint16_t height)
{
	const size_t box = BeginBox(writer, "tkhd");
	WriteU8(writer, 0);
	WriteU24(writer, 7);
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteU32(writer, 1);
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteZeros(writer, 8);
	WriteU16(writer, 0);
	WriteU16(writer, 0);
	WriteU16(writer, 0);
	WriteU16(writer, 0);
	WriteMatrix(writer);
	WriteU32(writer, (uint32_t)width << 16);
	WriteU32(writer, (uint32_t)height << 16);
	EndBox(writer, box);
}

static void WriteMdhd(Mp4Writer* writer)
{
	const size_t box = BeginBox(writer, "mdhd");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteU32(writer, FMP4_TIMESCALE);
	WriteU32(writer, 0);
	WriteU16(writer, 0x55C4);
	WriteU16(writer, 0);
	EndBox(writer, box);
}

static void WriteHdlr(Mp4Writer* writer)
{
	static const char name[] = "VideoHandler";
	const size_t box = BeginBox(writer, "hdlr");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteBytes(writer, "vide", 4);
	WriteZeros(writer, 12);
	WriteBytes(writer, name, sizeof(name));
	EndBox(writer, box);
}

static void WriteAvcC(Mp4Writer* writer, const Fmp4Stream* stream)
{
	const size_t box = BeginBox(writer, "avcC");
	WriteU8(writer, 1);
	WriteU8(writer, stream->sps[1]);
	WriteU8(writer, stream->sps[2]);
	WriteU8(writer, stream->sps[3]);
	WriteU8(writer, 0xFF);
	WriteU8(writer, 0xE1);
	WriteU16(writer, (uint16_t)stream->spsSize);
	WriteBytes(writer, stream->sps, stream->spsSize);
	WriteU8(writer, 1);
	WriteU16(writer, (uint16_t)stream->ppsSize);
	WriteBytes(writer, stream->pps, stream->ppsSize);
	EndBox(writer, box);
}

static void WriteStsd(
	Mp4Writer* writer,
	const Fmp4Stream* stream,
	uint16_t width,
	uint16_t height)
{
	const size_t stsd = BeginBox(writer, "stsd");
	WriteU32(writer, 0);
	WriteU32(writer, 1);

	const size_t avc1 = BeginBox(writer, "avc1");
	WriteZeros(writer, 6);
	WriteU16(writer, 1);
	WriteU16(writer, 0);
	WriteU16(writer, 0);
	WriteZeros(writer, 12);
	WriteU16(writer, width);
	WriteU16(writer, height);
	WriteU32(writer, 0x00480000);
	WriteU32(writer, 0x00480000);
	WriteU32(writer, 0);
	WriteU16(writer, 1);
	WriteZeros(writer, 32);
	WriteU16(writer, 0x0018);
	WriteU16(writer, 0xFFFF);
	WriteAvcC(writer, stream);
	EndBox(writer, avc1);
	EndBox(writer, stsd);
}

static void WriteStbl(
	Mp4Writer* writer,
	const Fmp4Stream* stream,
	uint16_t width,
	uint16_t height)
{
	const size_t stbl = BeginBox(writer, "stbl");
	WriteStsd(writer, stream, width, height);

	size_t box = BeginBox(writer, "stts");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	EndBox(writer, box);

	box = BeginBox(writer, "stsc");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	EndBox(writer, box);

	box = BeginBox(writer, "stsz");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	EndBox(writer, box);

	box = BeginBox(writer, "stco");
	WriteU32(writer, 0);
	WriteU32(writer, 0);
	EndBox(writer, box);
	EndBox(writer, stbl);
}

static void WriteMinf(
	Mp4Writer* writer,
	const Fmp4Stream* stream,
	uint16_t width,
	uint16_t height)
{
	const size_t minf = BeginBox(writer, "minf");

	size_t box = BeginBox(writer, "vmhd");
	WriteU8(writer, 0);
	WriteU24(writer, 1);
	WriteU16(writer, 0);
	WriteZeros(writer, 6);
	EndBox(writer, box);

	const size_t dinf = BeginBox(writer, "dinf");
	const size_t dref = BeginBox(writer, "dref");
	WriteU32(writer, 0);
	WriteU32(writer, 1);
	box = BeginBox(writer, "url ");
	WriteU8(writer, 0);
	WriteU24(writer, 1);
	EndBox(writer, box);
	EndBox(writer, dref);
	EndBox(writer, dinf);

	WriteStbl(writer, stream, width, height);
	EndBox(writer, minf);
}

static void WriteTrak(
	Mp4Writer* writer,
	const Fmp4Stream* stream,
	uint16_t width,
	uint16_t height)
{
	const size_t trak = BeginBox(writer, "trak");
	WriteTkhd(writer, width, height);

	const size_t mdia = BeginBox(writer, "mdia");
	WriteMdhd(writer);
	WriteHdlr(writer);
	WriteMinf(writer, stream, width, height);
	EndBox(writer, mdia);
	EndBox(writer, trak);
}

static void WriteMvex(Mp4Writer* writer)
{
	const size_t mvex = BeginBox(writer, "mvex");
	const size_t trex = BeginBox(writer, "trex");
	WriteU32(writer, 0);
	WriteU32(writer, 1);
	WriteU32(writer, 1);
	WriteU32(writer, FMP4_DEFAULT_SAMPLE_DURATION);
	WriteU32(writer, 0);
	WriteU32(writer, 0x01010000);
	EndBox(writer, trex);
	EndBox(writer, mvex);
}

size_t Fmp4BuildInitSegment(
	const Fmp4Stream* stream,
	uint8_t* out,
	size_t capacity,
	uint16_t width,
	uint16_t height)
{
	if (!Fmp4HasConfiguration(stream) || !out || capacity == 0 ||
		width == 0 || height == 0)
		return 0;

	Mp4Writer writer = {
		.data = out,
		.capacity = capacity
	};

	size_t box = BeginBox(&writer, "ftyp");
	WriteBytes(&writer, "iso6", 4);
	WriteU32(&writer, 1);
	WriteBytes(&writer, "iso6", 4);
	WriteBytes(&writer, "mp41", 4);
	WriteBytes(&writer, "avc1", 4);
	EndBox(&writer, box);

	box = BeginBox(&writer, "moov");
	WriteMvhd(&writer);
	WriteTrak(&writer, stream, width, height);
	WriteMvex(&writer);
	EndBox(&writer, box);

	return writer.failed ? 0 : writer.position;
}

static bool ShouldIncludeNal(uint8_t type)
{
	return type != 7 && type != 8;
}

static size_t AvcSampleSize(
	const uint8_t* annexB,
	size_t annexBSize,
	bool* hasVcl)
{
	size_t total = 0;
	size_t cursor = 0;
	AnnexBNal nal;
	*hasVcl = false;

	while (NextNal(annexB, annexBSize, &cursor, &nal)) {
		if (nal.type == 1 || nal.type == 5)
			*hasVcl = true;
		if (!ShouldIncludeNal(nal.type))
			continue;
		if (nal.size > UINT32_MAX || total > SIZE_MAX - 4 - nal.size)
			return 0;
		total += 4 + nal.size;
	}
	return total;
}

static void WriteAvcSample(
	Mp4Writer* writer,
	const uint8_t* annexB,
	size_t annexBSize)
{
	size_t cursor = 0;
	AnnexBNal nal;
	while (NextNal(annexB, annexBSize, &cursor, &nal)) {
		if (!ShouldIncludeNal(nal.type))
			continue;
		WriteU32(writer, (uint32_t)nal.size);
		WriteBytes(writer, nal.data, nal.size);
	}
}

size_t Fmp4BuildFragment(
	Fmp4Stream* stream,
	const uint8_t* annexB,
	size_t annexBSize,
	uint64_t timestampUs,
	bool randomAccess,
	uint8_t* out,
	size_t capacity)
{
	if (!Fmp4HasConfiguration(stream) || !annexB || annexBSize == 0 ||
		!out || capacity == 0)
		return 0;

	bool hasVcl = false;
	const size_t sampleSize = AvcSampleSize(annexB, annexBSize, &hasVcl);
	if (!hasVcl || sampleSize == 0 || sampleSize > UINT32_MAX ||
		capacity < 108 || sampleSize > capacity - 108)
		return 0;

	if (!stream->mediaStarted) {
		stream->mediaStartTimestampUs = timestampUs;
		stream->mediaStarted = true;
	}
	const uint64_t relativeTimestamp =
		timestampUs >= stream->mediaStartTimestampUs
			? timestampUs - stream->mediaStartTimestampUs
			: 0;
	// GRC timestamps contain small sub-frame jitter. Quantize them to the
	// 30 fps media clock so adjacent samples never overlap in MSE, while
	// retaining larger gaps when captured frames are dropped.
	const uint64_t frameNumber =
		(relativeTimestamp * 30 + 500000) / 1000000;
	uint64_t decodeTime =
		frameNumber * FMP4_DEFAULT_SAMPLE_DURATION;
	if (decodeTime < stream->nextDecodeTime)
		decodeTime = stream->nextDecodeTime;
	stream->nextDecodeTime = decodeTime + FMP4_DEFAULT_SAMPLE_DURATION;

	Mp4Writer writer = {
		.data = out,
		.capacity = capacity
	};

	const size_t moof = BeginBox(&writer, "moof");
	size_t box = BeginBox(&writer, "mfhd");
	WriteU32(&writer, 0);
	WriteU32(&writer, stream->nextSequence++);
	EndBox(&writer, box);

	const size_t traf = BeginBox(&writer, "traf");
	box = BeginBox(&writer, "tfhd");
	WriteU8(&writer, 0);
	WriteU24(&writer, 0x020000);
	WriteU32(&writer, 1);
	EndBox(&writer, box);

	box = BeginBox(&writer, "tfdt");
	WriteU8(&writer, 1);
	WriteU24(&writer, 0);
	WriteU64(&writer, decodeTime);
	EndBox(&writer, box);

	box = BeginBox(&writer, "trun");
	WriteU8(&writer, 0);
	WriteU24(&writer, 0x000701);
	WriteU32(&writer, 1);
	const size_t dataOffset = writer.position;
	WriteU32(&writer, 0);
	WriteU32(&writer, FMP4_DEFAULT_SAMPLE_DURATION);
	WriteU32(&writer, (uint32_t)sampleSize);
	WriteU32(&writer, randomAccess ? 0x02000000 : 0x01010000);
	EndBox(&writer, box);
	EndBox(&writer, traf);
	EndBox(&writer, moof);

	PatchU32(
		&writer,
		dataOffset,
		(uint32_t)(writer.position - moof + 8));
	box = BeginBox(&writer, "mdat");
	WriteAvcSample(&writer, annexB, annexBSize);
	EndBox(&writer, box);

	return writer.failed ? 0 : writer.position;
}
