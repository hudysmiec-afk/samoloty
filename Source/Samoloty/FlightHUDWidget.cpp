#include "FlightHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The lock artwork contains long outer ticks and a large empty center, so the
	// same canvas size makes its useful reticle look much smaller than the selected marker.
	constexpr float LockMarkerArtworkScale = 1.55f;
}

UFlightHUDWidget::UFlightHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTextureFinder(
		TEXT("/Game/Main/Images/HUD/IMG_Crosshair.IMG_Crosshair"));
	if (CrosshairTextureFinder.Succeeded())
	{
		CrosshairTexture = CrosshairTextureFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> LockMarkerTextureFinder(
		TEXT("/Game/Main/Images/HUD/IMG_LockMarker.IMG_LockMarker"));
	if (LockMarkerTextureFinder.Succeeded())
	{
		LockMarkerTexture = LockMarkerTextureFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarBackgroundFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarBackground.IMG_RadarBackground"));
	RadarBackgroundTexture = RadarBackgroundFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarFrameFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarFrame.IMG_RadarFrame"));
	RadarFrameTexture = RadarFrameFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarPlayerFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarPlayer.IMG_RadarPlayer"));
	RadarPlayerTexture = RadarPlayerFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarContactFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarContact.IMG_RadarContact"));
	RadarContactTexture = RadarContactFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarAboveFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarAbove.IMG_RadarAbove"));
	RadarAboveTexture = RadarAboveFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarBelowFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarBelow.IMG_RadarBelow"));
	RadarBelowTexture = RadarBelowFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadarTargetFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_RadarTarget.IMG_RadarTarget"));
	RadarTargetTexture = RadarTargetFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> SelectedTargetDirectionFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_SelectedTargetDirection.IMG_SelectedTargetDirection"));
	SelectedTargetDirectionTexture = SelectedTargetDirectionFinder.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackerDirectionFinder(
		TEXT("/Game/Main/Images/HUD/Radar/IMG_AttackerDirection.IMG_AttackerDirection"));
	AttackerDirectionTexture = AttackerDirectionFinder.Object;
}

bool UFlightHUDWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	BuildNativeWidgetTree();
	return true;
}

