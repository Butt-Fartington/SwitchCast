#pragma once
#include <stdbool.h>

typedef enum {
	UtilScreenMode_Failed = -1,
	UtilScreenMode_Unchanged = 0,
	UtilScreenMode_Changed = 1
} UtilScreenModeResult;

// Changes the console backlight state and reports whether SwitchCast actually
// changed it. Callers can then avoid turning on a display that was already off.
UtilScreenModeResult UtilSetConsoleScreenMode(bool on);
