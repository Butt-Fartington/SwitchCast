#pragma once
#include <string>
#include <vector>

namespace Strings
{
	struct MainPageTable
	{
		std::string ModeCastTitle = "SwitchCast";
		std::string ModeCast = "Send gameplay video to a Chromecast or Google Cast TV on the same network.\n"
			"Uses the receiver's built-in native mirroring transport: no hosted receiver or developer app ID.\n"
			"Video-only; capture and casting start while a compatible game is running.";

		std::string CastPickerTitle = "Select a Cast receiver";
		std::string CastPickerDescription = "Choose a Chromecast or Google Cast TV on this network. SwitchCast saves the receiver for future game sessions.";
		std::string CastPickerCurrent = "Current receiver:";
		std::string CastPickerAutomatic = "Automatic (first receiver discovered)";
		std::string CastPickerSearching = "Searching for Cast receivers...";
		std::string CastPickerNoneFound = "No receivers found. Check that the Switch and receiver are on the same network, then rescan.";
		std::string CastPickerRescan = "Rescan";
		std::string CastPickerManualIp = "Manual IPv4 address";
		std::string CastPickerUseManual = "Use address";
		std::string CastPickerCancel = "Cancel";
		std::string CastPickerInvalidIp = "Enter a valid IPv4 address.";
		std::string CastPickerSaveFailed = "Couldn't save the selected receiver to the SD card.";
		std::string CastStatusPrefix = "SwitchCast status:";
		std::string CastStatusOff = "Off";
		std::string CastStatusDiscovering = "Discovering receiver";
		std::string CastStatusConnecting = "Opening the Cast control channel";
		std::string CastStatusWaitingHttp = "Negotiating native Cast Streaming";
		std::string CastStatusHttpConnected = "UDP video session ready; waiting for a keyframe";
		std::string CastStatusStreaming = "Native low-latency gameplay video is streaming";
		std::string CastStatusLoadFailed = "Receiver rejected the built-in streaming app or media offer";
		std::string CastStatusHttpTimeout = "Receiver did not answer the native media offer";
		std::string CastStatusNetworkError = "Network or receiver connection failed";
		std::string CastStatusHlsError = "Cast RTP packet transmission failed";
		std::string CastStatusWaitingGame = "Armed; waiting for a capture-compatible game";
		std::string CastStatusGameMonitorError = "Unable to monitor running games";
		std::string CastStatusMemoryError = "Not enough memory for SwitchCast; disable other sysmodules";
		std::string CastStatusHlsStartupTimeout = "Gameplay did not provide valid H.264 setup data";
		std::string CastStatusReceiverClosed = "Receiver closed the Cast session";
		std::string CastStatusControlError = "Cast control channel failed";
		std::string CastStatusHttpRequestError = "Could not build the native Cast Streaming offer";
		std::string CastStatusPreparingHls = "Reserved SwitchCast state";
		std::string CastStatusControlSendError = "Cast TLS send failed";
		std::string CastStatusControlPollError = "Cast TLS poll failed";
		std::string CastStatusControlHeaderError = "Cast control header read failed";
		std::string CastStatusControlFrameTooLarge = "Cast control frame size was invalid or exceeded 8 KiB";
		std::string CastStatusControlBodyError = "Cast control body read failed";
		std::string CastStatusControlDecodeError = "Cast control protobuf decode failed";
		std::string CastStatusControlEncodeError = "Cast control protobuf encode failed";
		std::string CastStatusControlPendingError = "Cast TLS pending check failed";
		std::string CastStatusControlSocketPollError = "Cast socket poll failed";
		std::string CastStatusControlSocketClosed = "Cast control socket closed";
		std::string CastStatusReceiverStatusTimeout = "Receiver did not return Cast app status";
		std::string CastStatusMediaErrorCode = "Receiver media error code";
		std::string CastStatusAppIdMissing = "Reserved SwitchCast state 29";
		std::string CastStatusReceiverReadyTimeout = "Refreshing the Cast media session after interrupted feedback";
		std::string CastStatusReceiverStreamError = "Reserved SwitchCast state 31";

		std::string ModeDisabled = "Stop streaming";

		std::string OptGuide = "Guide";
		std::string OptSetDefault = "Set current mode as default on boot";
		std::string OptPatchManager = "dvr-patches manager";
		std::string OptSave = "Save and exit";

		std::string ActiveMode = "Enabled";
		std::string DefaultMode = "On boot";

		std::string OptTerminateSysmodule = "Terminate SwitchCast";
		std::string WarnSysmoduleKill = "This will terminate the SwitchCast process and free its memory for other sysmodules to use.\nNote that this option may cause conflicts with third-party sysmodule managers.";
		std::string ContinueQuestion = "Do you want to continue?";
		std::string Yes = "Yes";
		std::string No = "No";

