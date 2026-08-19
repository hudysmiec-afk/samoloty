#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "SamolotyGameUserSettings.generated.h"

/** Persistent client rendering options not covered by standard scalability groups. */
UCLASS(Config=GameUserSettings)
class SAMOLOTY_API USamolotyGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	int32 GetAntiAliasingMethod() const { return AntiAliasingMethod; }
	void SetAntiAliasingMethod(int32 NewMethod);
	bool AreVolumetricCloudsEnabled() const { return bVolumetricCloudsEnabled; }
	void SetVolumetricCloudsEnabled(bool bEnabled);
	float GetMasterVolume() const { return MasterVolume; }
	void SetMasterVolume(float NewVolume);
	bool ShouldShowFPS() const { return bShowFPS; }
	void SetShowFPS(bool bEnabled) { bShowFPS = bEnabled; }
	bool ShouldShowPing() const { return bShowPing; }
	void SetShowPing(bool bEnabled) { bShowPing = bEnabled; }

private:
	void ApplyProjectRenderingSettings() const;

	/** 1 = FXAA, 2 = TAA, 4 = TSR. */
	UPROPERTY(Config)
	int32 AntiAliasingMethod = 1;

	UPROPERTY(Config)
	bool bVolumetricCloudsEnabled = false;

	UPROPERTY(Config)
	float MasterVolume = 1.0f;

	UPROPERTY(Config)
	bool bShowFPS = false;

	UPROPERTY(Config)
	bool bShowPing = false;
};
