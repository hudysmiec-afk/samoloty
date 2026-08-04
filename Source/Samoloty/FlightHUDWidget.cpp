#include "FlightHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

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
	if (!WidgetTree || WidgetTree->RootWidget)
	{
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
	LockMarkerImage->SetColorAndOpacity(FLinearColor::White);
	LockMarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	LockMarkerSlot = RootCanvas->AddChildToCanvas(LockMarkerImage);
	if (LockMarkerSlot)
	{
		LockMarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		LockMarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LockMarkerSlot->SetSize(LockMarkerSize);
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
		SelectedTargetMarkerSlot->SetSize(SelectedTargetMarkerSize);
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

	BuildRadarWidgetTree();
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
	const FVector2D& ScreenDirection, const FVector2D& WidgetViewportSize)
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
	if (bShowDirection)
	{
		const FVector2D Direction = ScreenDirection.GetSafeNormal();
		const FVector2D Center = WidgetViewportSize * 0.5f;
		const float Radius = FMath::Min(WidgetViewportSize.X, WidgetViewportSize.Y)
			* SelectedTargetDirectionRadius;
		SelectedTargetDirectionSlot->SetPosition(Center + Direction * Radius);
		SelectedTargetDirectionImage->SetRenderTransformAngle(
			FMath::RadiansToDegrees(FMath::Atan2(Direction.X, -Direction.Y)));
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
