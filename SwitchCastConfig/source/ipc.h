#pragma once

#ifdef __SWITCH__
#include <switch.h>
#else
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t u32;
typedef uint32_t Result;
#define MAKERESULT(x,y) 0
#define R_FAILED(x) ((x) != 0)
#define R_SUCCEEDED(x) ((x) == 0)
#define R_DESCRIPTION(x) (x)
#define R_MODULE(x) (x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "../../sysmodule/source/modes/defines.h"

#define SWITCHCAST_EXEFS_PATH \
	"/atmosphere/contents/00FF000053434153/exefs.nsp"

Result SwitchCastProcManagerInit(void);
void SwitchCastProcManagerExit(void);
bool SwitchCastProcManagerIsInitialized(void);
bool SwitchCastProcIsRunning(void);
Result SwitchCastProcTerminate(void);
Result SwitchCastProcLaunch(void);

bool SwitchCastIsRunning(void);
Result SwitchCastConnect(void);
void SwitchCastClose(void);
void SwitchCastDebugCrash(void);

Result SwitchCastGetVersion(u32* version);
Result SwitchCastGetEnabled(u32* enabled);
Result SwitchCastGetStatus(u32* status);
Result SwitchCastGetTargetDelay(u32* delay);
Result SwitchCastGetReceiverDelay(u32* delay);
Result SwitchCastSetEnabled(bool enabled);

#ifdef __cplusplus
}
#endif
