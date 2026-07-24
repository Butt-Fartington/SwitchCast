#pragma once

#include <stdatomic.h>
#include <switch.h>

#include "modes/defines.h"

#define LOGGING_ENABLED (UDP_LOGGING || FILE_LOGGING || CUSTOM_LOGGING)

#if (UDP_LOGGING + FILE_LOGGING + CUSTOM_LOGGING) > 1
	#pragma error "Only one logging backend may be enabled"
#endif

#define NEEDS_FS 1
#define NEEDS_SOCKETS 1

#if LOGGING_ENABLED
	#if FILE_LOGGING
		#include <stdio.h>
		#define LogFunctionImpl(...) \
			do { printf(__VA_ARGS__); fflush(stdout); } while (0)
	#else
		void LogFunctionImpl(const char* fmt, ...);
	#endif
	#define LOG(...) do { LogFunctionImpl(__VA_ARGS__); } while (0)
	#define LOGGING_STACK_BOOST 0x1000
#else
	#define LOG(...) do { } while (0)
	#define LOGGING_STACK_BOOST 0
#endif

#define R_THROW(x) \
	do { Result result_ = (x); if (R_FAILED(result_)) fatalThrow(result_); } while (0)

Result CoreInit(void);
void CoreStart(void);
void CoreStop(void);
bool CoreIsEnabled(void);

void LaunchThread(
	Thread* thread,
	ThreadFunc function,
	void* argument,
	void* stackLocation,
	u32 stackSize,
	u32 priority);
void JoinThread(Thread* thread);

bool CoreReadSdFile(
	const char* path,
	void* output,
	u64 capacity,
	u64* outputSize);
bool CoreWriteSdFile(const char* path, const void* data, u64 size);
