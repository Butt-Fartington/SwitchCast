#include "Scenes.hpp"
#include "Common.hpp"

#include "../ipc.h"
#include "../translaton.hpp"

namespace {
bool CanStart;
bool ProcessRunning;
std::string StartResult;

void TryStartSwitchCast(void)
{
	CanStart = false;
	const Result rc = SwitchCastProcLaunch();
	if (R_FAILED(rc)) {
		StartResult =
			Strings::Main.StartFailed +
			std::to_string(2000 + R_MODULE(rc)) +
			"-" +
			std::to_string(R_DESCRIPTION(rc));
		return;
	}
	StartResult = Strings::Main.StartSuccess;
}
}

namespace scenes {
void InitNoConnection(void)
{
	ProcessRunning = SwitchCastProcIsRunning();
	CanStart =
		SwitchCastProcManagerIsInitialized() &&
		!ProcessRunning;
	StartResult.clear();
}

void NoConnection(void)
{
	SetupMainWindow("No connection");
	CenterImage(SwitchCastLogo, 1);

	if (StartResult.empty()) {
		CenterText(
			Strings::Error.SysmoduleConnectionFailed);
		CenterText(
			Strings::Error.SysmoduleConnectionTroubleshoot);
		ImGui::NewLine();
		CenterText(
			ProcessRunning
				? Strings::Error.DiagProcessStatusOn
				: Strings::Error.DiagProcessStatusOff);
		CenterText(
			Strings::Error.SysmoduleConnectionTroubleshootLink);

		const int selection = CanStart
			? ImGuiCenterButtons<std::string_view>({
				Strings::Error.FailExitButton,
				Strings::Main.OptPatchManager,
				Strings::Main.OptTryStart
			})
			: ImGuiCenterButtons<std::string_view>({
				Strings::Error.FailExitButton,
				Strings::Main.OptPatchManager
			});
		if (
			selection == 0 ||
			ImGui::GetIO().NavInputs[
				ImGuiNavInput_Menu])
			app::RequestExit();
		else if (selection == 1)
			app::SetNextScene(Scene::DvrPatches);
		else if (selection == 2)
			TryStartSwitchCast();
	}
	else {
		ImGui::NewLine();
		CenterText(StartResult);
		ImGui::NewLine();
		if (
			ImGuiCenterButtons<std::string_view>({
				Strings::Error.FailExitButton
			}) == 0 ||
			ImGui::GetIO().NavInputs[
				ImGuiNavInput_Menu])
			app::RequestExit();
	}
	ImGui::End();
}
}
