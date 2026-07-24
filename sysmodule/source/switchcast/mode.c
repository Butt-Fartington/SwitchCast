#include "../modes/modes.h"

#include "../capture.h"
#include "../cast/cast.h"

static Thread CastThread;

static bool ContainsIdrNal(const u8* data, size_t size)
{
	for (size_t i = 0; i + 4 < size; ++i) {
		size_t nalOffset = 0;
		if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
			nalOffset = i + 3;
		else if (
			data[i] == 0 &&
			data[i + 1] == 0 &&
			data[i + 2] == 0 &&
			data[i + 3] == 1)
			nalOffset = i + 4;
		if (nalOffset && (data[nalOffset] & 0x1F) == 5)
			return true;
	}
	return false;
}

static void StreamVideo(void* unused)
{
	(void)unused;
	if (!IsThreadRunning)
		fatalThrow(ERR_SWITCHCAST_VIDEO);

	while (IsThreadRunning) {
		while (!Cast_ClientStreaming && IsThreadRunning)
			svcSleepThread(100E+6);
		if (!IsThreadRunning)
			break;

		CaptureVideoConnected();
		const u32 captureGeneration = Cast_GetCaptureGeneration();
		u64 firstTimestamp = 0;

		while (Cast_ClientStreaming && IsThreadRunning) {
			if (!CaptureReadVideo()) {
				Cast_NotifyVideoStopped();
				break;
			}
			if (!IsThreadRunning ||
				!Cast_ClientStreaming ||
				Cast_GetCaptureGeneration() != captureGeneration)
				break;

			if (firstTimestamp == 0)
				firstTimestamp = VPkt.Header.Timestamp;
			const bool isIdr =
				ContainsIdrNal(VPkt.Data, VPkt.Header.DataSize);
			if (!Cast_WriteH264(
				VPkt.Data,
				VPkt.Header.DataSize,
				VPkt.Header.Timestamp - firstTimestamp,
				isIdr))
				break;
		}
	}
}

static void InitializeMode(void)
{
	CaptureConfigResetDefault();
	CaptureSetNalHashing(false, false);
	CaptureSetPPSSPSInject(true);
	CaptureSetPPSSPSInterval(1);
	LaunchThread(
		&CastThread,
		Cast_ServerThread,
		NULL,
		Buffers.CastMode.ControlThreadStackArea,
		sizeof(Buffers.CastMode.ControlThreadStackArea),
		0x2D);
}

static void ExitMode(void)
{
	Cast_StopServer();
	JoinThread(&CastThread);
}

const StreamMode CAST_MODE = {
	InitializeMode,
	ExitMode,
	StreamVideo,
	NULL
};
