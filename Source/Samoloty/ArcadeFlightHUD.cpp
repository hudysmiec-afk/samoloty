#include "ArcadeFlightHUD.h"

#include "ArcadeFlightComponent.h"
#include "FlightHUDWidget.h"
#include "HealthComponent.h"
#include "MissileTargetingComponent.h"
#include "RadarComponent.h"
#include "TargetSelectionComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FTargetScreenInfo
	{
		FVector2D WidgetPosition = FVector2D::ZeroVector;
		FVector2D ScreenDirection = FVector2D(0.0f, -1.0f);
		bool bOnScreen = false;
	};

	FVector GetActorCenter(AActor* Actor)
	{
		FVector Center = Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		if (Actor)
		{
			Actor->GetActorBounds(true, Center, Extent);
		}
		return Center;
	}

	FTargetScreenInfo GetTargetScreenInfo(APlayerController* PlayerController,
		AActor* Target, const FVector2D& WidgetViewportSize)
	{
		FTargetScreenInfo Result;
		if (!PlayerController || !IsValid(Target))
		{
			return Result;
		}

		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
		const FVector ToTarget = GetActorCenter(Target) - CameraLocation;
		const FVector CameraForward = CameraRotation.Vector();
		const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
		const FVector CameraUp = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Z);
		Result.ScreenDirection = FVector2D(
			FVector::DotProduct(ToTarget, CameraRight),
			-FVector::DotProduct(ToTarget, CameraUp)).GetSafeNormal();
		if (Result.ScreenDirection.IsNearlyZero())
		{
			Result.ScreenDirection = FVector2D(0.0f,
				FVector::DotProduct(ToTarget, CameraForward) >= 0.0f ? -1.0f : 1.0f);
		}

		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController, GetActorCenter(Target), Result.WidgetPosition, true);
		const bool bInFront = FVector::DotProduct(ToTarget, CameraForward) > 0.0f;
		const float ScreenMargin = 24.0f;
		Result.bOnScreen = bProjected && bInFront
			&& Result.WidgetPosition.X >= ScreenMargin
			&& Result.WidgetPosition.Y >= ScreenMargin
			&& Result.WidgetPosition.X <= WidgetViewportSize.X - ScreenMargin
			&& Result.WidgetPosition.Y <= WidgetViewportSize.Y - ScreenMargin;
		return Result;
	}
}

AArcadeFlightHUD::AArcadeFlightHUD()
{
	FlightHUDWidgetClass = UFlightHUDWidget::StaticClass();
}

void AArcadeFlightHUD::BeginPlay()
{
	Super::BeginPlay();
	if (PlayerOwner && PlayerOwner->IsLocalController() && FlightHUDWidgetClass)
	{
		FlightHUDWidget = CreateWidget<UFlightHUDWidget>(PlayerOwner, FlightHUDWidgetClass);
		if (FlightHUDWidget)
		{
			FlightHUDWidget->AddToPlayerScreen(0);
		}
	}
}

void AArcadeFlightHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!FlightHUDWidget || !PlayerOwner)
	{
		return;
	}

	APawn* OwnerPawn = PlayerOwner->GetPawn();
	UpdateHealthBinding(OwnerPawn);
	const UArcadeFlightComponent* Flight =
		OwnerPawn ? OwnerPawn->FindComponentByClass<UArcadeFlightComponent>() : nullptr;
	const bool bCombatElementsVisible = !Flight || Flight->IsCombatFlightEnabled();
	FlightHUDWidget->SetCombatElementsVisible(bCombatElementsVisible);

	const UMissileTargetingComponent* Targeting =
		OwnerPawn ? OwnerPawn->FindComponentByClass<UMissileTargetingComponent>() : nullptr;
	AActor* LockedTarget = Targeting && Targeting->IsTargetingEnabled()
		? Targeting->GetCurrentTarget() : nullptr;
	const UTargetSelectionComponent* TargetSelection =
		OwnerPawn ? OwnerPawn->FindComponentByClass<UTargetSelectionComponent>() : nullptr;
	AActor* SelectedTarget = TargetSelection ? TargetSelection->GetSelectedTarget() : nullptr;

	const URadarComponent* Radar =
		OwnerPawn ? OwnerPawn->FindComponentByClass<URadarComponent>() : nullptr;
	TArray<FRadarDisplayContact> RadarContacts;
	if (OwnerPawn && Radar && Radar->GetDetectionRange() > KINDA_SMALL_NUMBER)
	{
		const FVector OwnerLocation = OwnerPawn->GetActorLocation();
		FVector FlatForward = OwnerPawn->GetActorForwardVector();
		FlatForward.Z = 0.0f;
		if (!FlatForward.Normalize())
		{
			FlatForward = FRotator(0.0f, OwnerPawn->GetActorRotation().Yaw, 0.0f).Vector();
		}
		const FVector FlatRight = FVector::CrossProduct(FVector::UpVector, FlatForward);
		const float RadarRange = Radar->GetDetectionRange();

		for (const TWeakObjectPtr<AActor>& ContactPtr : Radar->GetDetectedContacts())
		{
			AActor* ContactActor = ContactPtr.Get();
			if (!IsValid(ContactActor))
			{
				continue;
			}

			const FVector Relative = ContactActor->GetActorLocation() - OwnerLocation;
			const FVector FlatRelative(Relative.X, Relative.Y, 0.0f);
			const float HorizontalDistance = FlatRelative.Size();
			const float RadialAlpha = FMath::Clamp(Relative.Size() / RadarRange, 0.0f, 1.0f);
			FRadarDisplayContact& DisplayContact = RadarContacts.AddDefaulted_GetRef();
			if (HorizontalDistance > KINDA_SMALL_NUMBER)
			{
				const FVector HorizontalDirection = FlatRelative / HorizontalDistance;
				DisplayContact.NormalizedPosition.X =
					FVector::DotProduct(HorizontalDirection, FlatRight) * RadialAlpha;
				DisplayContact.NormalizedPosition.Y =
					-FVector::DotProduct(HorizontalDirection, FlatForward) * RadialAlpha;
			}
			DisplayContact.bSelected = ContactActor == SelectedTarget;
			switch (Radar->GetAltitudeRelation(ContactActor))
			{
			case ERadarAltitudeRelation::Above:
				DisplayContact.Icon = ERadarContactIcon::Above;
				break;
			case ERadarAltitudeRelation::Below:
				DisplayContact.Icon = ERadarContactIcon::Below;
				break;
			default:
				DisplayContact.Icon = ERadarContactIcon::Level;
				break;
			}
		}
	}
	FlightHUDWidget->SetRadarContacts(OwnerPawn && Radar, RadarContacts);

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerOwner->GetViewportSize(ViewportSizeX, ViewportSizeY);
	const float ViewportScale = FMath::Max(0.01f,
		UWidgetLayoutLibrary::GetViewportScale(this));
	const FVector2D WidgetViewportSize(
		static_cast<float>(ViewportSizeX) / ViewportScale,
		static_cast<float>(ViewportSizeY) / ViewportScale);

	if (bCombatElementsVisible && IsValid(LockedTarget))
	{
		const FTargetScreenInfo LockedInfo = GetTargetScreenInfo(
			PlayerOwner, LockedTarget, WidgetViewportSize);
		FlightHUDWidget->SetLockMarkerPosition(
			LockedInfo.bOnScreen, LockedInfo.WidgetPosition);
	}
	else
	{
		FlightHUDWidget->SetLockMarkerPosition(false, FVector2D::ZeroVector);
	}

	const bool bSelectedIsRadarContact = IsValid(SelectedTarget)
		&& Radar && Radar->IsDetected(SelectedTarget);
	const FTargetScreenInfo SelectedInfo = bSelectedIsRadarContact
		? GetTargetScreenInfo(PlayerOwner, SelectedTarget, WidgetViewportSize)
		: FTargetScreenInfo();
	FlightHUDWidget->SetSelectedTargetIndicator(
		bSelectedIsRadarContact, SelectedInfo.bOnScreen, SelectedInfo.WidgetPosition,
		SelectedInfo.ScreenDirection, WidgetViewportSize);

	TArray<FVector2D> AttackerDirections;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (auto It = RecentAttackersUntil.CreateIterator(); It; ++It)
	{
		AActor* Attacker = It.Key().Get();
		if (!IsValid(Attacker) || It.Value() <= Now)
		{
			It.RemoveCurrent();
			continue;
		}
		if (Radar && Radar->IsDetected(Attacker))
		{
			const FTargetScreenInfo AttackerInfo = GetTargetScreenInfo(
				PlayerOwner, Attacker, WidgetViewportSize);
			if (!AttackerInfo.bOnScreen)
			{
				AttackerDirections.Add(AttackerInfo.ScreenDirection);
			}
		}
	}
	FlightHUDWidget->SetAttackerDirections(AttackerDirections, WidgetViewportSize);
}

void AArcadeFlightHUD::UpdateHealthBinding(APawn* OwnerPawn)
{
	UHealthComponent* Health = OwnerPawn
		? OwnerPawn->FindComponentByClass<UHealthComponent>() : nullptr;
	if (BoundHealthComponent.Get() == Health)
	{
		return;
	}
	if (UHealthComponent* PreviousHealth = BoundHealthComponent.Get())
	{
		PreviousHealth->OnDamageReceived.RemoveDynamic(
			this, &AArcadeFlightHUD::HandleDamageReceived);
	}
	BoundHealthComponent = Health;
	RecentAttackersUntil.Reset();
	if (Health)
	{
		Health->OnDamageReceived.AddDynamic(this, &AArcadeFlightHUD::HandleDamageReceived);
	}
}

void AArcadeFlightHUD::HandleDamageReceived(AActor* AttackerActor)
{
	if (IsValid(AttackerActor) && GetWorld())
	{
		RecentAttackersUntil.Add(AttackerActor,
			GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, AttackerIndicatorDuration));
	}
}
