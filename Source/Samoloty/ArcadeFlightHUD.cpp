#include "ArcadeFlightHUD.h"

#include "ArcadeFlightComponent.h"
#include "BackwardDashComponent.h"
#include "BlinkComponent.h"
#include "EvasiveRollComponent.h"
#include "FlightHUDWidget.h"
#include "ForwardDashComponent.h"
#include "GroundBarrageWeaponComponent.h"
#include "HealthComponent.h"
#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "MissileTargetingComponent.h"
#include "PlaneTargetingUtils.h"
#include "PlaneWeaponComponent.h"
#include "RadarComponent.h"
#include "QuickReversalComponent.h"
#include "SamolotyGameUserSettings.h"
#include "TargetSelectionComponent.h"
#include "TargetIntentComponent.h"
#include "WeaponSystemComponent.h"
#include "WorldNameComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameUserSettings.h"
#include "UObject/ConstructorHelpers.h"

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
		FVector Center;
		FVector Extent;
		PlaneTargeting::GetTargetBounds(Actor, Center, Extent);
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
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackerOnScreenFinder(
		TEXT("/Game/Main/Images/HUD/IMG_AttackerOnScreen.IMG_AttackerOnScreen"));
	AttackerOnScreenTexture = AttackerOnScreenFinder.Object;
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
	const bool bCombatElementsVisible = !Flight || Flight->AreCombatElementsVisible();
	FlightHUDWidget->SetCombatElementsVisible(bCombatElementsVisible);
	FlightHUDWidget->SetBoundaryWarning(
		Flight ? Flight->GetBoundaryWarningAlpha() : 0.0f,
		Flight && Flight->IsBoundaryReturnActive());
	const UHealthComponent* OwnerHealth = OwnerPawn
		? OwnerPawn->FindComponentByClass<UHealthComponent>() : nullptr;
	const UJetBoostComponent* Boost = OwnerPawn
		? OwnerPawn->FindComponentByClass<UJetBoostComponent>() : nullptr;
	const UJetStatsComponent* Stats = OwnerPawn
		? OwnerPawn->FindComponentByClass<UJetStatsComponent>() : nullptr;
	const UWeaponSystemComponent* PlayerWeapons = OwnerPawn
		? OwnerPawn->FindComponentByClass<UWeaponSystemComponent>() : nullptr;

	const float FrameDeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (FrameDeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const float InstantFPS = 1.0f / FrameDeltaSeconds;
		SmoothedFPS = SmoothedFPS <= 0.0f ? InstantFPS
			: FMath::FInterpTo(SmoothedFPS, InstantFPS, FrameDeltaSeconds, 3.0f);
	}
	const double PerformanceNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (PerformanceNow >= NextPerformanceStatsUpdateTime)
	{
		NextPerformanceStatsUpdateTime = PerformanceNow + 0.25;
		const USamolotyGameUserSettings* ProjectSettings = GEngine
			? Cast<USamolotyGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
		const APlayerState* LocalPlayerState = PlayerOwner->PlayerState;
		FlightHUDWidget->SetPerformanceStats(
			ProjectSettings && ProjectSettings->ShouldShowFPS(), SmoothedFPS,
			ProjectSettings && ProjectSettings->ShouldShowPing(),
			LocalPlayerState ? LocalPlayerState->GetPingInMilliseconds() : 0.0f);
	}
	if (PerformanceNow >= NextAbilityCooldownUpdateTime)
	{
		NextAbilityCooldownUpdateTime = PerformanceNow + 0.1;
		const UQuickReversalComponent* QuickReversal = OwnerPawn
			? OwnerPawn->FindComponentByClass<UQuickReversalComponent>() : nullptr;
		const UBackwardDashComponent* BackwardDash = OwnerPawn
			? OwnerPawn->FindComponentByClass<UBackwardDashComponent>() : nullptr;
		const UForwardDashComponent* ForwardDash = OwnerPawn
			? OwnerPawn->FindComponentByClass<UForwardDashComponent>() : nullptr;
		const UBlinkComponent* Blink = OwnerPawn
			? OwnerPawn->FindComponentByClass<UBlinkComponent>() : nullptr;
		const UEvasiveRollComponent* EvasiveRoll = OwnerPawn
			? OwnerPawn->FindComponentByClass<UEvasiveRollComponent>() : nullptr;
		FlightHUDWidget->SetAbilityCooldowns(
			QuickReversal ? QuickReversal->GetCooldownRemaining() : 0.0f,
			BackwardDash ? BackwardDash->GetCooldownRemaining() : 0.0f,
			ForwardDash ? ForwardDash->GetCooldownRemaining() : 0.0f,
			Blink ? Blink->GetCooldownRemaining() : 0.0f,
			EvasiveRoll ? EvasiveRoll->GetCooldownRemaining() : 0.0f);
	}
	UPlaneWeaponComponent* ActiveGun = PlayerWeapons
		? PlayerWeapons->GetActiveWeapon(EWeaponSlot::Gun) : nullptr;
	UPlaneWeaponComponent* ActiveMissile = PlayerWeapons
		? PlayerWeapons->GetActiveWeapon(EWeaponSlot::Missile) : nullptr;
	float GunRemaining = 0.0f;
	float GunDuration = 0.0f;
	float MissileRemaining = 0.0f;
	float MissileDuration = 0.0f;
	if (ActiveGun)
	{
		ActiveGun->GetCooldownStatus(GunRemaining, GunDuration);
	}
	if (ActiveMissile)
	{
		ActiveMissile->GetCooldownStatus(MissileRemaining, MissileDuration);
	}
	const float MaximumBoost = Stats ? Stats->GetFlightStats().MaxBoostEnergy : 0.0f;
	FlightHUDWidget->SetPlayerStatus(
		OwnerHealth ? OwnerHealth->GetCurrentHealth() : 0.0f,
		OwnerHealth ? OwnerHealth->GetMaxHealth() : 0.0f,
		Boost ? Boost->GetCurrentEnergy() : 0.0f, MaximumBoost,
		ActiveGun ? ActiveGun->GetWeaponDisplayName() : FName(TEXT("Gun")),
		GunRemaining, GunDuration,
		ActiveMissile ? ActiveMissile->GetWeaponDisplayName() : FName(TEXT("Missile")),
		MissileRemaining, MissileDuration,
		Flight ? Flight->GetCurrentVelocity().Size()
			: OwnerPawn ? OwnerPawn->GetVelocity().Size() : 0.0f);
	DrawGroundBarrageMarker(OwnerPawn, bCombatElementsVisible);
	DrawWorldNameplates(OwnerPawn);

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
	const float SelectedDistance = bSelectedIsRadarContact && OwnerPawn
		? FVector::Distance(OwnerPawn->GetActorLocation(), SelectedTarget->GetActorLocation())
		: 0.0f;
	FlightHUDWidget->SetSelectedTargetIndicator(
		bSelectedIsRadarContact, SelectedInfo.bOnScreen, SelectedInfo.WidgetPosition,
		SelectedInfo.ScreenDirection, WidgetViewportSize, SelectedDistance);

	TArray<FVector2D> AttackerDirections;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (const TWeakObjectPtr<APawn>& ThreatPawnPtr : CachedNameplateActors)
	{
		APawn* Attacker = ThreatPawnPtr.Get();
		const UTargetIntentComponent* TargetIntent = IsValid(Attacker)
			? Attacker->FindComponentByClass<UTargetIntentComponent>() : nullptr;
		if (!TargetIntent || !TargetIntent->IsTargeting(OwnerPawn))
		{
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

	TArray<FDamageNumberDisplay> DamageNumberDisplays;
	for (int32 Index = ActiveDamageNumbers.Num() - 1; Index >= 0; --Index)
	{
		FActiveDamageNumber& Entry = ActiveDamageNumbers[Index];
		const float Age = static_cast<float>(Now - Entry.StartTime);
		if (Age >= DamageNumberDuration)
		{
			ActiveDamageNumbers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		if (AActor* Target = Entry.Target.Get())
		{
			Entry.LastWorldLocation = GetActorCenter(Target);
		}
		FVector2D WidgetPosition;
		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerOwner, Entry.LastWorldLocation, WidgetPosition, true);
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerOwner->GetPlayerViewPoint(CameraLocation, CameraRotation);
		const bool bInFront = FVector::DotProduct(
			Entry.LastWorldLocation - CameraLocation, CameraRotation.Vector()) > 0.0f;
		if (!bProjected || !bInFront)
		{
			continue;
		}

		const float LifetimeAlpha = FMath::Clamp(
			Age / FMath::Max(0.1f, DamageNumberDuration), 0.0f, 1.0f);
		FDamageNumberDisplay& Display = DamageNumberDisplays.AddDefaulted_GetRef();
		Display.Position = WidgetPosition + FVector2D(
			Entry.HorizontalOffset, -36.0f - 58.0f * LifetimeAlpha);
		Display.Text = FString::Printf(TEXT("%.0f"), Entry.Damage);
		Display.Opacity = 1.0f - FMath::SmoothStep(0.58f, 1.0f, LifetimeAlpha);
		Display.Scale = FMath::Lerp(
			1.25f, 1.0f, FMath::SmoothStep(0.0f, 0.18f, LifetimeAlpha));
	}
	FlightHUDWidget->SetDamageNumbers(DamageNumberDisplays);
}

void AArcadeFlightHUD::RefreshNameplateActors(APawn* OwnerPawn)
{
	CachedNameplateActors.Reset();
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn == OwnerPawn)
		{
			continue;
		}
		const UWorldNameComponent* WorldName =
			Pawn->FindComponentByClass<UWorldNameComponent>();
		if (Pawn->GetPlayerState() || (WorldName && WorldName->ShouldShowNameplate()))
		{
			CachedNameplateActors.Add(Pawn);
		}
	}
}