		std::string OptTryStart = "Try to start SwitchCast";
		std::string StartFailed = "Launching SwitchCast failed. Error code: ";
		std::string StartSuccess = "SwitchCast was started. Close and open this app again.";
	};

	struct GuideTable
	{
		std::string GuideTitle = "The guide is hosted on Github:";
	};

	struct ErrorTable
	{
		std::string FailedToDetectMode = "Failed to detect the current SwitchCast mode";
		std::string InvalidMode = "The reported SwitchCast mode is not valid";
		std::string TroubleshootReboot = "Try rebooting your console";
		std::string ModeChangeFailed = "Failed to change mode";
		std::string BootModeChangeFailed = "Couldn't set boot mode";
		std::string TroubleshootBootMode = "Try checking your SD card for corruption";

		std::string SysmoduleConnectionFailed = "Couldn't connect to SwitchCast.";
		std::string SysmoduleConnectionTroubleshoot = "If you just installed it, reboot; otherwise wait a moment and try again.";
		std::string SysmoduleConnectionTroubleshootLink = "For support check the troubleshooting page:";

		std::string FailExitButton = "Click or press + to exit";

		std::string VersionError = "Couldn't get the SwitchCast version";

		std::string OlderVersion = "You're using an outdated version of SwitchCast";
		std::string NewerVersion = "You're using a newer version of SwitchCast";

		std::string VersionTroubleshoot = "Please download the latest settings app from github.";

		std::string NotInstalled = "SwitchCast is not installed on your console";
		std::string NotInsalledSecondLine = "The file /atmosphere/contents/00FF000053434153/exefs.nsp was not found. Extract the complete SwitchCast package to the SD root, then reboot.";

		std::string DiagProcessStatusOn = "The sysmodule process seems to be running.";
		std::string DiagProcessStatusOff = "The sysmodule process is not running.";
	};

	struct PatchesTable
	{
		std::string CurlError = "Curl initialization failed";
		std::string CurlGETFailed = "Curl GET failed";
		std::string CurlNoData = "No data received";
		std::string ParseReleaseFailure = "Failed to parse release info";
		std::string ParseTagFailure = "Failed to parse github tags info";
		std::string ParseTagCommitFailure = "Failed to find commit for tag";
		std::string ParseDownloadFailure = "Failed to find the download link for the release";

		std::string NoLinkFound = "No update source found";
		std::string ZipExtractFail = "Failed to open zip archive";

		std::string LatestVer = "You're already using latest version of dvr-patches.";
		std::string NewVerAvail = "New version of dvr-patches available:";
		std::string DownloadOk = "Update downloaded.";

		std::string Title = "dvr-patches manager";
		std::string Loading = "Loading...";

		std::string Description = "dvr-patches are system patches that allow SwitchCast to capture games that normally opt out of video recording; a few games are reported to crash. Read the issue tracker at https://github.com/exelix11/dvr-patches.\n"
			"You can also manually download the zip file from the GitHub repo with a computer.";

		std::string Status = "dvr-patches status:";

		std::string StatusNotInstalled = "not installed";
		std::string StatusUnknownVersion = "installed, version unknown";
		std::string StatusInstalled = "installed commit %s";

		std::string SdcardPath = "Sdcard path:";
		std::string UninstallButton = "Uninstall";
		std::string DownloadButton = "Download and install";

		std::string RebootWarning = "To apply the changes you need to reboot your console    ";

		std::string RebootButton = "Reboot now";

		std::string BackButton = "Go back";
		std::string SearchLatestButton = "Search for latest patches on GitHub";
	};

	struct ConnectingTable
	{
		std::string Title = "Connecting to SwitchCast...";

		std::string Description = "If you just turned on your console this may take up to 20 seconds.";

		std::string Troubleshoot1 = "If you can't get past this screen SwitchCast is not running";
		std::string Troubleshoot2 = "Make sure your setup is correct and reboot your console.";
	};

	enum class GlyphRange
	{
		NotSpecified,
		ChineseSimplifiedCommon,
		Cyrillic,
		Default,
		Greek,
		Japanese,
		Korean,
		Thai,
		Vietnamese
	};

	extern std::string FontName;
	extern GlyphRange ImguiFontGlyphRange;

	extern MainPageTable Main;
	extern GuideTable Guide;
	extern ErrorTable Error;
	extern PatchesTable Patches;
	extern ConnectingTable Connecting;

	void LoadTranslationForSystemLanguage();
	void IterateAllStringsForFontBuilding(void*, void (*)(void*, std::string_view));

	void ResetStringTable();

	// For development only
	void SerializeCurrentLanguage();
}
