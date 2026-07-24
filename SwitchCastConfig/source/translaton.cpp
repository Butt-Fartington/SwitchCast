#include <map>

#include "Libs/nlohmann/json.hpp"

#include "translaton.hpp"
#include "Platform/Platform.hpp"
#include "Platform/fs.hpp"

namespace Strings
{
	std::string FontName;
	GlyphRange ImguiFontGlyphRange;

	MainPageTable Main = {};
	GuideTable Guide = {};
	ErrorTable Error = {};
	PatchesTable Patches = {};
	ConnectingTable Connecting = {};

	// TODO: Uncomment the line of the language you want to add and change the name of the json
	static std::map<std::string, std::string> Translations = {
		{ "SetLanguage_ENUS", ASSET("strings/english.json")},
		{ "SetLanguage_ENGB", ASSET("strings/english.json")},
		{ "SetLanguage_IT", ASSET("strings/italian.json")},
		{ "SetLanguage_ZHCN", ASSET("strings/simplifiedChinese.json")},
		{ "SetLanguage_ZHHANS", ASSET("strings/simplifiedChinese.json")},
		{ "SetLanguage_ZHTW", ASSET("strings/traditionalChinese.json")},
		{ "SetLanguage_ZHHANT", ASSET("strings/traditionalChinese.json")},
		//{ "SetLanguage_JA", ASSET("strings/example.json")},
		{ "SetLanguage_FR", ASSET("strings/french.json")},
		//{ "SetLanguage_DE", ASSET("strings/example.json")},
		{ "SetLanguage_ES", ASSET("strings/spanish.json")},
		//{ "SetLanguage_KO", ASSET("strings/example.json")},
		//{ "SetLanguage_NL", ASSET("strings/example.json")},
		{ "SetLanguage_PT", ASSET("strings/portuguese.json")},
		//{ "SetLanguage_RU", ASSET("strings/example.json")},
		//{ "SetLanguage_FRCA", ASSET("strings/example.json")},
		{ "SetLanguage_ES419", ASSET("strings/spanish.json")},
		{ "SetLanguage_PTBR", ASSET("strings/brazilianPortuguese.json")}
	};

	#define STRING_META_ITERATE(v1) cb(ptr, obj.v1);

	// Use a version that accepts missing fields, they are default-initialized with english text
	#undef NLOHMANN_JSON_FROM
	#define NLOHMANN_JSON_FROM(v1) if (nlohmann_json_j.count(#v1)) nlohmann_json_j.at(#v1).get_to(nlohmann_json_t.v1);

	// We need to be able to iterate over all the strings in the language structs to build the proper font atlases
	// This macro resues macro crimes from nlohmann/json.hpp to also generate the iterator we need
	#define DEFINE_LANGUAGE_JSON_PARSER(classname, ...) \
	static void iterate_strings_##classname(const classname& obj, void* ptr, void (*cb)(void*, std::string_view s)) \
	{ \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(STRING_META_ITERATE, __VA_ARGS__)) \
	} \
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(classname, __VA_ARGS__)

	// Keep the newest ASCII-only Cast diagnostics default-initialized instead
	// of exceeding nlohmann's 64-field non-intrusive parser macro.
	DEFINE_LANGUAGE_JSON_PARSER(MainPageTable, ModeCastTitle, ModeCast, CastPickerTitle, CastPickerDescription, CastPickerCurrent, CastPickerAutomatic, CastPickerSearching, CastPickerNoneFound, CastPickerRescan, CastPickerManualIp, CastPickerUseManual, CastPickerCancel, CastPickerInvalidIp, CastPickerSaveFailed, CastStatusPrefix, CastStatusOff, CastStatusDiscovering, CastStatusConnecting, CastStatusWaitingHttp, CastStatusHttpConnected, CastStatusStreaming, CastStatusLoadFailed, CastStatusHttpTimeout, CastStatusNetworkError, CastStatusHlsError, CastStatusWaitingGame, CastStatusGameMonitorError, CastStatusMemoryError, CastStatusHlsStartupTimeout, CastStatusReceiverClosed, CastStatusControlError, CastStatusHttpRequestError, CastStatusPreparingHls, CastStatusControlSendError, CastStatusControlPollError, CastStatusControlHeaderError, CastStatusControlFrameTooLarge, CastStatusControlBodyError, CastStatusControlDecodeError, CastStatusControlEncodeError, ModeDisabled, OptGuide, OptSetDefault, OptPatchManager, OptSave, ActiveMode, DefaultMode, OptTerminateSysmodule, WarnSysmoduleKill, ContinueQuestion, Yes, No, OptTryStart, StartFailed, StartSuccess)

	DEFINE_LANGUAGE_JSON_PARSER(GuideTable, GuideTitle)

	DEFINE_LANGUAGE_JSON_PARSER(ErrorTable, FailedToDetectMode, InvalidMode, TroubleshootReboot, ModeChangeFailed, BootModeChangeFailed, TroubleshootBootMode, SysmoduleConnectionFailed, SysmoduleConnectionTroubleshoot, SysmoduleConnectionTroubleshootLink, FailExitButton, VersionError, OlderVersion, NewerVersion, VersionTroubleshoot, DiagProcessStatusOn, DiagProcessStatusOff)

	DEFINE_LANGUAGE_JSON_PARSER(PatchesTable, CurlError, CurlGETFailed, CurlNoData, ParseReleaseFailure, ParseTagFailure, ParseTagCommitFailure, ParseDownloadFailure, NoLinkFound, ZipExtractFail, LatestVer, NewVerAvail, DownloadOk, Title, Loading, Description, Status, StatusNotInstalled, StatusUnknownVersion, StatusInstalled, SdcardPath, UninstallButton, DownloadButton, RebootWarning, RebootButton, BackButton, SearchLatestButton)

	DEFINE_LANGUAGE_JSON_PARSER(ConnectingTable, Title, Description, Troubleshoot1, Troubleshoot2)

	void IterateAllStringsForFontBuilding(void* obj, void (*cb)(void*, std::string_view))
	{
		iterate_strings_MainPageTable(Main, obj, cb);
		iterate_strings_ErrorTable(Error, obj, cb);
		iterate_strings_PatchesTable(Patches, obj, cb);
		iterate_strings_GuideTable(Guide, obj, cb);
		iterate_strings_ConnectingTable(Connecting, obj, cb);
	}

