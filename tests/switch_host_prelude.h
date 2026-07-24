#pragma once

// Minimal newlib lock definitions used only for host-side syntax checking of
// libnx headers. Real Switch builds get these from devkitA64's newlib.
typedef unsigned int _LOCK_T;

typedef struct {
	volatile int lock;
	unsigned int counter;
} _LOCK_RECURSIVE_T;
