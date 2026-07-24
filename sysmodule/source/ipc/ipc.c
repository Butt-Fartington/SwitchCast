#include <string.h>

#include "ipc.h"
#include "../cast/cast.h"
#include "../core.h"
#include "../modes/defines.h"

static Handle Handles[2];
static SmServiceName ServerName;

static Handle* const ServerHandle = &Handles[0];
static Handle* const ClientHandle = &Handles[1];

static void StartServer(void)
{
	*ServerHandle = INVALID_HANDLE;
	*ClientHandle = INVALID_HANDLE;
	memcpy(ServerName.name, "swcast", sizeof("swcast"));
	R_THROW(smRegisterService(
		ServerHandle,
		ServerName,
		false,
		1));
}

static void DisconnectClient(void)
{
	if (*ClientHandle == INVALID_HANDLE)
		return;
	svcCloseHandle(*ClientHandle);
	*ClientHandle = INVALID_HANDLE;
}

static void StopServer(void)
{
	DisconnectClient();
	svcCloseHandle(*ServerHandle);
	smUnregisterService(ServerName);
	*ServerHandle = INVALID_HANDLE;
}

typedef struct {
	u32 type;
	u64 commandId;
	void* data;
	u32 dataSize;
} Request;

static Request ParseRequestFromTls(void)
{
	Request request = {0};
	void* base = armGetTls();
	HipcParsedRequest hipc = hipcParseRequest(base);
	request.type = hipc.meta.type;

	if (hipc.meta.type != CmifCommandType_Request)
		return request;

	CmifInHeader* header = (CmifInHeader*)cmifGetAlignedDataStart(
		hipc.data.data_words,
		base);
	const size_t dataSize = hipc.meta.num_data_words * 4;
	if (!header)
		fatalThrow(ERR_IPC_INVHEADER);
	if (dataSize < sizeof(CmifInHeader))
		fatalThrow(ERR_IPC_INVSIZE);
	if (header->magic != CMIF_IN_HEADER_MAGIC)
		fatalThrow(ERR_IPC_INVMAGIC);

	request.commandId = header->command_id;
	request.dataSize = dataSize - sizeof(CmifInHeader);
	request.data = request.dataSize
		? ((u8*)header) + sizeof(CmifInHeader)
		: NULL;
	return request;
}

static void WriteResponseToTls(Result rc)
{
	HipcMetadata metadata = {0};
	metadata.type = CmifCommandType_Request;
	metadata.num_data_words =
		(sizeof(CmifOutHeader) + 0x10) / 4;

	void* base = armGetTls();
	HipcRequest hipc = hipcMakeRequest(base, metadata);
	CmifOutHeader* header =
		(CmifOutHeader*)cmifGetAlignedDataStart(
			hipc.data_words,
			base);
	header->magic = CMIF_OUT_HEADER_MAGIC;
	header->result = rc;
	header->token = 0;
}

static void WritePayloadResponseToTls(
	Result rc,
	const void* payload,
	u32 length)
{
	HipcMetadata metadata = {0};
	metadata.type = CmifCommandType_Request;
	metadata.num_data_words =
		(sizeof(CmifOutHeader) + 0x10 + length) / 4;

	void* base = armGetTls();
	HipcRequest hipc = hipcMakeRequest(base, metadata);
	CmifOutHeader* header =
		(CmifOutHeader*)cmifGetAlignedDataStart(
			hipc.data_words,
			base);
	header->magic = CMIF_OUT_HEADER_MAGIC;
	header->result = rc;
	header->token = 0;
	memcpy(
		((u8*)header) + sizeof(CmifOutHeader),
		payload,
		length);
}

typedef enum {
	Pending_None,
	Pending_Enable,
	Pending_Disable
} PendingAction;

static PendingAction Pending = Pending_None;

static void ApplyPendingAction(void)
{
	const PendingAction action = Pending;
	Pending = Pending_None;
	if (action == Pending_Enable)
		CoreStart();
	else if (action == Pending_Disable)
		CoreStop();
}

