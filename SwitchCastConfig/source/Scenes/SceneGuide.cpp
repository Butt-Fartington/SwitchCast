#include "Scenes.hpp"
#include "Common.hpp"

#include "../translaton.hpp"

namespace scenes {
void InitGuide(void)
{
}

void Guide(void)
{
	SetupMainWindow("SwitchCast guide");
	CenterImage(SwitchCastLogo, .6f);
	ImGui::NewLine();
	CenterText("1. Choose Cast over Wi-Fi or USB Dock on the main screen.");
	CenterText("2. Start the selected transport; choose a receiver for Cast.");
	CenterText("3. For USB, connect the Switch after the Dock finishes booting.");
	CenterText("4. Exit this app and launch a capture-compatible game.");
	ImGui::NewLine();
	CenterText("The stream is video-only and the HOME menu is not captured.");
	CenterText("Read SWITCHCAST.md in the release package for troubleshooting.");
	ImGui::NewLine();
	CenterText("SwitchCast is built on SysDVR by Exelix11 and contributors.");
	CenterText("SysDVR supplied the principal capture and sysmodule foundation.");
	CenterText("github.com/exelix11/SysDVR");
	ImGui::NewLine();
	if (ImGuiCenterButtons({"Back"}) == 0)
		app::SetNextScene(Scene::ModeSelect);
	ImGui::End();
}
}