void UFlightHUDWidget::BuildNativeWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		// Blueprint-derived HUDs already own a widget tree. Reuse their root canvas
		// and append native status elements instead of silently skipping them.
		RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (RootCanvas)
		{
			BuildStatusWidgetTree();
		}
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	CrosshairImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CrosshairImage"));
	CrosshairImage->SetBrushFromTexture(CrosshairTexture, false);
	CrosshairImage->SetColorAndOpacity(FLinearColor::White);
	CrosshairImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CrosshairSlot = RootCanvas->AddChildToCanvas(CrosshairImage))
	{
		CrosshairSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CrosshairSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CrosshairSlot->SetPosition(FVector2D::ZeroVector);
		CrosshairSlot->SetSize(CrosshairSize);
		CrosshairSlot->SetAutoSize(false);
		CrosshairSlot->SetZOrder(10);
	}

	LockMarkerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LockMarkerImage"));
	LockMarkerImage->SetBrushFromTexture(LockMarkerTexture, false);
	LockMarkerImage->SetColorAndOpacity(FLinearColor(1.0f, 0.04f, 0.02f, 1.0f));
	LockMarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	LockMarkerSlot = RootCanvas->AddChildToCanvas(LockMarkerImage);
	if (LockMarkerSlot)
	{
		LockMarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		LockMarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		// Compensate for transparent padding/spacing inside the lock artwork so its
		// visible reticle matches the selected-target marker's apparent size.
		LockMarkerSlot->SetSize(WorldTargetMarkerSize * LockMarkerArtworkScale);
		LockMarkerSlot->SetAutoSize(false);
		LockMarkerSlot->SetZOrder(5);
	}

	SelectedTargetMarkerImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("SelectedTargetMarkerImage"));
	SelectedTargetMarkerImage->SetBrushFromTexture(RadarTargetTexture, false);
	SelectedTargetMarkerImage->SetColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.03f, 1.0f));
	SelectedTargetMarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	SelectedTargetMarkerSlot = RootCanvas->AddChildToCanvas(SelectedTargetMarkerImage);
	if (SelectedTargetMarkerSlot)
	{
		SelectedTargetMarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		SelectedTargetMarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SelectedTargetMarkerSlot->SetSize(WorldTargetMarkerSize);
		SelectedTargetMarkerSlot->SetAutoSize(false);
		SelectedTargetMarkerSlot->SetZOrder(6);
	}

	SelectedTargetDirectionImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("SelectedTargetDirectionImage"));
	SelectedTargetDirectionImage->SetBrushFromTexture(SelectedTargetDirectionTexture, false);
	SelectedTargetDirectionImage->SetColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.03f, 1.0f));
	SelectedTargetDirectionImage->SetVisibility(ESlateVisibility::Collapsed);
	SelectedTargetDirectionSlot = RootCanvas->AddChildToCanvas(SelectedTargetDirectionImage);
	if (SelectedTargetDirectionSlot)
	{
		SelectedTargetDirectionSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		SelectedTargetDirectionSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SelectedTargetDirectionSlot->SetSize(SelectedTargetDirectionSize);
		SelectedTargetDirectionSlot->SetAutoSize(false);
		SelectedTargetDirectionSlot->SetZOrder(7);
	}

	SelectedTargetDistanceText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SelectedTargetDistanceText"));
	FSlateFontInfo DistanceFont = SelectedTargetDistanceText->GetFont();
	DistanceFont.Size = FMath::RoundToInt(SelectedTargetDistanceFontSize);
	SelectedTargetDistanceText->SetFont(DistanceFont);
	SelectedTargetDistanceText->SetJustification(ETextJustify::Center);
	SelectedTargetDistanceText->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.72f, 0.03f, 1.0f)));
	SelectedTargetDistanceText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	SelectedTargetDistanceText->SetShadowColorAndOpacity(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	SelectedTargetDistanceText->SetVisibility(ESlateVisibility::Collapsed);
	SelectedTargetDistanceSlot = RootCanvas->AddChildToCanvas(SelectedTargetDistanceText);
	if (SelectedTargetDistanceSlot)
	{
		SelectedTargetDistanceSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		SelectedTargetDistanceSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SelectedTargetDistanceSlot->SetAutoSize(true);
		SelectedTargetDistanceSlot->SetZOrder(8);
	}

	BuildRadarWidgetTree();
	BuildStatusWidgetTree();
}

