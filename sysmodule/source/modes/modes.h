#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <switch.h>

#include "defines.h"
#include "../core.h"

extern atomic_bool IsThreadRunning;

typedef struct {
	void (*InitFn)(void);
	void (*ExitFn)(void);
	void (*VThread)(void*);
	void* Vargs;
} StreamMode;

typedef struct {
	struct {
		u8 alignas(0x1000)
			ControlThreadStackArea[0x5000 + LOGGING_STACK_BOOST];
		u8 ControlTx[4096];
		u8 ControlRx[8192];
		u8 MdnsResponse[1536];
		u8 UdpTx[1472];
		u8 UdpRx[2048];
	} CastMode;
} StaticBuffers;

extern StaticBuffers Buffers;
extern const StreamMode CAST_MODE;
