#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "../core.h"
#include "../cast/cast.h"
#include "../ipc/ipc.h"
#include "../modes/modes.h"
#include "../net/sockets.h"

/*
 * SwitchCast deliberately runs without a general heap. libnx's USB comms
 * helper requires exactly two page-aligned endpoint scratch pages, so those
 * allocations are served from the USB member of the transport buffer union.
 * Any other allocation remains a fatal programming error.
 */
static atomic_uint UsbEndpointAllocationMask;

void* __libnx_aligned_alloc(size_t alignment, size_t size)
{
	if (
		size == 0x1000 &&
		alignment == 0x1000) {
		for (unsigned int page = 0; page < 2; ++page) {
			const unsigned int bit = 1U << page;
			unsigned int mask =
				atomic_load(&UsbEndpointAllocationMask);
			while ((mask & bit) == 0) {
				if (atomic_compare_exchange_weak(
					&UsbEndpointAllocationMask,
					&mask,
					mask | bit)) {
					return Buffers.UsbMode.EndpointPages[page];
				}
			}
		}
	}
	fatalThrow(ERR_MAIN_ALLOC_DISABLED);
	return NULL;
}

void __libnx_free(void* pointer)
{
	if (!pointer)
		return;
	for (unsigned int page = 0; page < 2; ++page) {
		if (pointer == Buffers.UsbMode.EndpointPages[page]) {
			const unsigned int bit = 1U << page;
			const unsigned int previous =
				atomic_fetch_and(
					&UsbEndpointAllocationMask,
					~bit);
			if ((previous & bit) == 0)
				fatalThrow(ERR_MAIN_ALLOC_DISABLED);
			return;
		}
	}
	fatalThrow(ERR_MAIN_ALLOC_DISABLED);
}

void __libnx_initheap(void)
{
	extern char* fake_heap_start;
	extern char* fake_heap_end;
	fake_heap_start = NULL;
	fake_heap_end = NULL;
}

static Result FsInitResult;
static FsFileSystem SdCard;

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;
u32 __nx_fsdev_direntry_cache_size = 1;

void __attribute__((weak)) __appInit(void)
{
	// Let boot-critical services and other sysmodules settle first.
	svcSleepThread(20E+9);

	Result rc = smInitialize();
	if (R_FAILED(rc))
		fatalThrow(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

	rc = fsInitialize();
	if (R_FAILED(rc))
		fatalThrow(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

	rc = setsysInitialize();
	if (R_SUCCEEDED(rc)) {
		SetSysFirmwareVersion firmware;
		rc = setsysGetFirmwareVersion(&firmware);
		if (R_SUCCEEDED(rc)) {
			hosversionSet(MAKEHOSVERSION(
				firmware.major,
				firmware.minor,
				firmware.micro));
		}
		setsysExit();
	}
	if (R_FAILED(rc))
		fatalThrow(ERR_INIT_FAILED);

	rc = CoreInit();
	if (R_FAILED(rc))
		fatalThrow(rc);

	// Failure is nonfatal; SwitchCast can still be enabled from the UI.
	FsInitResult = fsOpenSdCardFileSystem(&SdCard);
}

void __attribute__((weak)) __appExit(void)
{
	CoreStop();
	SocketDeinit();
	if (R_SUCCEEDED(FsInitResult))
		fsFsClose(&SdCard);
	fsExit();
	smExit();
}

static bool FileExists(const char* path)
{
	if (R_FAILED(FsInitResult))
		return false;

	FsFile file;
	if (R_FAILED(fsFsOpenFile(
		&SdCard,
		path,
		FsOpenMode_Read,
		&file)))
		return false;
	fsFileClose(&file);
	return true;
}

bool CoreReadSdFile(
	const char* path,
	void* output,
	u64 capacity,
	u64* outputSize)
{
	if (outputSize)
		*outputSize = 0;
	if (
		R_FAILED(FsInitResult) ||
		!path ||
		!output ||
		capacity == 0)
		return false;

	FsFile file;
	if (R_FAILED(fsFsOpenFile(
		&SdCard,
		path,
		FsOpenMode_Read,
		&file)))
		return false;

	s64 fileSize = 0;
	Result rc = fsFileGetSize(&file, &fileSize);
	u64 bytesRead = 0;
	if (
		R_SUCCEEDED(rc) &&
		fileSize >= 0 &&
		(u64)fileSize <= capacity) {
		rc = fsFileRead(
			&file,
			0,
			output,
			(u64)fileSize,
			FsReadOption_None,
			&bytesRead);
	}
	fsFileClose(&file);

	if (
		R_FAILED(rc) ||
		fileSize < 0 ||
		bytesRead != (u64)fileSize)
		return false;
	if (outputSize)
		*outputSize = bytesRead;
	return true;
}

bool CoreWriteSdFile(
	const char* path,
	const void* data,
	u64 size)
{
	if (
		R_FAILED(FsInitResult) ||
		!path ||
		(!data && size != 0))
		return false;

	// Best effort: the file generally does not exist on the first write.
	fsFsDeleteFile(&SdCard, path);
	if (R_FAILED(fsFsCreateFile(
		&SdCard,
		path,
		(s64)size,
		0)))
		return false;

	FsFile file;
	if (R_FAILED(fsFsOpenFile(
		&SdCard,
		path,
		FsOpenMode_Write,
		&file)))
		return false;

	const Result rc = size == 0
		? 0
		: fsFileWrite(
			&file,
			0,
			data,
			size,
			FsWriteOption_Flush);
	fsFileClose(&file);
	return R_SUCCEEDED(rc);
}

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	char transport[8] = {0};
	u64 transportSize = 0;
	if (
		CoreReadSdFile(
			"/config/switchcast/transport",
			transport,
			sizeof(transport) - 1,
			&transportSize) &&
		transportSize >= 3 &&
		memcmp(transport, "usb", 3) == 0) {
		(void)CoreSetTransport(TYPE_MODE_USB_DOCK);
	}

	Cast_SetBlankScreenEnabled(
		FileExists("/config/switchcast/blank_screen"));
	if (FileExists("/config/switchcast/enabled"))
		CoreStart();

	IpcThread();
	return 0;
}