void AArcadeFlightHUD::DrawWorldNameplates(APawn* OwnerPawn)
{
	if (!OwnerPawn || !PlayerOwner || !Canvas || !GetWorld() || !GEngine)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextNameplateActorRefreshTime)
	{
		RefreshNameplateActors(OwnerPawn);
		NextNameplateActorRefreshTime = Now
			+ FMath::Max(0.05f, NameplateActorRefreshInterval);
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerOwner->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const float MaximumDistanceSquared = FMath::Square(NameplateMaxDistance);
	for (int32 Index = CachedNameplateActors.Num() - 1; Index >= 0; --Index)
	{
		APawn* Pawn = CachedNameplateActors[Index].Get();
		if (!IsValid(Pawn))
		{
			CachedNameplateActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (const UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
			Health && Health->IsDead())
		{
			continue;
		}

		FString Name;
		FLinearColor NameColor = PlayerNameColor;
		if (const APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			Name = PlayerState->GetPlayerName();
			if (Name.IsEmpty())
			{
				Name = FString::Printf(TEXT("Player %d"), PlayerState->GetPlayerId());
			}
		}
		else if (const UWorldNameComponent* WorldName =
			Pawn->FindComponentByClass<UWorldNameComponent>();
			WorldName && WorldName->ShouldShowNameplate())
		{
			Name = WorldName->GetDisplayName().ToString();
			NameColor = WorldName->GetNameColor();
		}
		if (Name.IsEmpty())
		{
			continue;
		}

		FVector BoundsCenter;
		FVector BoundsExtent;
		PlaneTargeting::GetTargetBounds(Pawn, BoundsCenter, BoundsExtent);
		const FVector NameWorldLocation = BoundsCenter
			+ FVector::UpVector * (BoundsExtent.Z + NameplateHeightOffset);
		const FVector ToName = NameWorldLocation - CameraLocation;
		const float DistanceSquared = ToName.SizeSquared();
		if (DistanceSquared > MaximumDistanceSquared
			|| FVector::DotProduct(ToName, CameraRotation.Vector()) <= 0.0f)
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerOwner->ProjectWorldLocationToScreen(
			NameWorldLocation, ScreenPosition, true)
			|| ScreenPosition.X < 0.0f || ScreenPosition.Y < 0.0f
			|| ScreenPosition.X > Canvas->ClipX || ScreenPosition.Y > Canvas->ClipY)
		{
			continue;
		}

		const float DistanceAlpha = FMath::Clamp(
			FMath::Sqrt(DistanceSquared) / FMath::Max(1.0f, NameplateMaxDistance),
			0.0f, 1.0f);
		FCanvasTextItem TextItem(ScreenPosition, FText::FromString(Name),
			GEngine->GetSmallFont(), NameColor);
		TextItem.bCentreX = true;
		TextItem.bCentreY = true;
		TextItem.bOutlined = false;
		TextItem.EnableShadow(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), FVector2D(1.0f, 1.0f));
		const float TextScale = FMath::Lerp(1.05f, 0.72f, DistanceAlpha);
		TextItem.Scale = FVector2D(TextScale);
		Canvas->DrawItem(TextItem);

		const UTargetIntentComponent* TargetIntent =
			Pawn->FindComponentByClass<UTargetIntentComponent>();
		if (AttackerOnScreenTexture && AttackerOnScreenTexture->GetResource()
			&& TargetIntent && TargetIntent->IsTargeting(OwnerPawn))
		{
			float UnscaledTextWidth = 0.0f;
			float UnscaledTextHeight = 0.0f;
			Canvas->StrLen(GEngine->GetSmallFont(), Name,
				UnscaledTextWidth, UnscaledTextHeight);
			const float TextHalfWidth = UnscaledTextWidth * TextScale * 0.5f;
			const float IconSize = AttackerOnScreenSize * TextScale;
			const float IconHalfSize = IconSize * 0.5f;
			const float Gap = AttackerOnScreenGap * TextScale;
			const FVector2D IconDimensions(IconSize, IconSize);

			FCanvasTileItem LeftIcon(
				FVector2D(ScreenPosition.X - TextHalfWidth - Gap - IconSize,
					ScreenPosition.Y - IconHalfSize),
				AttackerOnScreenTexture->GetResource(), IconDimensions,
				AttackerOnScreenColor);
			LeftIcon.BlendMode = SE_BLEND_Translucent;
			LeftIcon.PivotPoint = FVector2D(0.5f, 0.5f);
			LeftIcon.Rotation = FRotator(0.0f, 90.0f, 0.0f);
			Canvas->DrawItem(LeftIcon);

			FCanvasTileItem RightIcon(
				FVector2D(ScreenPosition.X + TextHalfWidth + Gap,
					ScreenPosition.Y - IconHalfSize),
				AttackerOnScreenTexture->GetResource(), IconDimensions,
				AttackerOnScreenColor);
			RightIcon.BlendMode = SE_BLEND_Translucent;
			RightIcon.PivotPoint = FVector2D(0.5f, 0.5f);
			RightIcon.Rotation = FRotator(0.0f, -90.0f, 0.0f);
			Canvas->DrawItem(RightIcon);
		}
	}
}

