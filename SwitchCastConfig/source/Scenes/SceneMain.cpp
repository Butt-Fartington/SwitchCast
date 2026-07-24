#include "Scenes.hpp"
#include "Common.hpp"

#include "../Platform/fs.hpp"
#include "../ipc.h"
#include "../translaton.hpp"

#include <string>

namespace {
constexpr auto BootEnabledPath =
	SDMC "/config/switchcast/enabled";
constexpr auto ReceiverPath =
	SDMC "/config/switchcast/receiver_ip";
constexpr auto LatencyProfilePath =
	SDMC "/config/switchcast/latency_profile";

constexpr u32 UltraTargetDelayMs = 90;
constexpr u32 StableTargetDelayMs = 150;

Image::Img CastImage;
bool BootEnabled;
bool CurrentEnabled;
bool UltraLatency;
bool RestartAfterProfileChange;
u32 CurrentStatus = CAST_STATUS_OFF;
u32 TargetDelayMs = UltraTargetDelayMs;
u32 ReceiverDelayMs;
std::string ConfiguredReceiver;

std::string CastStatusText(u32 status)
{
	if (status & CAST_STATUS_MEDIA_ERROR_FLAG) {
		return Strings::Main.CastStatusMediaErrorCode +
			" " +
			std::to_string(
				status & CAST_STATUS_MEDIA_ERROR_CODE_MASK);
	}

	switch (status) {
	case CAST_STATUS_DISCOVERING:
		return Strings::Main.CastStatusDiscovering;
	case CAST_STATUS_CONNECTING:
		return Strings::Main.CastStatusConnecting;
	case CAST_STATUS_WAITING_HTTP:
		return Strings::Main.CastStatusWaitingHttp;
	case CAST_STATUS_HTTP_CONNECTED:
		return Strings::Main.CastStatusHttpConnected;
	case CAST_STATUS_STREAMING:
		return Strings::Main.CastStatusStreaming;
	case CAST_STATUS_LOAD_FAILED:
		return Strings::Main.CastStatusLoadFailed;
	case CAST_STATUS_HTTP_TIMEOUT:
		return Strings::Main.CastStatusHttpTimeout;
	case CAST_STATUS_NETWORK_ERROR:
		return Strings::Main.CastStatusNetworkError;
	case CAST_STATUS_HLS_ERROR:
		return Strings::Main.CastStatusHlsError;
	case CAST_STATUS_WAITING_GAME:
		return Strings::Main.CastStatusWaitingGame;
	case CAST_STATUS_GAME_MONITOR_ERROR:
		return Strings::Main.CastStatusGameMonitorError;
	case CAST_STATUS_MEMORY_ERROR:
		return Strings::Main.CastStatusMemoryError;
	case CAST_STATUS_HLS_STARTUP_TIMEOUT:
		return Strings::Main.CastStatusHlsStartupTimeout;
	case CAST_STATUS_RECEIVER_CLOSED:
		return Strings::Main.CastStatusReceiverClosed;
	case CAST_STATUS_CONTROL_ERROR:
		return Strings::Main.CastStatusControlError;
	case CAST_STATUS_HTTP_REQUEST_ERROR:
		return Strings::Main.CastStatusHttpRequestError;
	case CAST_STATUS_PREPARING_HLS:
		return Strings::Main.CastStatusPreparingHls;
	case CAST_STATUS_CONTROL_SEND_ERROR:
		return Strings::Main.CastStatusControlSendError;
	case CAST_STATUS_CONTROL_POLL_ERROR:
		return Strings::Main.CastStatusControlPollError;
	case CAST_STATUS_CONTROL_HEADER_ERROR:
		return Strings::Main.CastStatusControlHeaderError;
	case CAST_STATUS_CONTROL_FRAME_TOO_LARGE:
		return Strings::Main.CastStatusControlFrameTooLarge;
	case CAST_STATUS_CONTROL_BODY_ERROR:
		return Strings::Main.CastStatusControlBodyError;
	case CAST_STATUS_CONTROL_DECODE_ERROR:
		return Strings::Main.CastStatusControlDecodeError;
	case CAST_STATUS_CONTROL_ENCODE_ERROR:
		return Strings::Main.CastStatusControlEncodeError;
	case CAST_STATUS_CONTROL_PENDING_ERROR:
		return Strings::Main.CastStatusControlPendingError;
	case CAST_STATUS_CONTROL_SOCKET_POLL_ERROR:
		return Strings::Main.CastStatusControlSocketPollError;
	case CAST_STATUS_CONTROL_SOCKET_CLOSED:
		return Strings::Main.CastStatusControlSocketClosed;
	case CAST_STATUS_RECEIVER_STATUS_TIMEOUT:
		return Strings::Main.CastStatusReceiverStatusTimeout;
	case CAST_STATUS_APP_ID_MISSING:
		return Strings::Main.CastStatusAppIdMissing;
	case CAST_STATUS_RECEIVER_READY_TIMEOUT:
		return Strings::Main.CastStatusReceiverReadyTimeout;
	case CAST_STATUS_RECEIVER_STREAM_ERROR:
		return Strings::Main.CastStatusReceiverStreamError;
	case CAST_STATUS_OFF:
	default:
		return Strings::Main.CastStatusOff;
	}
}

std::string ReadTrimmedFile(const std::string& path)
{
	try {
		if (!fs::Exists(path))
			return {};
		auto data = fs::OpenFile(path);
		std::string value(data.begin(), data.end());
		while (!value.empty() && (
			value.back() == '\r' ||
			value.back() == '\n' ||
			value.back() == ' ' ||
			value.back() == '\t'))
			value.pop_back();
		return value;
	}
	catch (std::exception&) {
		return {};
	}
}

bool SetBootEnabled(bool enabled)
{
	try {
		fs::CreateDir(SDMC "/config/");
		fs::CreateDir(SDMC "/config/switchcast/");
		fs::Delete(BootEnabledPath);
		if (enabled)
			fs::WriteFile(BootEnabledPath, {'1'});
		return true;
	}
	catch (std::exception&) {
		return false;
	}
}

bool SaveLatencyProfile(bool ultra)
{
	try {
		const std::string value = ultra ? "ultra" : "stable";
		fs::CreateDir(SDMC "/config/");
		fs::CreateDir(SDMC "/config/switchcast/");
		fs::Delete(LatencyProfilePath);
		fs::WriteFile(
			LatencyProfilePath,
			{value.begin(), value.end()});
		UltraLatency = ultra;
		return true;
	}
	catch (std::exception&) {
		return false;
	}
}

bool SetEnabled(bool enabled)
{
	const Result rc = SwitchCastSetEnabled(enabled);
	if (R_FAILED(rc)) {
		app::FatalErrorWithErrorCode(
			Strings::Error.ModeChangeFailed,
			rc);
		return false;
	}
	CurrentEnabled = enabled;
	return true;
}

void RefreshRuntimeState(void)
{
	u32 enabled = 0;
	if (R_SUCCEEDED(SwitchCastGetEnabled(&enabled)))
		CurrentEnabled = enabled != 0;
	if (CurrentEnabled) {
		(void)SwitchCastGetStatus(&CurrentStatus);
		(void)SwitchCastGetReceiverDelay(&ReceiverDelayMs);
	}
	else {
		CurrentStatus = CAST_STATUS_OFF;
		ReceiverDelayMs = 0;
	}
	(void)SwitchCastGetTargetDelay(&TargetDelayMs);
}

void PumpProfileRestart(void)
{
	if (!RestartAfterProfileChange)
		return;

	u32 enabled = 1;
	if (R_FAILED(SwitchCastGetEnabled(&enabled)) || enabled != 0)
		return;

	RestartAfterProfileChange = false;
	SetEnabled(true);
}

void SelectLatencyProfile(bool ultra)
{
	if (UltraLatency == ultra)
		return;
	if (!SaveLatencyProfile(ultra)) {
		app::FatalError(
			"Couldn't save the latency profile.",
			Strings::Error.TroubleshootBootMode);
		return;
	}

	TargetDelayMs = ultra
		? UltraTargetDelayMs
		: StableTargetDelayMs;
	ReceiverDelayMs = 0;
	if (CurrentEnabled && SetEnabled(false))
		RestartAfterProfileChange = true;
}

void StatusBadge(
	const char* text,
	const ImVec4& color)
{
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::TextUnformatted(text);
	ImGui::PopStyleColor();
}

void DrawDashboard(void)
{
	ImGui::SetCursorPos({70, 174});
	ImGui::BeginChild(
		"SwitchCastStatus",
		{1140, 198},
		true,
		ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::SetCursorPos({20, 38});
	ImGui::Image(CastImage, CastImage.Size());

	ImGui::SetCursorPos({235, 18});
	UI::BigFont();
	ImGui::TextUnformatted("Gameplay Cast");
	UI::PopFont();

	ImGui::SetCursorPos({235, 82});
	ImGui::Text(
		"Status: %s",
		(CurrentEnabled
			? CastStatusText(CurrentStatus)
			: Strings::Main.CastStatusOff).c_str());

	const std::string receiver =
		ConfiguredReceiver.empty()
			? Strings::Main.CastPickerAutomatic
			: ConfiguredReceiver;
	ImGui::SetCursorPos({235, 118});
	ImGui::Text("Receiver: %s", receiver.c_str());

	ImGui::SetCursorPos({235, 154});
	if (ReceiverDelayMs) {
		ImGui::Text(
			"Video delay target: %u ms  |  Receiver report: %u ms",
			TargetDelayMs,
			ReceiverDelayMs);
	}
	else {
		ImGui::Text(
			"Video delay target: %u ms  |  Receiver report: waiting",
			TargetDelayMs);
	}

	ImGui::SetCursorPos({965, 30});
	StatusBadge(
		CurrentEnabled ? "[ON] ENABLED" : "[OFF] STOPPED",
		CurrentEnabled
			? ImVec4(0.15f, 1.0f, 0.55f, 1.0f)
			: ImVec4(0.65f, 0.65f, 0.70f, 1.0f));
	ImGui::SetCursorPos({965, 68});
	StatusBadge(
		BootEnabled ? "[ON] ON AT BOOT" : "[OFF] MANUAL START",
		BootEnabled
			? ImVec4(0.15f, 0.75f, 1.0f, 1.0f)
			: ImVec4(0.65f, 0.65f, 0.70f, 1.0f));

	ImGui::EndChild();
}

void DrawTerminatePopup(void)
{
	ImGui::SetNextWindowSize({600, 350});
	if (!ImGui::BeginPopupModal(
		"KillSwitchCast",
		NULL,
		ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_AlwaysUseWindowPadding))
		return;

	ImGui::Spacing();
	ImGui::SetCursorPosX(10);
	ImGui::TextWrapped(
		Strings::Main.WarnSysmoduleKill.c_str());
	ImGui::NewLine();
	CenterText(Strings::Main.ContinueQuestion);
	ImGui::SetCursorPosX(
		ImGui::GetWindowWidth() / 2 - 200);
	if (ImGui::Button(
		Strings::Main.Yes.c_str(),
		{400, 0})) {
		SwitchCastProcTerminate();
		app::RequestExit();
	}
	ImGui::SetCursorPosX(
		ImGui::GetWindowWidth() / 2 - 200);
	if (ImGui::Button(
		Strings::Main.No.c_str(),
		{400, 0}))
		ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}
}

void scenes::InitModeSelect(void)
{
	CastImage = Image::Img(ASSET("SwitchCast.png"));
	RefreshModeSelect();
}

void scenes::RefreshModeSelect(void)
{
	u32 enabled = 0;
	const Result rc = SwitchCastGetEnabled(&enabled);
	if (R_FAILED(rc)) {
		app::FatalErrorWithErrorCode(
			Strings::Error.FailedToDetectMode,
			rc);
		return;
	}

	CurrentEnabled = enabled != 0;
	BootEnabled = fs::Exists(BootEnabledPath);
	ConfiguredReceiver = ReadTrimmedFile(ReceiverPath);
	UltraLatency =
		ReadTrimmedFile(LatencyProfilePath) != "stable";
	TargetDelayMs = UltraLatency
		? UltraTargetDelayMs
		: StableTargetDelayMs;
	ReceiverDelayMs = 0;
	RestartAfterProfileChange = false;
	RefreshRuntimeState();
}

void scenes::SetModeSelectState(uint32_t mode)
{
	CurrentEnabled = mode == TYPE_MODE_CAST;
}

void scenes::ModeSelect(void)
{
	RefreshRuntimeState();
	PumpProfileRestart();

	SetupMainWindow("SwitchCast");

	ImGui::SetCursorPosY(7);
	CenterImage(SwitchCastLogo, .42f);
	ImGui::SetCursorPosY(76);
	UI::BigFont();
	CenterText("SwitchCast");
	UI::PopFont();
	ImGui::SetCursorPosY(137);
	CenterText("Direct, video-only gameplay casting on your local network");

	DrawDashboard();

	ImGui::SetCursorPos({70, 386});
	if (ImGui::Button(
		CurrentEnabled
			? "Change Cast receiver"
			: "Select receiver & start",
		{555, 52}))
		OpenCastPicker();
	ImGui::SameLine();
	if (ImGui::Button(
		CurrentEnabled
			? "Stop SwitchCast"
			: "Start with saved receiver",
		{555, 52})) {
		if (!SetEnabled(!CurrentEnabled)) {
			ImGui::End();
			return;
		}
	}

	ImGui::SetCursorPos({70, 458});
	ImGui::TextUnformatted("Video latency profile");
	ImGui::SetCursorPos({360, 450});
	if (ImGui::Button(
		UltraLatency
			? "[Selected] Ultra-low - 90 ms##ultra"
			: "Ultra-low - 90 ms##ultra",
		{350, 48}))
		SelectLatencyProfile(true);
	ImGui::SameLine();
	if (ImGui::Button(
		!UltraLatency
			? "[Selected] Stable - 150 ms##stable"
			: "Stable - 150 ms##stable",
		{350, 48}))
		SelectLatencyProfile(false);

	ImGui::SetCursorPosY(510);
	CenterText(
		RestartAfterProfileChange
			? "Restarting the video session with the new latency profile..."
			: UltraLatency
				? "Ultra-low shortens receiver buffering and packet pacing; use Stable if Wi-Fi recovery suffers."
				: "Stable retains the proven 0.2.1 receiver buffer and packet pacing.");

	ImGui::SetCursorPos({70, 560});
	if (ImGui::Button(
		BootEnabled
			? "Disable automatic start"
			: "Start automatically on boot",
		{300, 44})) {
		if (!SetBootEnabled(!BootEnabled)) {
			app::FatalError(
				Strings::Error.BootModeChangeFailed,
				Strings::Error.TroubleshootBootMode);
		}
		else {
			BootEnabled = !BootEnabled;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(Strings::Main.OptGuide.c_str(), {150, 44}))
		app::SetNextScene(Scene::Guide);
	ImGui::SameLine();
	if (ImGui::Button(Strings::Main.OptPatchManager.c_str(), {250, 44}))
		app::SetNextScene(Scene::DvrPatches);
	ImGui::SameLine();
	if (ImGui::Button(
		Strings::Main.OptTerminateSysmodule.c_str(),
		{225, 44}))
		ImGui::OpenPopup("KillSwitchCast");
	ImGui::SameLine();
	if (ImGui::Button("Exit", {125, 44}))
		app::RequestExit();

	ImGui::SetCursorPosY(632);
	CenterText("Built on SysDVR by Exelix11 and contributors");
	ImGui::SetCursorPosY(665);
	CenterText("SysDVR remains the principal capture and sysmodule foundation of SwitchCast");

	DrawTerminatePopup();
	ImGui::End();
}