static bool HandleCommand(const Request* request)
{
	switch (request->commandId) {
	case CMD_GET_VER: {
		const u32 version = SWITCHCAST_IPC_VERSION;
		WritePayloadResponseToTls(
			0,
			&version,
			sizeof(version));
		return false;
	}
	case CMD_GET_ENABLED: {
		const u32 enabled = CoreIsEnabled() ? 1U : 0U;
		WritePayloadResponseToTls(
			0,
			&enabled,
			sizeof(enabled));
		return false;
	}
	case CMD_GET_CAST_STATUS: {
		const u32 status = Cast_GetStatus();
		WritePayloadResponseToTls(
			0,
			&status,
			sizeof(status));
		return false;
	}
	case CMD_GET_CAST_TARGET_DELAY: {
		const u32 delay = Cast_GetTargetDelayMs();
		WritePayloadResponseToTls(
			0,
			&delay,
			sizeof(delay));
		return false;
	}
	case CMD_GET_CAST_RECEIVER_DELAY: {
		const u32 delay = Cast_GetReceiverDelayMs();
		WritePayloadResponseToTls(
			0,
			&delay,
			sizeof(delay));
		return false;
	}
	case CMD_ENABLE:
		Pending = Pending_Enable;
		WriteResponseToTls(0);
		return false;
	case CMD_DISABLE:
		Pending = Pending_Disable;
		WriteResponseToTls(0);
		return false;
	case CMD_DEBUG_CRASH:
		*(volatile u32*)0 = 0xDEAD;
		return false;
	default:
		WriteResponseToTls(ERR_IPC_UNKCMD);
		return true;
	}
}

static bool IsClientConnected(void)
{
	return *ClientHandle != INVALID_HANDLE;
}

typedef enum {
	Ipc_Ok,
	Ipc_Again,
	Ipc_Terminate
} IpcStatus;

static IpcStatus WaitAndProcessRequest(void)
{
	s32 index = -1;
	Result rc = svcWaitSynchronization(
		&index,
		Handles,
		IsClientConnected() ? 2 : 1,
		UINT64_MAX);
	if (R_FAILED(rc)) {
		LOG("svcWaitSynchronization: %x\n", rc);
		if (
			rc == KERNELRESULT(ThreadTerminating) ||
			rc == KERNELRESULT(Cancelled))
			return Ipc_Terminate;
		if (rc == KERNELRESULT(InvalidHandle)) {
			if (IsClientConnected()) {
				DisconnectClient();
				Pending = Pending_None;
				return Ipc_Again;
			}
			return Ipc_Terminate;
		}
		fatalThrow(rc);
	}

	if (index == 0) {
		Handle newClient;
		if (R_FAILED(svcAcceptSession(
			&newClient,
			*ServerHandle)))
			return Ipc_Again;
		if (IsClientConnected()) {
			svcCloseHandle(newClient);
			return Ipc_Again;
		}
		Pending = Pending_None;
		*ClientHandle = newClient;
		return Ipc_Ok;
	}

	if (index != 1 || !IsClientConnected())
		return Ipc_Again;

	s32 receiveIndex;
	rc = svcReplyAndReceive(
		&receiveIndex,
		ClientHandle,
		1,
		0,
		UINT64_MAX);
	if (R_FAILED(rc)) {
		DisconnectClient();
		Pending = Pending_None;
		return Ipc_Again;
	}

	bool shouldClose = false;
	const Request request = ParseRequestFromTls();
	switch (request.type) {
	case CmifCommandType_Request:
		shouldClose = HandleCommand(&request);
		break;
	case CmifCommandType_Close:
		WriteResponseToTls(0);
		shouldClose = true;
		break;
	default:
		WriteResponseToTls(ERR_HIPC_UNKREQ);
		break;
	}

	// Return success before starting or stopping capture. This avoids making
	// the UI wait for thread teardown and lets it show the new state instantly.
	rc = svcReplyAndReceive(
		&receiveIndex,
		NULL,
		0,
		*ClientHandle,
		0);
	if (
		R_FAILED(rc) &&
		rc != KERNELRESULT(TimedOut)) {
		DisconnectClient();
		Pending = Pending_None;
		return Ipc_Again;
	}

	ApplyPendingAction();
	if (shouldClose)
		DisconnectClient();
	return Ipc_Ok;
}

void IpcThread(void)
{
	StartServer();
	while (WaitAndProcessRequest() != Ipc_Terminate) {
	}
	StopServer();
}
