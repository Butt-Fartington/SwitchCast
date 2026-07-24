#include <switch.h>
#include "util.h"
#include "modes/modes.h"

UtilScreenModeResult UtilSetConsoleScreenMode(bool on)
{
	Result rc = lblInitialize();
	bool changed = false;
	LOG("lblInitialize %x\n", rc);

	if (R_SUCCEEDED(rc)) {
		LblBacklightSwitchStatus lblstatus = LblBacklightSwitchStatus_Disabled;
		rc = lblGetBacklightSwitchStatus(&lblstatus);
		LOG("lblGetBacklightSwitchStatus %x\n", rc);
		if (R_SUCCEEDED(rc)) {
			if (on && lblstatus == LblBacklightSwitchStatus_Disabled) {
				rc = lblSwitchBacklightOn(0);
				changed = R_SUCCEEDED(rc);
			}
			else if (!on && lblstatus == LblBacklightSwitchStatus_Enabled) {
				rc = lblSwitchBacklightOff(0);
				changed = R_SUCCEEDED(rc);
			}
		}
		lblExit();
	}

	LOG(
		"UtilSetConsoleScreenMode(%d) %x changed=%d\n",
		on,
		rc,
		changed);
	if (R_FAILED(rc))
		return UtilScreenMode_Failed;
	return changed
		? UtilScreenMode_Changed
		: UtilScreenMode_Unchanged;
}
