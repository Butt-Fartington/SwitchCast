#include "Scenes.hpp"
#include "Common.hpp"

#include "../CastDiscovery.hpp"
#include "../Platform/fs.hpp"
#include "../ipc.h"
#include "../translaton.hpp"

#include <cstdio>
#include <string>

#ifdef WIN32
	#include <WinSock2.h>
	#include <WS2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
#endif

namespace
{
	constexpr auto CastTargetPath =
		SDMC "/config/switchcast/receiver_ip";
	std::string ConfiguredTarget;
	std::string PickerError;
	char ManualIp[64] = {};

	std::string ReadCastTarget()
	{
		try {
			if (!fs::Exists(CastTargetPath))
				return {};

			auto data = fs::OpenFile(CastTargetPath);
			std::string target(data.begin(), data.end());
			while (!target.empty() && (
				target.back() == '\r' ||
				target.back() == '\n' ||
				target.back() == ' ' ||
				target.back() == '\t'))
				target.pop_back();
			return target;
		}
		catch (std::exception&)
		{
			return {};
		}
	}

	bool SaveCastTarget(std::string_view target)
	{
		try {
			fs::CreateDir(SDMC "/config/");
			fs::CreateDir(SDMC "/config/switchcast/");
			fs::Delete(CastTargetPath);
			if (!target.empty())
				fs::WriteFile(
					CastTargetPath,
					{ target.begin(), target.end() });
			ConfiguredTarget = target;
			return true;
		}
		catch (std::exception&)
		{
			return false;
		}
	}

	bool IsValidIpv4Address(const char* address)
	{
		in_addr parsed = {};
		return address &&
			inet_pton(AF_INET, address, &parsed) == 1 &&
			parsed.s_addr != htonl(INADDR_ANY) &&
			parsed.s_addr != INADDR_NONE;
	}

	void ClosePicker()
	{
		CastDiscovery::Stop();
		scenes::RefreshModeSelect();
		app::ReturnToPreviousScene();
	}

	bool ActivateTarget(std::string_view target)
	{
		if (!SaveCastTarget(target))
		{
			PickerError = Strings::Main.CastPickerSaveFailed;
			return false;
		}

		const Result rc = SwitchCastSetEnabled(true);
		if (R_FAILED(rc))
		{
			CastDiscovery::Stop();
			app::FatalErrorWithErrorCode(Strings::Error.ModeChangeFailed, rc);
			return false;
		}

		scenes::SetModeSelectState(TYPE_MODE_CAST);
		ClosePicker();
		return true;
	}
}

void scenes::OpenCastPicker()
{
	ConfiguredTarget = ReadCastTarget();
	std::snprintf(
		ManualIp,
		sizeof(ManualIp),
		"%s",
		ConfiguredTarget.c_str());
	PickerError.clear();
	CastDiscovery::Start();
	app::SetNextScene(Scene::CastPicker);
}

void scenes::CastPicker()
{
	CastDiscovery::Poll();

	SetupMainWindow("SwitchCast receiver picker");

	CenterImage(SwitchCastLogo, .26f);
	CenterText(Strings::Main.CastPickerTitle);
	CenterText(
		"SwitchCast " SWITCHCAST_VERSION_STRING
		" - standalone Cast streaming");
	CenterText("Built on SysDVR by Exelix11 and contributors");
	ImGui::Spacing();
	ImGui::TextWrapped(Strings::Main.CastPickerDescription.c_str());

	const auto currentReceiver =
		ConfiguredTarget.empty()
			? Strings::Main.CastPickerAutomatic
			: ConfiguredTarget;
	ImGui::Text(
		"%s %s",
		Strings::Main.CastPickerCurrent.c_str(),
		currentReceiver.c_str());

	ImGui::Spacing();
	if (ImGui::Button(
		Strings::Main.CastPickerAutomatic.c_str(),
		{ ImGui::GetContentRegionAvail().x, 0 }))
	{
		if (ActivateTarget({}))
		{
			ImGui::End();
			return;
		}
	}

	ImGui::Separator();
	ImGui::BeginChild(
		"CastReceiverList",
		{ 0, 330 },
		true,
		ImGuiWindowFlags_AlwaysVerticalScrollbar);

	const auto& receivers = CastDiscovery::GetReceivers();
	for (const auto& receiver : receivers)
	{
		std::string label = receiver.Name + " - " + receiver.IpAddress;
		if (receiver.IpAddress == ConfiguredTarget)
			label += "  *";

		ImGui::PushID(receiver.IpAddress.c_str());
		if (ImGui::Button(
			label.c_str(),
			{ ImGui::GetContentRegionAvail().x, 46 }))
		{
			ImGui::PopID();
			ImGui::EndChild();
			if (ActivateTarget(receiver.IpAddress))
			{
				ImGui::End();
				return;
			}
			goto receiver_list_done;
		}
		ImGui::PopID();
	}

	if (CastDiscovery::IsScanning())
		ImGui::TextUnformatted(Strings::Main.CastPickerSearching.c_str());
	else if (receivers.empty())
		ImGui::TextWrapped(Strings::Main.CastPickerNoneFound.c_str());

	ImGui::EndChild();

receiver_list_done:
	if (ImGui::Button(Strings::Main.CastPickerRescan.c_str(), { 180, 0 }))
	{
		PickerError.clear();
		CastDiscovery::Start();
	}

	ImGui::Separator();
	ImGui::PushItemWidth(410);
	ImGui::InputText(
		Strings::Main.CastPickerManualIp.c_str(),
		ManualIp,
		sizeof(ManualIp),
		ImGuiInputTextFlags_CharsDecimal);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button(Strings::Main.CastPickerUseManual.c_str()))
	{
		if (IsValidIpv4Address(ManualIp))
		{
			if (ActivateTarget(ManualIp))
			{
				ImGui::End();
				return;
			}
		}
		else
			PickerError = Strings::Main.CastPickerInvalidIp;
	}

	if (!PickerError.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
		ImGui::TextWrapped(PickerError.c_str());
		ImGui::PopStyleColor();
	}

	if (ImGui::Button(
		Strings::Main.CastPickerCancel.c_str(),
		{ ImGui::GetContentRegionAvail().x, 0 }))
		ClosePicker();

	ImGui::End();
}