void UFlightHUDWidget::BuildStatusWidgetTree()
{
	if (!RootCanvas || !WidgetTree || HealthBar)
	{
		return;
	}

	PerformanceStatsText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PerformanceStatsText"));
	FSlateFontInfo PerformanceFont = PerformanceStatsText->GetFont();
	PerformanceFont.Size = 18;
	PerformanceStatsText->SetFont(PerformanceFont);
	PerformanceStatsText->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.35f, 1.0f, 0.4f, 1.0f)));
	PerformanceStatsText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	PerformanceStatsText->SetShadowColorAndOpacity(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	PerformanceStatsText->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PerformanceSlot =
		RootCanvas->AddChildToCanvas(PerformanceStatsText))
	{
		PerformanceSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		PerformanceSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		PerformanceSlot->SetPosition(FVector2D(24.0f, 24.0f));
		PerformanceSlot->SetAutoSize(true);
		PerformanceSlot->SetZOrder(30);
	}

	SpeedText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SpeedText"));
	FSlateFontInfo SpeedFont = SpeedText->GetFont();
	SpeedFont.Size = 20;
	SpeedText->SetFont(SpeedFont);
	SpeedText->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.25f, 0.85f, 1.0f, 1.0f)));
	SpeedText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	SpeedText->SetShadowColorAndOpacity(FLinearColor::Black);
	SpeedText->SetText(FText::FromString(TEXT("SPEED  0 m/s")));
	if (UCanvasPanelSlot* SpeedSlot = RootCanvas->AddChildToCanvas(SpeedText))
	{
		SpeedSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		SpeedSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		SpeedSlot->SetPosition(FVector2D(32.0f, -152.0f));
		SpeedSlot->SetAutoSize(true);
		SpeedSlot->SetZOrder(20);
	}

	AbilityCooldownContainer = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("AbilityCooldownContainer"));
	if (UCanvasPanelSlot* AbilityContainerSlot =
		RootCanvas->AddChildToCanvas(AbilityCooldownContainer))
	{
		AbilityContainerSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		AbilityContainerSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		AbilityContainerSlot->SetPosition(FVector2D(0.0f, -28.0f));
		AbilityContainerSlot->SetAutoSize(true);
		AbilityContainerSlot->SetZOrder(25);
	}

	const TCHAR* AbilityWidgetNames[] = {
		TEXT("QuickReversal"), TEXT("BackwardDash"), TEXT("ForwardDash"),
		TEXT("Blink"), TEXT("EvasiveRoll")
	};
	for (const TCHAR* AbilityWidgetName : AbilityWidgetNames)
	{
		USizeBox* AbilitySize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*(FString(AbilityWidgetName) + TEXT("Size"))));
		AbilitySize->SetWidthOverride(165.0f);
		AbilitySize->SetHeightOverride(40.0f);
		if (UHorizontalBoxSlot* AbilityHorizontalSlot =
			AbilityCooldownContainer->AddChildToHorizontalBox(AbilitySize))
		{
			AbilityHorizontalSlot->SetPadding(FMargin(3.0f, 0.0f));
		}

		UBorder* AbilityBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			FName(*(FString(AbilityWidgetName) + TEXT("Border"))));
		AbilityBorder->SetPadding(FMargin(6.0f, 4.0f));
		AbilityBorder->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.52f));
		AbilitySize->AddChild(AbilityBorder);

		UTextBlock* AbilityText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*(FString(AbilityWidgetName) + TEXT("Text"))));
		FSlateFontInfo AbilityFont = AbilityText->GetFont();
		AbilityFont.Size = 12;
		AbilityText->SetFont(AbilityFont);
		AbilityText->SetJustification(ETextJustify::Center);
		AbilityText->SetAutoWrapText(false);
		AbilityText->SetClipping(EWidgetClipping::ClipToBounds);
		AbilityText->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.62f, 0.68f, 0.74f, 0.85f)));
		AbilityText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		AbilityText->SetShadowColorAndOpacity(FLinearColor::Black);
		if (UBorderSlot* AbilityTextSlot =
			Cast<UBorderSlot>(AbilityBorder->AddChild(AbilityText)))
		{
			AbilityTextSlot->SetHorizontalAlignment(HAlign_Fill);
			AbilityTextSlot->SetVerticalAlignment(VAlign_Center);
		}
		AbilityCooldownBorders.Add(AbilityBorder);
		AbilityCooldownTexts.Add(AbilityText);
	}
	SetAbilityCooldowns(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	struct FBarSpec
	{
		const TCHAR* BarName;
		const TCHAR* TextName;
		float Y;
		FLinearColor Color;
		TObjectPtr<UProgressBar>* Bar;
		TObjectPtr<UTextBlock>* Text;
	};
	FBarSpec Specs[] = {
		{TEXT("HealthBar"), TEXT("HealthText"), -120.0f,
			FLinearColor(0.08f, 0.85f, 0.18f, 1.0f), &HealthBar, &HealthText},
		{TEXT("BoostBar"), TEXT("BoostText"), -88.0f,
			FLinearColor(0.05f, 0.65f, 1.0f, 1.0f), &BoostBar, &BoostText},
		{TEXT("GunCooldownBar"), TEXT("GunCooldownText"), -56.0f,
			FLinearColor(1.0f, 0.65f, 0.08f, 1.0f), &GunCooldownBar, &GunCooldownText},
		{TEXT("MissileCooldownBar"), TEXT("MissileCooldownText"), -24.0f,
			FLinearColor(0.9f, 0.2f, 0.08f, 1.0f), &MissileCooldownBar, &MissileCooldownText}
	};
	for (FBarSpec& Spec : Specs)
	{
		UOverlay* BarLayer = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), FName(*(FString(Spec.BarName) + TEXT("Layer"))));
		BarLayer->SetClipping(EWidgetClipping::ClipToBounds);
		if (UCanvasPanelSlot* LayerSlot = RootCanvas->AddChildToCanvas(BarLayer))
		{
			LayerSlot->SetAnchors(FAnchors(0.0f, 1.0f));
			LayerSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			LayerSlot->SetPosition(FVector2D(32.0f, Spec.Y));
			LayerSlot->SetSize(FVector2D(360.0f, 24.0f));
			LayerSlot->SetZOrder(20);
		}

		*Spec.Bar = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), Spec.BarName);
		(*Spec.Bar)->SetFillColorAndOpacity(Spec.Color);
		(*Spec.Bar)->SetPercent(1.0f);
		if (UOverlaySlot* BarSlot = BarLayer->AddChildToOverlay(*Spec.Bar))
		{
			BarSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			BarSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
		}
		*Spec.Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), Spec.TextName);
		FSlateFontInfo Font = (*Spec.Text)->GetFont();
		Font.Size = 13;
		(*Spec.Text)->SetFont(Font);
		(*Spec.Text)->SetJustification(ETextJustify::Center);
		(*Spec.Text)->SetAutoWrapText(false);
		(*Spec.Text)->SetClipping(EWidgetClipping::ClipToBounds);
		(*Spec.Text)->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		(*Spec.Text)->SetShadowOffset(FVector2D(1.0f, 1.0f));
		(*Spec.Text)->SetShadowColorAndOpacity(FLinearColor::Black);
		if (UOverlaySlot* TextSlot = BarLayer->AddChildToOverlay(*Spec.Text))
		{
			TextSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			TextSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
		}
	}

	BoundaryWarningText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BoundaryWarningText"));
	FSlateFontInfo WarningFont = BoundaryWarningText->GetFont();
	WarningFont.Size = 30;
	BoundaryWarningText->SetFont(WarningFont);
	BoundaryWarningText->SetJustification(ETextJustify::Center);
	BoundaryWarningText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	BoundaryWarningText->SetShadowColorAndOpacity(FLinearColor::Black);
	BoundaryWarningText->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* WarningSlot = RootCanvas->AddChildToCanvas(BoundaryWarningText))
	{
		WarningSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		WarningSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		WarningSlot->SetPosition(FVector2D(0.0f, 70.0f));
		WarningSlot->SetSize(FVector2D(720.0f, 48.0f));
		WarningSlot->SetZOrder(40);
	}
}

