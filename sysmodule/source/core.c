#include "core.h"

#include <string.h>

#include "capture.h"
#include "modes/modes.h"

static const char BuildIdentity[] =
	"SWITCHCAST_BUILD/" SWITCHCAST_VERSION_STRING "\n"
	"BASED_ON_SYSDVR_BY_EXELIX11_AND_CONTRIBUTORS/GPL-2.0";

StaticBuffers Buffers;
atomic_bool IsThreadRunning = false;

static Thread VideoThread;
static Mutex StateMutex;
static bool StateMutexReady;
static atomic_uint SelectedTransport = TYPE_MODE_CAST;
static const StreamMode* ActiveMode;

Result CoreInit(void)
{
	if (!StateMutexReady) {
		mutexInit(&StateMutex);
		StateMutexReady = true;
	}

	Result rc = CaptureInitialize();
	if (R_FAILED(rc))
		return rc;

	// Keep the build identity and primary attribution easy to scan in both
	// the binary and static memory.
	memcpy(
		Buffers.CastMode.ControlRx,
		BuildIdentity,
		sizeof(BuildIdentity));
	return 0;
}

void LaunchThread(
	Thread* thread,
	ThreadFunc function,
	void* argument,
	void* stackLocation,
	u32 stackSize,
	u32 priority)
{
	Result rc = threadCreate(
		thread,
		function,
		argument,
		stackLocation,
		stackSize,
		priority,
		3);
	if (R_FAILED(rc))
		fatalThrow(rc);
	rc = threadStart(thread);
	if (R_FAILED(rc))
		fatalThrow(rc);
}

void JoinThread(Thread* thread)
{
	if (thread->handle == INVALID_HANDLE)
		return;
	Result rc = threadWaitForExit(thread);
	if (R_FAILED(rc))
		fatalThrow(rc);
	rc = threadClose(thread);
	if (R_FAILED(rc))
		fatalThrow(rc);
	thread->handle = INVALID_HANDLE;
}

bool CoreIsEnabled(void)
{
	return atomic_load(&IsThreadRunning);
}

u32 CoreGetTransport(void)
{
	return atomic_load(&SelectedTransport);
}

Result CoreSetTransport(u32 transport)
{
	if (
		transport != TYPE_MODE_CAST &&
		transport != TYPE_MODE_USB_DOCK)
		return ERR_SWITCHCAST_TRANSPORT;

	mutexLock(&StateMutex);
	if (atomic_load(&IsThreadRunning)) {
		mutexUnlock(&StateMutex);
		return ERR_SWITCHCAST_TRANSPORT;
	}
	atomic_store(&SelectedTransport, transport);
	mutexUnlock(&StateMutex);
	return 0;
}

void CoreStart(void)
{
	mutexLock(&StateMutex);
	if (atomic_load(&IsThreadRunning)) {
		mutexUnlock(&StateMutex);
		return;
	}

	memset(&Buffers, 0, sizeof(Buffers));
	ActiveMode =
		atomic_load(&SelectedTransport) == TYPE_MODE_USB_DOCK
			? &USB_MODE
			: &CAST_MODE;
	atomic_store(&IsThreadRunning, true);
	if (ActiveMode->InitFn)
		ActiveMode->InitFn();
	if (ActiveMode->VThread) {
		static u8 alignas(0x1000)
			VideoStack[0x2000 + LOGGING_STACK_BOOST];
		memset(VideoStack, 0, sizeof(VideoStack));
		LaunchThread(
			&VideoThread,
			ActiveMode->VThread,
			ActiveMode->Vargs,
			VideoStack,
			sizeof(VideoStack),
			0x2C);
	}
	mutexUnlock(&StateMutex);
}

void CoreStop(void)
{
	mutexLock(&StateMutex);
	if (!atomic_load(&IsThreadRunning)) {
		mutexUnlock(&StateMutex);
		return;
	}

	atomic_store(&IsThreadRunning, false);
	if (ActiveMode && ActiveMode->ExitFn)
		ActiveMode->ExitFn();
	if (ActiveMode && ActiveMode->VThread)
		JoinThread(&VideoThread);
	ActiveMode = NULL;
	mutexUnlock(&StateMutex);
}
