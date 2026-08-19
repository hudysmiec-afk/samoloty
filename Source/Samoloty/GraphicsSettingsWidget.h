#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "GraphicsSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UGameUserSettings;
class USamolotyGameUserSettings;

/** C++ logic base for the visual WBP_Settings widget. */
UCLASS(Blueprintable)
class SAMOLOTY_API UGraphicsSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_QualityPreset;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_Resolution;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_WindowMode;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_AntiAliasing;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<USlider> SLD_ResolutionScale;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> TXT_ResolutionScaleValue;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_Shadows;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_GlobalIllumination;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_Reflections;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_Clouds;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UCheckBox> CHK_VSync;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UComboBoxString> CMB_FrameLimit;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<USlider> SLD_MasterVolume;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_MasterVolumeValue;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> CHK_ShowFPS;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> CHK_ShowPing;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> TXT_SettingsStatus;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_Reset;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_Back;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_Apply;

private:
	void PopulateOptions();
	void LoadSavedSettings();
	void ApplyPresetToControls(const FString& Preset);
	void SetStatus(const FText& Text, bool bError = false) const;
	void UpdateResolutionScaleText(float Value) const;
	void UpdateMasterVolumeText(float Value) const;
	UGameUserSettings* GetSettings() const;
	USamolotyGameUserSettings* GetProjectSettings() const;

	UFUNCTION()
	void HandlePresetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleResolutionScaleChanged(float Value);
	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);
	UFUNCTION()
	void HandleApply();
	UFUNCTION()
	void HandleReset();
	UFUNCTION()
	void HandleBack();

	bool bUpdatingControls = false;
};
