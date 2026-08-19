#include "GraphicsSettingsWidget.h"

#include "SamolotyGameUserSettings.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"

namespace
{
	const FString CustomPreset(TEXT("Custom"));
	const FString LowQuality(TEXT("Low"));
	const FString MediumQuality(TEXT("Medium"));
	const FString HighQuality(TEXT("High"));
	const FString UltraQuality(TEXT("Ultra"));

	void AddOptions(UComboBoxString* Combo, const TArray<FString>& Options)
	{
		if (!Combo) return;
		Combo->ClearOptions();
		for (const FString& Option : Options) Combo->AddOption(Option);
	}

	FString QualityName(const int32 Quality)
	{
		switch (FMath::Clamp(Quality, 0, 3))
		{
		case 0: return LowQuality;
		case 1: return MediumQuality;
		case 2: return HighQuality;
		default: return UltraQuality;
		}
	}

	int32 QualityIndex(const FString& Name)
	{
		if (Name == LowQuality) return 0;
		if (Name == MediumQuality) return 1;
		if (Name == HighQuality) return 2;
		return 3;
	}

	FString FormatResolution(const FIntPoint Resolution)
	{
		return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
	}

	bool ParseResolution(const FString& Text, FIntPoint& OutResolution)
	{
		FString Width;
		FString Height;
		if (!Text.Split(TEXT("x"), &Width, &Height)) return false;
		OutResolution.X = FCString::Atoi(*Width.TrimStartAndEnd());
		OutResolution.Y = FCString::Atoi(*Height.TrimStartAndEnd());
		return OutResolution.X > 0 && OutResolution.Y > 0;
	}

	FString WindowModeName(const EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Fullscreen: return TEXT("Fullscreen");
		case EWindowMode::Windowed: return TEXT("Windowed");
		default: return TEXT("Borderless");
		}
	}

	EWindowMode::Type WindowModeFromName(const FString& Name)
	{
		if (Name == TEXT("Fullscreen")) return EWindowMode::Fullscreen;
		if (Name == TEXT("Windowed")) return EWindowMode::Windowed;
		return EWindowMode::WindowedFullscreen;
	}

	FString AntiAliasingName(const int32 Method)
	{
		if (Method == 2) return TEXT("TAA");
		if (Method == 4) return TEXT("TSR");
		return TEXT("FXAA");
	}

	int32 AntiAliasingMethodFromName(const FString& Name)
	{
		if (Name == TEXT("TAA")) return 2;
		if (Name == TEXT("TSR")) return 4;
		return 1;
	}
}

void UGraphicsSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PopulateOptions();

	CMB_QualityPreset->OnSelectionChanged.RemoveDynamic(
		this, &UGraphicsSettingsWidget::HandlePresetChanged);
	CMB_QualityPreset->OnSelectionChanged.AddDynamic(
		this, &UGraphicsSettingsWidget::HandlePresetChanged);
	SLD_ResolutionScale->OnValueChanged.RemoveDynamic(
		this, &UGraphicsSettingsWidget::HandleResolutionScaleChanged);
	SLD_ResolutionScale->OnValueChanged.AddDynamic(
		this, &UGraphicsSettingsWidget::HandleResolutionScaleChanged);
	if (SLD_MasterVolume)
	{
		SLD_MasterVolume->OnValueChanged.RemoveDynamic(
			this, &UGraphicsSettingsWidget::HandleMasterVolumeChanged);
		SLD_MasterVolume->OnValueChanged.AddDynamic(
			this, &UGraphicsSettingsWidget::HandleMasterVolumeChanged);
	}
	BTN_Apply->OnClicked.RemoveDynamic(this, &UGraphicsSettingsWidget::HandleApply);
	BTN_Apply->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::HandleApply);
	BTN_Reset->OnClicked.RemoveDynamic(this, &UGraphicsSettingsWidget::HandleReset);
	BTN_Reset->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::HandleReset);
	BTN_Back->OnClicked.RemoveDynamic(this, &UGraphicsSettingsWidget::HandleBack);
	BTN_Back->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::HandleBack);

	LoadSavedSettings();
}

