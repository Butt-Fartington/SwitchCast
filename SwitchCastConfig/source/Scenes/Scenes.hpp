#pragma once

#include <cstdint>
#include <string_view>

enum class Scene {
	ModeSelect,
	CastPicker,
	Guide,
	FatalError,
	DvrPatches,
	NoConnection,
};

// Each implemented in its own scenes cpp file
namespace scenes {
	void InitModeSelect();
	void RefreshModeSelect();
	void SetModeSelectState(uint32_t mode);
	void ModeSelect();

	void OpenCastPicker();
	void CastPicker();

	void InitGuide();
	void Guide();

	void InitDvrPatches();
	void DeinitDvrPatches();
	void DvrPatches();

	void InitFatalError();
	void FatalError();
	
	void InitNoConnection();
	void NoConnection();
}

// from main.cpp
namespace app {
	void FatalError(std::string_view message, std::string_view secondline);
	void FatalErrorWithErrorCode(std::string_view message, uint32_t rc);
	
	void SetNextScene(Scene s);
	void ReturnToPreviousScene(); 

	void RequestExit();
}
