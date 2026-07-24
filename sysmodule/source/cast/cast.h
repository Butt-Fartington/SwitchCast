#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

extern atomic_bool Cast_ClientStreaming;

uint32_t Cast_GetStatus(void);
uint32_t Cast_GetTargetDelayMs(void);
uint32_t Cast_GetReceiverDelayMs(void);
uint32_t Cast_GetCaptureGeneration(void);
void Cast_NotifyVideoStopped(void);
void Cast_ServerThread(void*);
void Cast_StopServer(void);

// Sends one Annex-B H.264 access unit to the connected Cast receiver.
bool Cast_WriteH264(
	const uint8_t* annexB,
	size_t annexBSize,
	uint64_t timestampUs,
	bool randomAccess);