void UGraphicsSettingsWidget::PopulateOptions()
{
	AddOptions(CMB_QualityPreset,
		{CustomPreset, LowQuality, MediumQuality, HighQuality, UltraQuality});
	AddOptions(CMB_WindowMode, {TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed")});
	AddOptions(CMB_AntiAliasing, {TEXT("FXAA"), TEXT("TAA"), TEXT("TSR")});
	AddOptions(CMB_Shadows, {LowQuality, MediumQuality, HighQuality, UltraQuality});
	AddOptions(CMB_GlobalIllumination, {LowQuality, MediumQuality, HighQuality, UltraQuality});
	AddOptions(CMB_Reflections, {LowQuality, MediumQuality, HighQuality, UltraQuality});
	AddOptions(CMB_Clouds, {TEXT("Off"), TEXT("On")});
	AddOptions(CMB_FrameLimit,
		{TEXT("Unlimited"), TEXT("30"), TEXT("60"), TEXT("90"), TEXT("120"),
		 TEXT("144"), TEXT("165"), TEXT("240")});
	AddOptions(CMB_Resolution,
		{TEXT("1280 x 720"), TEXT("1600 x 900"), TEXT("1920 x 1080"),
		 TEXT("2560 x 1440"), TEXT("3840 x 2160")});
}

void UGraphicsSettingsWidget::LoadSavedSettings()
{
	UGameUserSettings* Settings = GetSettings();
	if (!Settings)
	{
		SetStatus(NSLOCTEXT("GraphicsSettings", "Unavailable",
			"Graphics settings are unavailable."), true);
		return;
	}

	bUpdatingControls = true;
	const int32 OverallQuality = Settings->GetOverallScalabilityLevel();
	CMB_QualityPreset->SetSelectedOption(
		OverallQuality >= 0 && OverallQuality <= 3 ? QualityName(OverallQuality) : CustomPreset);

	const FString CurrentResolution = FormatResolution(Settings->GetScreenResolution());
	if (CMB_Resolution->FindOptionIndex(CurrentResolution) == INDEX_NONE)
		CMB_Resolution->AddOption(CurrentResolution);
	CMB_Resolution->SetSelectedOption(CurrentResolution);
	CMB_WindowMode->SetSelectedOption(WindowModeName(Settings->GetFullscreenMode()));
	CMB_Shadows->SetSelectedOption(QualityName(Settings->GetShadowQuality()));
	CMB_GlobalIllumination->SetSelectedOption(
		QualityName(Settings->GetGlobalIlluminationQuality()));
	CMB_Reflections->SetSelectedOption(QualityName(Settings->GetReflectionQuality()));
	CHK_VSync->SetIsChecked(Settings->IsVSyncEnabled());

	float NormalizedScale = 1.0f;
	float ScaleValue = 100.0f;
	float MinimumScale = 50.0f;
	float MaximumScale = 100.0f;
	Settings->GetResolutionScaleInformationEx(
		NormalizedScale, ScaleValue, MinimumScale, MaximumScale);
	SLD_ResolutionScale->SetMinValue(FMath::Max(25.0f, MinimumScale));
	SLD_ResolutionScale->SetMaxValue(FMath::Max(MinimumScale, MaximumScale));
	SLD_ResolutionScale->SetValue(ScaleValue);
	UpdateResolutionScaleText(ScaleValue);

	const float FrameLimit = Settings->GetFrameRateLimit();
	const FString FrameOption = FrameLimit <= 0.0f
		? TEXT("Unlimited") : FString::FromInt(FMath::RoundToInt(FrameLimit));
	if (CMB_FrameLimit->FindOptionIndex(FrameOption) == INDEX_NONE)
		CMB_FrameLimit->AddOption(FrameOption);
	CMB_FrameLimit->SetSelectedOption(FrameOption);

	if (const USamolotyGameUserSettings* ProjectSettings = GetProjectSettings())
	{
		CMB_AntiAliasing->SetSelectedOption(
			AntiAliasingName(ProjectSettings->GetAntiAliasingMethod()));
		CMB_Clouds->SetSelectedOption(
			ProjectSettings->AreVolumetricCloudsEnabled() ? TEXT("On") : TEXT("Off"));
		if (SLD_MasterVolume)
		{
			SLD_MasterVolume->SetMinValue(0.0f);
			SLD_MasterVolume->SetMaxValue(100.0f);
			SLD_MasterVolume->SetValue(ProjectSettings->GetMasterVolume() * 100.0f);
			UpdateMasterVolumeText(SLD_MasterVolume->GetValue());
		}
		if (CHK_ShowFPS)
		{
			CHK_ShowFPS->SetIsChecked(ProjectSettings->ShouldShowFPS());
		}
		if (CHK_ShowPing)
		{
			CHK_ShowPing->SetIsChecked(ProjectSettings->ShouldShowPing());
		}
	}
	else
	{
		const IConsoleVariable* AntiAliasing =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
		const IConsoleVariable* Clouds =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud"));
		CMB_AntiAliasing->SetSelectedOption(
			AntiAliasingName(AntiAliasing ? AntiAliasing->GetInt() : 1));
		CMB_Clouds->SetSelectedOption(Clouds && Clouds->GetInt() != 0 ? TEXT("On") : TEXT("Off"));
	}
	bUpdatingControls = false;
	SetStatus(FText::GetEmpty());
}

void UGraphicsSettingsWidget::ApplyPresetToControls(const FString& Preset)
{
	if (Preset == CustomPreset) return;
	const int32 Quality = Preset == LowQuality ? 0
		: Preset == MediumQuality ? 1 : Preset == HighQuality ? 2 : 3;
	bUpdatingControls = true;
	CMB_Shadows->SetSelectedOption(QualityName(Quality));
	CMB_GlobalIllumination->SetSelectedOption(QualityName(Quality));
	CMB_Reflections->SetSelectedOption(QualityName(Quality));
	CMB_AntiAliasing->SetSelectedOption(
		Quality <= 1 ? TEXT("FXAA") : Quality == 2 ? TEXT("TAA") : TEXT("TSR"));
	CMB_Clouds->SetSelectedOption(Quality <= 1 ? TEXT("Off") : TEXT("On"));
	SLD_ResolutionScale->SetValue(Quality == 0 ? 80.0f : 100.0f);
	UpdateResolutionScaleText(SLD_ResolutionScale->GetValue());
	bUpdatingControls = false;
}

void UGraphicsSettingsWidget::SetStatus(const FText& Text, const bool bError) const
{
	TXT_SettingsStatus->SetText(Text);
	TXT_SettingsStatus->SetColorAndOpacity(FSlateColor(
		bError ? FLinearColor(1.0f, 0.15f, 0.08f) : FLinearColor(0.25f, 0.8f, 1.0f)));
}

void UGraphicsSettingsWidget::UpdateResolutionScaleText(const float Value) const
{
	TXT_ResolutionScaleValue->SetText(FText::FromString(
		FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value))));
}

