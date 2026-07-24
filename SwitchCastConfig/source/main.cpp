/*
 * The SwitchCast Settings application is derived from the SysDVR Settings
 * application by Exelix11 and contributors. See SYSDVR-ATTRIBUTION.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "UI/UI.hpp"
#include "Platform/Platform.hpp"
#include "Platform/fs.hpp"
#include "UI/imgui/imgui.h"
#include "UI/Image.hpp"
#include "ipc.h"

#include "Scenes/Scenes.hpp"
#include "Scenes/Common.hpp"

#include "translaton.hpp"

Image::Img SwitchCastLogo;

namespace {
	Scene currentScene = Scene::FatalError;
	Scene returnTo = Scene::FatalError;

	std::string_view statusMessage = "Fatal error";
	std::string_view errorSecondline = "This should never happen, try rebooting your console";

	std::string formattedError;

	void ImguiBindController()
	{
		ImGuiIO& io = ImGui::GetIO();

		io.NavInputs[ImGuiNavInput_DpadDown] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN];
		io.NavInputs[ImGuiNavInput_DpadUp] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP];
		io.NavInputs[ImGuiNavInput_DpadLeft] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT];
		io.NavInputs[ImGuiNavInput_DpadRight] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT];

		io.NavInputs[ImGuiNavInput_Activate] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_A];
		io.NavInputs[ImGuiNavInput_Cancel] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_B];
		
		io.NavInputs[ImGuiNavInput_Menu] = g_gamepad.buttons[GLFW_GAMEPAD_BUTTON_START];
	}
		
	Result ConnectToSysmodule()
	{
		// SwitchCast delays initialization during boot so critical services can
		// settle before its capture and network resources are opened.
		int connectAttemptCount = 0;
		int connectMaxAttempt = 9;
		int connDelaySeconds = 3;

		try_again:

		UI::StartFrame();

		SetupMainWindow("Connecting...");
		CenterImage(SwitchCastLogo, 1);

		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::NewLine();

		CenterText(Strings::Connecting.Title);
		CenterText(Strings::Connecting.Description);

		std::string attemptLine = std::to_string(connectAttemptCount + 1) + "/" + std::to_string(connectMaxAttempt);
		UI::SmallFont();
		CenterText(attemptLine);
		UI::PopFont();

		ImGui::NewLine();
		CenterText(Strings::Connecting.Troubleshoot1);
		CenterText(Strings::Connecting.Troubleshoot2);

		ImGui::End();
		UI::EndFrame();

		Result rc = SwitchCastConnect();
		if (R_FAILED(rc))
		{
			if (SwitchCastProcIsRunning())
			{
				if (++connectAttemptCount == connectMaxAttempt)
					return rc;

				Platform::Sleep(connDelaySeconds * 1000);

				goto try_again;
			}
		}

		return rc;
	}
}

namespace scenes {
	void InitFatalError() 
	{
	}
	
	void FatalError()
	{
		SetupMainWindow("Fatal error");
		CenterImage(SwitchCastLogo, 1);

		CenterText(statusMessage);
		if (errorSecondline.size() > 0) {
			ImGui::PushTextWrapPos();
			ImGui::TextUnformatted(errorSecondline.data(), errorSecondline.data() + errorSecondline.size());
			ImGui::PopTextWrapPos();
		}

		ImGui::NewLine();
		CenterText("See SWITCHCAST.md in the release package");

		ImGui::NewLine();
		if (ImGuiCenterButtons<std::string_view>({ Strings::Error.FailExitButton }) != -1 || ImGui::GetIO().NavInputs[ImGuiNavInput_Menu])
			app::RequestExit();
		
		ImGui::End();
	}
}

namespace app {
	void SetNextScene(Scene s)
	{
		returnTo = currentScene;
		currentScene = s;
	}

	void ReturnToPreviousScene() 
	{
		currentScene = returnTo;
		returnTo = Scene::FatalError;
	}
	
	void FatalErrorWithErrorCode(std::string_view message, uint32_t rc)
	{
		if (rc == 0)
			formattedError = "No error code specified";
		else
			formattedError = "Error code: " + std::to_string(2000 + R_MODULE(rc)) + "-" + std::to_string(R_DESCRIPTION(rc));

		app::FatalError(message, formattedError);
	}

	void FatalError(std::string_view message, std::string_view secondline)
	{
		SetNextScene(Scene::FatalError);
		statusMessage = message;
		errorSecondline = secondline;
	}

	void RequestExit()
	{
		Glfw::SetShouldClose();
	}	
}

int main(int argc, char* argv[])
{
	Platform::Init();

	Strings::LoadTranslationForSystemLanguage();

	if (!UI::Init())
	{
		Platform::Exit();
		return -1;
	}
	
	
	Platform::AfterInit();
	
	// If this fails, it's fine
	SwitchCastProcManagerInit();

	SwitchCastLogo = Image::Img(ASSET("logo.png"));
	
	currentScene = Scene::ModeSelect;

	{
		Result rc = ConnectToSysmodule();
		if (R_FAILED(rc)) {
			if (!fs::Exists(SDMC SWITCHCAST_EXEFS_PATH))
			{
				app::FatalError(Strings::Error.NotInstalled, Strings::Error.NotInsalledSecondLine);
				goto mainloop;
			}

			app::SetNextScene(Scene::NoConnection);
			goto dvrNotConnected;
		}

		u32 version;
		rc = SwitchCastGetVersion(&version);
		if (R_FAILED(rc)) {
			app::FatalErrorWithErrorCode(Strings::Error.VersionError, rc);
			goto mainloop;
		}

		if (version > SWITCHCAST_IPC_VERSION) {
			app::FatalError(Strings::Error.NewerVersion, Strings::Error.VersionTroubleshoot);
			goto mainloop;
		}
		else if (version < SWITCHCAST_IPC_VERSION) {
			app::FatalError(Strings::Error.OlderVersion, Strings::Error.VersionTroubleshoot);
			goto mainloop;
		}
	}

	scenes::InitModeSelect();
	scenes::InitGuide();

dvrNotConnected:
	// dvr-patches remain useful for games that opt out of video capture.
	scenes::InitDvrPatches();

mainloop:
	// Always init the fatal error scene
	scenes::InitFatalError();
	scenes::InitNoConnection();
	
	while (Glfw::MainLoop())
	{
		Platform::GetInputs();
		ImguiBindController();
		Platform::ImguiBindings();

		UI::StartFrame();

		// Render background
		ImGui::SetNextWindowSize({ UI::WindowWidth , UI::WindowHeight });
		ImGui::SetNextWindowPos({ 0, 0 });

		ImGui::Begin("_background_", 0, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration);
		
		UI::SmallFont();

		switch (currentScene)
		{
		case Scene::ModeSelect:
			scenes::ModeSelect();
			break;
		case Scene::CastPicker:
			scenes::CastPicker();
			break;
		case Scene::Guide:
			scenes::Guide();
			break;
		case Scene::FatalError:
			scenes::FatalError();
			break;
		case Scene::DvrPatches:
			scenes::DvrPatches();
			break;
		case Scene::NoConnection:
			scenes::NoConnection();
			break;
		default:
			Glfw::SetShouldClose();
			break;
		}

		UI::PopFont();
		ImGui::End();

		UI::EndFrame();
		Platform::Sleep(1 / 30.0f * 1000);
	}

	scenes::DeinitDvrPatches();

	SwitchCastClose();
	SwitchCastProcManagerExit();

	UI::Exit();
	Platform::Exit();
	return EXIT_SUCCESS;
}