void UFlightHUDWidget::SetPerformanceStats(const bool bShowFPS, const float FPS,
	const bool bShowPing, const float PingMilliseconds)
{
	if (!PerformanceStatsText)
	{
		return;
	}
	if (!bShowFPS && !bShowPing)
	{
		PerformanceStatsText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TArray<FString> Lines;
	if (bShowFPS)
	{
		Lines.Add(FString::Printf(TEXT("FPS: %d"),
			FMath::Max(0, FMath::RoundToInt(FPS))));
	}
	if (bShowPing)
	{
		Lines.Add(FString::Printf(TEXT("PING: %d ms"),
			FMath::Max(0, FMath::RoundToInt(PingMilliseconds))));
	}
	PerformanceStatsText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	PerformanceStatsText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFlightHUDWidget::SetAbilityCooldowns(const float QuickReversalRemaining,
	const float BackwardDashRemaining, const float ForwardDashRemaining,
	const float BlinkRemaining, const float EvasiveRollRemaining)
{
	static const TCHAR* Labels[] = {
		TEXT("Q  TURN"), TEXT("E  REVERSE"), TEXT("C  DASH"),
		TEXT("F  BLINK"), TEXT("A/D  ROLL")
	};
	const float RemainingTimes[] = {
		QuickReversalRemaining, BackwardDashRemaining, ForwardDashRemaining,
		BlinkRemaining, EvasiveRollRemaining
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
	{
		UBorder* Border = AbilityCooldownBorders.IsValidIndex(Index)
			? AbilityCooldownBorders[Index].Get() : nullptr;
		UTextBlock* Text = AbilityCooldownTexts.IsValidIndex(Index)
			? AbilityCooldownTexts[Index].Get() : nullptr;
		if (!Border || !Text)
		{
			continue;
		}
		const float Remaining = FMath::Max(0.0f, RemainingTimes[Index]);
		const bool bOnCooldown = Remaining > 0.05f;
		Text->SetText(FText::FromString(bOnCooldown
			? FString::Printf(TEXT("%s  %.1fs"), Labels[Index], Remaining)
			: FString::Printf(TEXT("%s  READY"), Labels[Index])));
		Text->SetColorAndOpacity(FSlateColor(bOnCooldown
			? FLinearColor(1.0f, 0.92f, 0.72f, 1.0f)
			: FLinearColor(0.62f, 0.68f, 0.74f, 0.85f)));
		Border->SetBrushColor(bOnCooldown
			? FLinearColor(0.62f, 0.18f, 0.02f, 0.76f)
			: FLinearColor(0.015f, 0.025f, 0.04f, 0.52f));
	}
}

void UFlightHUDWidget::SetBoundaryWarning(const float WarningAlpha,
	const bool bAutomaticReturn)
{
	if (!BoundaryWarningText)
	{
		return;
	}
	const bool bVisible = bAutomaticReturn || WarningAlpha > 0.01f;
	BoundaryWarningText->SetVisibility(bVisible
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}
	BoundaryWarningText->SetText(FText::FromString(bAutomaticReturn
		? TEXT("AUTOMATIC RETURN") : TEXT("WARNING: MAP BOUNDARY")));
	const float Pulse = 0.82f + 0.18f * FMath::Sin(
		GetWorld()->GetTimeSeconds() * (bAutomaticReturn ? 8.0f : 5.0f));
	const FLinearColor WarningColor = bAutomaticReturn
		? FLinearColor(1.0f, 0.08f, 0.03f, Pulse)
		: FLinearColor(1.0f, 0.55f, 0.03f,
			FMath::Lerp(0.45f, Pulse, FMath::Clamp(WarningAlpha, 0.0f, 1.0f)));
	BoundaryWarningText->SetColorAndOpacity(FSlateColor(WarningColor));
}

void UFlightHUDWidget::UpdateWeaponCooldown(UProgressBar* Bar, UTextBlock* Text,
	const FName WeaponName, const float Remaining, const float Duration)
{
	if (!Bar || !Text)
	{
		return;
	}
	const float ReadyAlpha = Duration > UE_SMALL_NUMBER
		? 1.0f - FMath::Clamp(Remaining / Duration, 0.0f, 1.0f) : 1.0f;
	Bar->SetPercent(ReadyAlpha);
	const FString State = Remaining > 0.01f
		? FString::Printf(TEXT("%.1fs"), Remaining) : TEXT("READY");
	Text->SetText(FText::FromString(FString::Printf(
		TEXT("%s  %s"), *WeaponName.ToString().ToUpper(), *State)));
}

void UFlightHUDWidget::SetPlayerStatus(const float CurrentHealth,
	const float MaxHealth, const float CurrentBoost, const float MaxBoost,
	const FName GunName, const float GunCooldownRemaining,
	const float GunCooldownDuration, const FName MissileName,
	const float MissileCooldownRemaining, const float MissileCooldownDuration,
	const float SpeedCentimetersPerSecond)
{
	if (SpeedText)
	{
		SpeedText->SetText(FText::FromString(FString::Printf(
			TEXT("SPEED  %.0f m/s"),
			FMath::Max(0.0f, SpeedCentimetersPerSecond) / 100.0f)));
	}
	if (HealthBar && HealthText)
	{
		HealthBar->SetPercent(MaxHealth > UE_SMALL_NUMBER
			? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f);
		HealthText->SetText(FText::FromString(FString::Printf(
			TEXT("HP  %.0f / %.0f"), CurrentHealth, MaxHealth)));
	}
	if (BoostBar && BoostText)
	{
		BoostBar->SetPercent(MaxBoost > UE_SMALL_NUMBER
			? FMath::Clamp(CurrentBoost / MaxBoost, 0.0f, 1.0f) : 0.0f);
		BoostText->SetText(FText::FromString(FString::Printf(
			TEXT("BOOST  %.0f / %.0f"), CurrentBoost, MaxBoost)));
	}
	UpdateWeaponCooldown(GunCooldownBar, GunCooldownText,
		GunName, GunCooldownRemaining, GunCooldownDuration);
	UpdateWeaponCooldown(MissileCooldownBar, MissileCooldownText,
		MissileName, MissileCooldownRemaining, MissileCooldownDuration);
}

void UFlightHUDWidget::EnsureAttackerDirectionPool(const int32 RequiredCount)
{
	if (!RootCanvas || !WidgetTree)
	{
		return;
	}
	while (AttackerDirectionImages.Num() < RequiredCount)
	{
		const int32 Index = AttackerDirectionImages.Num();
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("AttackerDirection_%d"), Index));
		Image->SetBrushFromTexture(AttackerDirectionTexture, false);
		Image->SetColorAndOpacity(FLinearColor(1.0f, 0.04f, 0.02f, 1.0f));
		Image->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* DirectionSlot = RootCanvas->AddChildToCanvas(Image);
		if (DirectionSlot)
		{
			DirectionSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			DirectionSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DirectionSlot->SetSize(AttackerDirectionSize);
			DirectionSlot->SetAutoSize(false);
			DirectionSlot->SetZOrder(6);
		}
		AttackerDirectionImages.Add(Image);
		AttackerDirectionSlots.Add(DirectionSlot);
	}
}

void UFlightHUDWidget::EnsureDamageNumberPool(const int32 RequiredCount)
{
	if (!RootCanvas || !WidgetTree)
	{
		return;
	}
	while (DamageNumberTexts.Num() < RequiredCount)
	{
		const int32 Index = DamageNumberTexts.Num();
		UTextBlock* DamageText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("DamageNumber_%d"), Index));
		FSlateFontInfo Font = DamageText->GetFont();
		Font.Size = 28;
		DamageText->SetFont(Font);
		DamageText->SetJustification(ETextJustify::Center);
		DamageText->SetShadowOffset(FVector2D(2.0f, 2.0f));
		DamageText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		DamageText->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* DamageSlot = RootCanvas->AddChildToCanvas(DamageText);
		if (DamageSlot)
		{
			DamageSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			DamageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DamageSlot->SetAutoSize(true);
			DamageSlot->SetZOrder(30);
		}
		DamageNumberTexts.Add(DamageText);
		DamageNumberSlots.Add(DamageSlot);
	}
}

