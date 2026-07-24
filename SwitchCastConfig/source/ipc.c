#include <stdio.h>
#include <string.h>

#include "ipc.h"

#ifdef __SWITCH__
static bool HasPmShell;
static bool HasPmDmnt;
static Service SwitchCastService;

Result SwitchCastProcManagerInit(void)
{
	Result rc = pmshellInitialize();
	if (R_FAILED(rc))
		return rc;
	HasPmShell = true;

	rc = pmdmntInitialize();
	if (R_FAILED(rc)) {
		pmshellExit();
		HasPmShell = false;
		return rc;
	}
	HasPmDmnt = true;
	return 0;
}

bool SwitchCastProcManagerIsInitialized(void)
{
	return HasPmShell && HasPmDmnt;
}

void SwitchCastProcManagerExit(void)
{
	if (HasPmShell)
		pmshellExit();
	if (HasPmDmnt)
		pmdmntExit();
	HasPmShell = false;
	HasPmDmnt = false;
}

bool SwitchCastProcIsRunning(void)
{
	u64 processId;
	return R_SUCCEEDED(pmdmntGetProcessId(
		&processId,
		SWITCHCAST_CONTENT_ID));
}

Result SwitchCastProcTerminate(void)
{
	u64 processId;
	Result rc = pmdmntGetProcessId(
		&processId,
		SWITCHCAST_CONTENT_ID);
	if (R_SUCCEEDED(rc))
		rc = pmshellTerminateProcess(processId);
	return rc;
}

Result SwitchCastProcLaunch(void)
{
	NcmProgramLocation location = {0};
	location.program_id = SWITCHCAST_CONTENT_ID;
	location.storageID = NcmStorageId_None;
	u64 processId;
	return pmshellLaunchProgram(
		0,
		&location,
		&processId);
}

// Atmosphere extension:
// docs/components/modules/sm.md, command 65100.
static Result AtmosphereHasService(
	SmServiceName name,
	bool* available)
{
	if (
		hosversionIsAtmosphere() ||
		hosversionAtLeast(12, 0, 0)) {
		return tipcDispatchInOut(
			smGetServiceSessionTipc(),
			65100,
			name,
			*available);
	}
	return serviceDispatchInOut(
		smGetServiceSession(),
		65100,
		name,
		*available);
}

static bool LegacyServiceCheck(SmServiceName name)
{
	Handle handle;
	const Result rc = smRegisterService(
		&handle,
		name,
		false,
		1);
	if (R_FAILED(rc))
		return true;
	smUnregisterService(name);
	svcCloseHandle(handle);
	return false;
}

bool SwitchCastIsRunning(void)
{
	SmServiceName name = {0};
	memcpy(name.name, "swcast", sizeof("swcast"));
	bool available = false;
	const Result rc = AtmosphereHasService(
		name,
		&available);
	return R_SUCCEEDED(rc)
		? available
		: LegacyServiceCheck(name);
}

Result SwitchCastConnect(void)
{
	if (!SwitchCastIsRunning())
		return ERR_MAIN_NOTRUNNING;
	return smGetService(
		&SwitchCastService,
		"swcast");
}

void SwitchCastClose(void)
{
	serviceClose(&SwitchCastService);
}

void SwitchCastDebugCrash(void)
{
	serviceDispatch(
		&SwitchCastService,
		CMD_DEBUG_CRASH);
}

Result SwitchCastGetVersion(u32* version)
{
	u32 value;
	const Result rc = serviceDispatchOut(
		&SwitchCastService,
		CMD_GET_VER,
		value);
	if (R_SUCCEEDED(rc))
		*version = value;
	return rc;
}

Result SwitchCastGetEnabled(u32* enabled)
{
	u32 value;
	const Result rc = serviceDispatchOut(
		&SwitchCastService,
		CMD_GET_ENABLED,
		value);
	if (R_SUCCEEDED(rc))
		*enabled = value;
	return rc;
}

Result SwitchCastGetStatus(u32* status)
{
	u32 value;
	const Result rc = serviceDispatchOut(
		&SwitchCastService,
		CMD_GET_CAST_STATUS,
		value);
	if (R_SUCCEEDED(rc))
		*status = value;
	return rc;
}

Result SwitchCastGetTargetDelay(u32* delay)
{
	u32 value;
	const Result rc = serviceDispatchOut(
		&SwitchCastService,
		CMD_GET_CAST_TARGET_DELAY,
		value);
	if (R_SUCCEEDED(rc))
		*delay = value;
	return rc;
}

Result SwitchCastGetReceiverDelay(u32* delay)
{
	u32 value;
	const Result rc = serviceDispatchOut(
		&SwitchCastService,
		CMD_GET_CAST_RECEIVER_DELAY,
		value);
	if (R_SUCCEEDED(rc))
		*delay = value;
	return rc;
}

Result SwitchCastGetBlankScreen(u32* enabled)
{
	u32 value;
	const Result rc = serviceDispatchOut(
		&SwitchCastService,
		CMD_GET_BLANK_SCREEN,
		value);
	if (R_SUCCEEDED(rc))
		*enabled = value;
	return rc;
}

Result SwitchCastSetEnabled(bool enabled)
{
	return serviceDispatch(
		&SwitchCastService,
		enabled ? CMD_ENABLE : CMD_DISABLE);
}

Result SwitchCastSetBlankScreen(bool enabled)
{
	const u32 value = enabled ? 1U : 0U;
	return serviceDispatchIn(
		&SwitchCastService,
		CMD_SET_BLANK_SCREEN,
		value);
}

#else

Result SwitchCastProcManagerInit(void)
{
	return 0;
}

void SwitchCastProcManagerExit(void)
{
}

bool SwitchCastProcManagerIsInitialized(void)
{
	return true;
}

bool SwitchCastProcIsRunning(void)
{
	return true;
}

Result SwitchCastProcTerminate(void)
{
	return 0;
}

Result SwitchCastProcLaunch(void)
{
	return 0;
}

bool SwitchCastIsRunning(void)
{
	return true;
}

Result SwitchCastConnect(void)
{
	return 0;
}

void SwitchCastClose(void)
{
}

void SwitchCastDebugCrash(void)
{
}

Result SwitchCastGetVersion(u32* version)
{
	*version = SWITCHCAST_IPC_VERSION;
	return 0;
}

Result SwitchCastGetEnabled(u32* enabled)
{
	*enabled = 1;
	return 0;
}

Result SwitchCastGetStatus(u32* status)
{
	*status = CAST_STATUS_STREAMING;
	return 0;
}

Result SwitchCastGetTargetDelay(u32* delay)
{
	*delay = 90;
	return 0;
}

Result SwitchCastGetReceiverDelay(u32* delay)
{
	*delay = 90;
	return 0;
}

Result SwitchCastGetBlankScreen(u32* enabled)
{
	*enabled = 0;
	return 0;
}

Result SwitchCastSetEnabled(bool enabled)
{
	(void)enabled;
	return 0;
}

Result SwitchCastSetBlankScreen(bool enabled)
{
	(void)enabled;
	return 0;
}

#endif