void UGraphicsSettingsWidget::UpdateMasterVolumeText(const float Value) const
{
	if (TXT_MasterVolumeValue)
	{
		TXT_MasterVolumeValue->SetText(FText::FromString(
			FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value))));
	}
}

UGameUserSettings* UGraphicsSettingsWidget::GetSettings() const
{
	return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

USamolotyGameUserSettings* UGraphicsSettingsWidget::GetProjectSettings() const
{
	return Cast<USamolotyGameUserSettings>(GetSettings());
}

void UGraphicsSettingsWidget::HandlePresetChanged(
	FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (!bUpdatingControls) ApplyPresetToControls(SelectedItem);
}

void UGraphicsSettingsWidget::HandleResolutionScaleChanged(const float Value)
{
	UpdateResolutionScaleText(Value);
}

void UGraphicsSettingsWidget::HandleMasterVolumeChanged(const float Value)
{
	UpdateMasterVolumeText(Value);
}

void UGraphicsSettingsWidget::HandleApply()
{
	UGameUserSettings* Settings = GetSettings();
	USamolotyGameUserSettings* ProjectSettings = GetProjectSettings();
	if (!Settings || !ProjectSettings)
	{
		SetStatus(NSLOCTEXT("GraphicsSettings", "RestartRequired",
			"Restart the editor once so the project settings class can be loaded."), true);
		return;
	}

	const FString Preset = CMB_QualityPreset->GetSelectedOption();
	if (Preset != CustomPreset)
		Settings->SetOverallScalabilityLevel(QualityIndex(Preset));

	FIntPoint Resolution;
	if (ParseResolution(CMB_Resolution->GetSelectedOption(), Resolution))
		Settings->SetScreenResolution(Resolution);
	Settings->SetFullscreenMode(WindowModeFromName(CMB_WindowMode->GetSelectedOption()));
	Settings->SetResolutionScaleValueEx(SLD_ResolutionScale->GetValue());
	Settings->SetShadowQuality(QualityIndex(CMB_Shadows->GetSelectedOption()));
	Settings->SetGlobalIlluminationQuality(
		QualityIndex(CMB_GlobalIllumination->GetSelectedOption()));
	Settings->SetReflectionQuality(QualityIndex(CMB_Reflections->GetSelectedOption()));
	Settings->SetVSyncEnabled(CHK_VSync->IsChecked());

	const FString FrameLimit = CMB_FrameLimit->GetSelectedOption();
	Settings->SetFrameRateLimit(FrameLimit == TEXT("Unlimited")
		? 0.0f : static_cast<float>(FCString::Atoi(*FrameLimit)));
	ProjectSettings->SetAntiAliasingMethod(
		AntiAliasingMethodFromName(CMB_AntiAliasing->GetSelectedOption()));
	ProjectSettings->SetVolumetricCloudsEnabled(
		CMB_Clouds->GetSelectedOption() == TEXT("On"));
	if (SLD_MasterVolume)
	{
		ProjectSettings->SetMasterVolume(SLD_MasterVolume->GetValue() / 100.0f);
	}
	if (CHK_ShowFPS)
	{
		ProjectSettings->SetShowFPS(CHK_ShowFPS->IsChecked());
	}
	if (CHK_ShowPing)
	{
		ProjectSettings->SetShowPing(CHK_ShowPing->IsChecked());
	}

	Settings->ApplySettings(false);
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();
	SetStatus(NSLOCTEXT("GraphicsSettings", "Applied", "Settings applied and saved."));
}

void UGraphicsSettingsWidget::HandleReset()
{
	bUpdatingControls = true;
	CMB_QualityPreset->SetSelectedOption(MediumQuality);
	bUpdatingControls = false;
	ApplyPresetToControls(MediumQuality);
	CMB_WindowMode->SetSelectedOption(TEXT("Borderless"));
	CHK_VSync->SetIsChecked(false);
	CMB_FrameLimit->SetSelectedOption(TEXT("120"));
	if (SLD_MasterVolume)
	{
		SLD_MasterVolume->SetValue(100.0f);
		UpdateMasterVolumeText(100.0f);
	}
	if (CHK_ShowFPS)
	{
		CHK_ShowFPS->SetIsChecked(false);
	}
	if (CHK_ShowPing)
	{
		CHK_ShowPing->SetIsChecked(false);
	}
	SetStatus(NSLOCTEXT("GraphicsSettings", "DefaultsPrepared",
		"Recommended defaults prepared. Press Apply to save them."));
}

void UGraphicsSettingsWidget::HandleBack()
{
	RemoveFromParent();
}