void UFlightHUDWidget::SetDamageNumbers(
	const TArray<FDamageNumberDisplay>& DamageNumbers)
{
	EnsureDamageNumberPool(DamageNumbers.Num());
	for (int32 Index = 0; Index < DamageNumberTexts.Num(); ++Index)
	{
		UTextBlock* DamageText = DamageNumberTexts[Index];
		UCanvasPanelSlot* DamageSlot = DamageNumberSlots.IsValidIndex(Index)
			? DamageNumberSlots[Index] : nullptr;
		if (!DamageNumbers.IsValidIndex(Index) || !DamageText || !DamageSlot)
		{
			if (DamageText)
			{
				DamageText->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		const FDamageNumberDisplay& Display = DamageNumbers[Index];
		DamageText->SetText(FText::FromString(Display.Text));
		DamageText->SetColorAndOpacity(FSlateColor(FLinearColor(
			1.0f, 0.68f, 0.08f, FMath::Clamp(Display.Opacity, 0.0f, 1.0f))));
		DamageText->SetRenderScale(FVector2D(Display.Scale));
		DamageSlot->SetPosition(Display.Position);
		DamageText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UFlightHUDWidget::BuildRadarWidgetTree()
{
	if (!RootCanvas || !WidgetTree)
	{
		return;
	}

	RadarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RadarCanvas"));
	RadarCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* RadarSlot = RootCanvas->AddChildToCanvas(RadarCanvas))
	{
		RadarSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		RadarSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		RadarSlot->SetPosition(FVector2D(-RadarScreenMargin.X, RadarScreenMargin.Y));
		RadarSlot->SetSize(RadarSize);
		RadarSlot->SetAutoSize(false);
		RadarSlot->SetZOrder(20);
	}

	auto AddRadarImage = [this](const FName Name, UTexture2D* Texture,
		const FVector2D& Size, const FVector2D& Position, const int32 ZOrder)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Image->SetBrushFromTexture(Texture, false);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* ImageSlot = RadarCanvas->AddChildToCanvas(Image))
		{
			ImageSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			ImageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ImageSlot->SetPosition(Position);
			ImageSlot->SetSize(Size);
			ImageSlot->SetAutoSize(false);
			ImageSlot->SetZOrder(ZOrder);
		}
		return Image;
	};

	const FVector2D Center = RadarSize * 0.5f;
	AddRadarImage(TEXT("RadarBackground"), RadarBackgroundTexture, RadarSize, Center, 0);
	AddRadarImage(TEXT("RadarPlayer"), RadarPlayerTexture, RadarPlayerSize, Center, 40);

	RadarSelectedImage = AddRadarImage(TEXT("RadarSelected"), RadarTargetTexture,
		RadarSelectedSize, Center, 30);
	RadarSelectedImage->SetColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.03f, 1.0f));
	RadarSelectedImage->SetVisibility(ESlateVisibility::Collapsed);
	RadarSelectedSlot = Cast<UCanvasPanelSlot>(RadarSelectedImage->Slot);

	AddRadarImage(TEXT("RadarFrame"), RadarFrameTexture, RadarSize, Center, 50);
}

