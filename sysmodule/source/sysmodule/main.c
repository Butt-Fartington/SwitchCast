#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "../core.h"
#include "../ipc/ipc.h"
#include "../net/sockets.h"

// SwitchCast deliberately runs without a heap. All transport and capture
// buffers are statically reserved so allocation pressure cannot destabilize
// other sysmodules while a game is running.
void* __libnx_aligned_alloc(size_t alignment, size_t size)
{
	(void)alignment;
	(void)size;
	fatalThrow(ERR_MAIN_ALLOC_DISABLED);
	return NULL;
}

void __libnx_free(void* pointer)
{
	(void)pointer;
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

	if (FileExists("/config/switchcast/enabled"))
		CoreStart();

	IpcThread();
	return 0;
}
