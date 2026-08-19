#include "SamolotyGameUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	void SetGameSettingCVar(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

void USamolotyGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	SetOverallScalabilityLevel(1);
	SetResolutionScaleValueEx(100.0f);
	SetVSyncEnabled(false);
	SetFrameRateLimit(120.0f);
	AntiAliasingMethod = 1;
	bVolumetricCloudsEnabled = false;
	MasterVolume = 1.0f;
	bShowFPS = false;
	bShowPing = false;
}

void USamolotyGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	ApplyProjectRenderingSettings();
}

void USamolotyGameUserSettings::SetAntiAliasingMethod(const int32 NewMethod)
{
	AntiAliasingMethod = NewMethod == 2 || NewMethod == 4 ? NewMethod : 1;
}

void USamolotyGameUserSettings::SetVolumetricCloudsEnabled(const bool bEnabled)
{
	bVolumetricCloudsEnabled = bEnabled;
}

void USamolotyGameUserSettings::SetMasterVolume(const float NewVolume)
{
	MasterVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
}

void USamolotyGameUserSettings::ApplyProjectRenderingSettings() const
{
	SetGameSettingCVar(TEXT("r.AntiAliasingMethod"), AntiAliasingMethod);
	SetGameSettingCVar(TEXT("r.VolumetricCloud"), bVolumetricCloudsEnabled ? 1 : 0);
	if (GEngine)
	{
		if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw())
		{
			AudioDevice->SetTransientPrimaryVolume(
				FMath::Clamp(MasterVolume, 0.0f, 1.0f));
		}
	}
}
