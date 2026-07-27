/*
 * SwitchCast timeout-aware USB transport.
 *
 * Derived from SysDVR's UsbComms transport by exelix11 and contributors.
 * GPL-2.0-only; see SYSDVR-ATTRIBUTION.md.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <switch.h>

Result UsbTransportInitialize(void);
void UsbTransportExit(void);

bool UsbTransportRead(void* data, size_t size);
bool UsbTransportWrite(const void* data, size_t size);