void UFlightHUDWidget::EnsureRadarContactPool(const int32 RequiredCount)
{
	if (!RadarCanvas || !WidgetTree)
	{
		return;
	}
	while (RadarContactImages.Num() < RequiredCount)
	{
		const int32 Index = RadarContactImages.Num();
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("RadarContact_%d"), Index));
		Image->SetVisibility(ESlateVisibility::Collapsed);
		Image->SetColorAndOpacity(FLinearColor(1.0f, 0.08f, 0.04f, 1.0f));
		UCanvasPanelSlot* ContactSlot = RadarCanvas->AddChildToCanvas(Image);
		if (ContactSlot)
		{
			ContactSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			ContactSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ContactSlot->SetSize(RadarContactSize);
			ContactSlot->SetAutoSize(false);
			ContactSlot->SetZOrder(20);
		}
		RadarContactImages.Add(Image);
		RadarContactSlots.Add(ContactSlot);
	}
}

UTexture2D* UFlightHUDWidget::GetRadarContactTexture(const ERadarContactIcon Icon) const
{
	switch (Icon)
	{
	case ERadarContactIcon::Above:
		return RadarAboveTexture;
	case ERadarContactIcon::Below:
		return RadarBelowTexture;
	default:
		return RadarContactTexture;
	}
}