void AArcadeFlightHUD::DrawGroundBarrageMarker(APawn* OwnerPawn,
	const bool bCombatElementsVisible)
{
	if (!bCombatElementsVisible || !OwnerPawn || !PlayerOwner || !Canvas)
	{
		return;
	}
	const UWeaponSystemComponent* WeaponSystem =
		OwnerPawn->FindComponentByClass<UWeaponSystemComponent>();
	const UGroundBarrageWeaponComponent* GroundBarrage = WeaponSystem
		? Cast<UGroundBarrageWeaponComponent>(
			WeaponSystem->GetActiveWeapon(EWeaponSlot::Missile)) : nullptr;
	if (!GroundBarrage)
	{
		return;
	}

	FVector ImpactPoint;
	if (!GroundBarrage->GetPredictedImpactPoint(ImpactPoint))
	{
		return;
	}
	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerOwner->GetPlayerViewPoint(CameraLocation, CameraRotation);
	if (FVector::DotProduct(ImpactPoint - CameraLocation, CameraRotation.Vector()) <= 0.0f)
	{
		return;
	}

	// Project the points of a horizontal world-space ring separately. Unlike a
	// screen-space circle, this becomes an ellipse in perspective and visibly lies
	// on the terrain as the camera moves.
	const FVector MarkerCenter = ImpactPoint + FVector::UpVector * 20.0f;
	const float WorldRadius = FMath::Max(100.0f, GroundBarrage->GetImpactSpreadRadius());
	const FLinearColor MarkerColor(1.0f, 0.42f, 0.03f, 0.95f);
	constexpr int32 SegmentCount = 32;
	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const float AngleA = 2.0f * UE_PI * static_cast<float>(Segment) / SegmentCount;
		const float AngleB = 2.0f * UE_PI * static_cast<float>(Segment + 1) / SegmentCount;
		const FVector WorldPointA = MarkerCenter
			+ FVector(FMath::Cos(AngleA), FMath::Sin(AngleA), 0.0f) * WorldRadius;
		const FVector WorldPointB = MarkerCenter
			+ FVector(FMath::Cos(AngleB), FMath::Sin(AngleB), 0.0f) * WorldRadius;
		FVector2D PointA;
		FVector2D PointB;
		if (PlayerOwner->ProjectWorldLocationToScreen(WorldPointA, PointA, true)
			&& PlayerOwner->ProjectWorldLocationToScreen(WorldPointB, PointB, true))
		{
			DrawLine(PointA.X, PointA.Y, PointB.X, PointB.Y, MarkerColor, 2.5f);
		}
	}

	auto DrawWorldLine = [this, &MarkerColor](const FVector& Start, const FVector& End)
	{
		FVector2D ScreenStart;
		FVector2D ScreenEnd;
		if (PlayerOwner->ProjectWorldLocationToScreen(Start, ScreenStart, true)
			&& PlayerOwner->ProjectWorldLocationToScreen(End, ScreenEnd, true))
		{
			DrawLine(ScreenStart.X, ScreenStart.Y, ScreenEnd.X, ScreenEnd.Y,
				MarkerColor, 2.5f);
		}
	};
	const float CrossHalfSize = WorldRadius * 0.42f;
	DrawWorldLine(MarkerCenter - FVector::ForwardVector * CrossHalfSize,
		MarkerCenter + FVector::ForwardVector * CrossHalfSize);
	DrawWorldLine(MarkerCenter - FVector::RightVector * CrossHalfSize,
		MarkerCenter + FVector::RightVector * CrossHalfSize);
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
		PreviousHealth->OnDamageDealt.RemoveDynamic(
			this, &AArcadeFlightHUD::HandleDamageDealt);
	}
	BoundHealthComponent = Health;
	RecentAttackersUntil.Reset();
	ActiveDamageNumbers.Reset();
	if (Health)
	{
		Health->OnDamageReceived.AddDynamic(this, &AArcadeFlightHUD::HandleDamageReceived);
		Health->OnDamageDealt.AddDynamic(this, &AArcadeFlightHUD::HandleDamageDealt);
	}
}

