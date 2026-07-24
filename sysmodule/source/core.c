#include "core.h"

#include <string.h>

#include "capture.h"
#include "modes/modes.h"
#include "net/sockets.h"

static const char BuildIdentity[] =
	"SWITCHCAST_BUILD/" SWITCHCAST_VERSION_STRING "\n"
	"BASED_ON_SYSDVR_BY_EXELIX11_AND_CONTRIBUTORS/GPL-2.0";

StaticBuffers Buffers;
atomic_bool IsThreadRunning = false;

static Thread VideoThread;
static Mutex StateMutex;
static bool StateMutexReady;

Result CoreInit(void)
{
	if (!StateMutexReady) {
		mutexInit(&StateMutex);
		StateMutexReady = true;
	}

	Result rc = CaptureInitialize();
	if (R_FAILED(rc))
		return rc;

	SocketInit();

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

void CoreStart(void)
{
	mutexLock(&StateMutex);
	if (atomic_load(&IsThreadRunning)) {
		mutexUnlock(&StateMutex);
		return;
	}

	memset(&Buffers, 0, sizeof(Buffers));
	atomic_store(&IsThreadRunning, true);
	if (CAST_MODE.InitFn)
		CAST_MODE.InitFn();
	if (CAST_MODE.VThread) {
		static u8 alignas(0x1000)
			VideoStack[0x2000 + LOGGING_STACK_BOOST];
		memset(VideoStack, 0, sizeof(VideoStack));
		LaunchThread(
			&VideoThread,
			CAST_MODE.VThread,
			CAST_MODE.Vargs,
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
	if (CAST_MODE.ExitFn)
		CAST_MODE.ExitFn();
	if (CAST_MODE.VThread)
		JoinThread(&VideoThread);
	mutexUnlock(&StateMutex);
}