#define map(x) { Strings::GlyphRange::x, #x }

	NLOHMANN_JSON_SERIALIZE_ENUM(Strings::GlyphRange, {
		map(NotSpecified),
		map(ChineseSimplifiedCommon),
		map(Cyrillic),
		map(Default),
		map(Greek),
		map(Japanese),
		map(Korean),
		map(Thai),
		map(Vietnamese)
	})

#undef map

	struct TranslationFile
	{
		std::string LanguageName;
		std::string TranslationAuthor;
		std::string FontName;
		Strings::GlyphRange ImguiGlyphRange;

		MainPageTable Main;
		GuideTable Guide;
		ErrorTable Error;
		PatchesTable Patches;
		ConnectingTable Connecting;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(TranslationFile, LanguageName, TranslationAuthor, FontName, ImguiGlyphRange,
			Main, Guide, Error, Patches, Connecting)
	};
	
	void ResetStringTable()
	{
		FontName = ASSET("fonts/opensans.ttf");
		ImguiFontGlyphRange = GlyphRange::Default;
		Main = {};
		Guide = {};
		Error = {};
		Patches = {};
		Connecting = {};
	}

	static std::string ReadString(const std::string& path)
	{
		auto file = fs::OpenFile(path);
		return std::string(file.begin(), file.end());
	}

	static std::vector<uint8_t> GetLanguageJson()
	{
		if (fs::Exists(SDMC "/config/switchcast/language.json"))
		{
			return fs::OpenFile(SDMC "/config/switchcast/language.json");
		}

		auto name = std::string(Platform::GetSystemLanguage());
		if (fs::Exists(SDMC "/config/switchcast/force_language"))
		{
			auto forceName = ReadString(SDMC "/config/switchcast/force_language");
			if (Translations.count(forceName))
			{
				name = forceName;
			}
			else
			{
				printf("force_language does not contain a valid language: %s\n", forceName.c_str());
			}
		}
		
		if (Translations.count(name))
		{
			auto path = Translations[name];
			printf("Loading translation %s: %s\n", name.c_str(), path.c_str());
			return fs::OpenFile(path);
		}

		return {};
	}

	void LoadTranslationForSystemLanguage()
	{
		FontName = ASSET("fonts/opensans.ttf");

		TranslationFile translation;
		
		try {
			auto file = GetLanguageJson();
			if (file.empty())
				return;
			
			auto json = nlohmann::json::parse(file.begin(), file.end());
			translation = json.get<TranslationFile>();
		}
		catch (std::exception& ex)
		{
			printf("Failed to load translation: %s\n", ex.what());
			return;
		}

		if (translation.FontName != "")
		{
			auto fontPath = std::string(ASSET("fonts/")) + translation.FontName;
			if (!fs::Exists(fontPath))
			{
				printf("The specified font does not exist: %s %s\n", translation.FontName.c_str(), fontPath.c_str());
				return;
			}

			FontName = fontPath;
		}

		ImguiFontGlyphRange = translation.ImguiGlyphRange;

		Main = translation.Main;
		Guide = translation.Guide;
		Error = translation.Error;
		Patches = translation.Patches;
		Connecting = translation.Connecting;
	}

	// Only for development purposes
	void SerializeCurrentLanguage()
	{
		TranslationFile translation;
		nlohmann::json json = translation;

		std::string s = json.dump(4);
		std::vector<u8> data(s.begin(), s.end());
		fs::WriteFile("english.json", data);
	}
}