void AArcadeFlightHUD::HandleDamageDealt(
	AActor* DamagedActor, const float Damage, const FVector WorldLocation)
{
	if (Damage <= 0.0f || !GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = ActiveDamageNumbers.Num() - 1; Index >= 0; --Index)
	{
		FActiveDamageNumber& Entry = ActiveDamageNumbers[Index];
		if (Entry.Target.Get() == DamagedActor
			&& Now - Entry.StartTime <= DamageNumberMergeWindow)
		{
			Entry.Damage += Damage;
			Entry.LastWorldLocation = WorldLocation;
			return;
		}
	}

	if (ActiveDamageNumbers.Num() >= 32)
	{
		ActiveDamageNumbers.RemoveAt(0, 1, EAllowShrinking::No);
	}
	FActiveDamageNumber& Entry = ActiveDamageNumbers.AddDefaulted_GetRef();
	Entry.Target = DamagedActor;
	Entry.LastWorldLocation = WorldLocation;
	Entry.Damage = Damage;
	Entry.StartTime = Now;
	Entry.HorizontalOffset = FMath::FRandRange(-18.0f, 18.0f);
}

void AArcadeFlightHUD::HandleDamageReceived(AActor* AttackerActor)
{
	if (IsValid(AttackerActor) && GetWorld())
	{
		RecentAttackersUntil.Add(AttackerActor,
			GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, AttackerIndicatorDuration));
	}
}