void UFlightHUDWidget::SetRadarContacts(const bool bVisible,
	const TArray<FRadarDisplayContact>& Contacts)
{
	if (!RadarCanvas)
	{
		return;
	}
	RadarCanvas->SetVisibility(bVisible
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}

	EnsureRadarContactPool(Contacts.Num());
	const FVector2D Center = RadarSize * 0.5f;
	const float ContactRadius = FMath::Min(RadarSize.X, RadarSize.Y) * 0.38f;
	bool bSelectedVisible = false;
	for (int32 Index = 0; Index < RadarContactImages.Num(); ++Index)
	{
		UImage* Image = RadarContactImages[Index];
		UCanvasPanelSlot* ContactSlot = RadarContactSlots.IsValidIndex(Index)
			? RadarContactSlots[Index].Get() : nullptr;
		if (!Image || !ContactSlot || !Contacts.IsValidIndex(Index))
		{
			if (Image)
			{
				Image->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		const FRadarDisplayContact& Contact = Contacts[Index];
		const FVector2D Position = Center + Contact.NormalizedPosition * ContactRadius;
		Image->SetBrushFromTexture(GetRadarContactTexture(Contact.Icon), false);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		ContactSlot->SetPosition(Position);
		if (Contact.bSelected && RadarSelectedImage && RadarSelectedSlot)
		{
			RadarSelectedSlot->SetPosition(Position);
			RadarSelectedImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			bSelectedVisible = true;
		}
	}

	if (RadarSelectedImage && !bSelectedVisible)
	{
		RadarSelectedImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFlightHUDWidget::SetCombatElementsVisible(const bool bVisible)
{
	bCombatElementsVisible = bVisible;
	if (AbilityCooldownContainer)
	{
		AbilityCooldownContainer->SetVisibility(bVisible
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CrosshairImage)
	{
		CrosshairImage->SetVisibility(
			bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bVisible && LockMarkerImage)
	{
		LockMarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!bVisible && SelectedTargetMarkerImage)
	{
		SelectedTargetMarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!bVisible && SelectedTargetDirectionImage)
	{
		SelectedTargetDirectionImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!bVisible && SelectedTargetDistanceText)
	{
		SelectedTargetDistanceText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!bVisible)
	{
		for (UImage* Image : AttackerDirectionImages)
		{
			if (Image)
			{
				Image->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UFlightHUDWidget::SetLockMarkerPosition(const bool bVisible, const FVector2D& WidgetPosition)
{
	if (!LockMarkerImage || !LockMarkerSlot)
	{
		return;
	}
	const bool bShowMarker = bCombatElementsVisible && bVisible;
	LockMarkerImage->SetVisibility(
		bShowMarker ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowMarker)
	{
		LockMarkerSlot->SetPosition(WidgetPosition);
	}
}

void UFlightHUDWidget::SetSelectedTargetIndicator(const bool bHasTarget,
	const bool bTargetOnScreen, const FVector2D& WidgetPosition,
	const FVector2D& ScreenDirection, const FVector2D& WidgetViewportSize,
	const float DistanceCentimeters)
{
	if (!SelectedTargetMarkerImage || !SelectedTargetMarkerSlot
		|| !SelectedTargetDirectionImage || !SelectedTargetDirectionSlot)
	{
		return;
	}

	const bool bShowOnScreen = bCombatElementsVisible && bHasTarget && bTargetOnScreen;
	SelectedTargetMarkerImage->SetVisibility(bShowOnScreen
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowOnScreen)
	{
		SelectedTargetMarkerSlot->SetPosition(WidgetPosition);
	}

	const bool bShowDirection = bCombatElementsVisible && bHasTarget && !bTargetOnScreen;
	SelectedTargetDirectionImage->SetVisibility(bShowDirection
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	FVector2D IndicatorPosition = WidgetPosition;
	if (bShowDirection)
	{
		const FVector2D Direction = ScreenDirection.GetSafeNormal();
		const FVector2D Center = WidgetViewportSize * 0.5f;
		const float Radius = FMath::Min(WidgetViewportSize.X, WidgetViewportSize.Y)
			* SelectedTargetDirectionRadius;
		IndicatorPosition = Center + Direction * Radius;
		SelectedTargetDirectionSlot->SetPosition(IndicatorPosition);
		SelectedTargetDirectionImage->SetRenderTransformAngle(
			FMath::RadiansToDegrees(FMath::Atan2(Direction.X, -Direction.Y)));
	}

	const bool bShowDistance = bCombatElementsVisible && bHasTarget
		&& SelectedTargetDistanceText && SelectedTargetDistanceSlot;
	if (SelectedTargetDistanceText)
	{
		SelectedTargetDistanceText->SetVisibility(bShowDistance
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowDistance)
	{
		const float DistanceMeters = FMath::Max(0.0f, DistanceCentimeters * 0.01f);
		const FString DistanceString = DistanceMeters >= 1000.0f
			? FString::Printf(TEXT("%.1f km"), DistanceMeters / 1000.0f)
			: FString::Printf(TEXT("%.0f m"), DistanceMeters);
		SelectedTargetDistanceText->SetText(FText::FromString(DistanceString));
		SelectedTargetDistanceSlot->SetPosition(
			IndicatorPosition + SelectedTargetDistanceOffset);
	}
}

void UFlightHUDWidget::SetAttackerDirections(const TArray<FVector2D>& ScreenDirections,
	const FVector2D& WidgetViewportSize)
{
	EnsureAttackerDirectionPool(ScreenDirections.Num());
	const FVector2D Center = WidgetViewportSize * 0.5f;
	const float Radius = FMath::Min(WidgetViewportSize.X, WidgetViewportSize.Y)
		* AttackerDirectionRadius;
	for (int32 Index = 0; Index < AttackerDirectionImages.Num(); ++Index)
	{
		UImage* Image = AttackerDirectionImages[Index];
		UCanvasPanelSlot* DirectionSlot = AttackerDirectionSlots.IsValidIndex(Index)
			? AttackerDirectionSlots[Index].Get() : nullptr;
		const bool bShow = bCombatElementsVisible && Image && DirectionSlot
			&& ScreenDirections.IsValidIndex(Index);
		if (Image)
		{
			Image->SetVisibility(bShow
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (bShow)
		{
			const FVector2D Direction = ScreenDirections[Index].GetSafeNormal();
			DirectionSlot->SetPosition(Center + Direction * Radius);
			Image->SetRenderTransformAngle(
				FMath::RadiansToDegrees(FMath::Atan2(Direction.X, -Direction.Y)));
		}
	}
}
