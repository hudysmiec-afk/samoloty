#include "GunAimAssistComponent.h"

#include "HealthComponent.h"
#include "PlaneTargetingUtils.h"
#include "RadarComponent.h"
#include "TargetSelectionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGunAimAssistComponent::UGunAimAssistComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGunAimAssistComponent::SetAimAssistEnabled(const bool bEnabled)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bAimAssistEnabled = bEnabled;
	}
}

bool UGunAimAssistComponent::FindAssistedAimPoint(
	const FVector& AimOrigin, const FVector& AimDirection,
	const float WeaponRange, FVector& OutAimPoint) const
{
	if (!bAimAssistEnabled || !GetWorld() || !GetOwner())
	{
		return false;
	}

	const URadarComponent* Radar = GetOwner()->FindComponentByClass<URadarComponent>();
	const float SensorRange = Radar ? Radar->GetGunTargetingRange() : WeaponRange;
	const float AllowedRange = FMath::Min(
		FMath::Max(0.0f, WeaponRange), FMath::Max(0.0f, SensorRange));
	const FVector Forward = AimDirection.GetSafeNormal();
	if (AllowedRange <= KINDA_SMALL_NUMBER || Forward.IsNearlyZero())
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(AimAssistAngleDegrees, 0.0f, 30.0f)));
	const UTargetSelectionComponent* Selection =
		GetOwner()->FindComponentByClass<UTargetSelectionComponent>();
	AActor* SelectedTarget = Selection
		? Selection->GetSelectedTargetForAimAssist() : nullptr;
	if (!IsValid(SelectedTarget) || SelectedTarget == GetOwner()
		|| SelectedTarget->IsActorBeingDestroyed())
	{
		return false;
	}
	const UHealthComponent* Health =
		SelectedTarget->FindComponentByClass<UHealthComponent>();
	if (!Health || Health->IsDead())
	{
		return false;
	}

	FVector TargetPoint;
	FVector TargetExtent;
	PlaneTargeting::GetTargetBounds(SelectedTarget, TargetPoint, TargetExtent);
	const FVector ToTarget = TargetPoint - AimOrigin;
	const float DistanceSquared = ToTarget.SizeSquared();
	if (DistanceSquared <= KINDA_SMALL_NUMBER
		|| DistanceSquared > FMath::Square(AllowedRange)
		|| FVector::DotProduct(Forward, ToTarget.GetSafeNormal()) < MinimumDot)
	{
		return false;
	}

	OutAimPoint = TargetPoint;
	if (bDrawAimAssistDebug)
	{
		DrawDebugLine(GetWorld(), AimOrigin, TargetPoint, FColor::Green,
			false, 0.1f, 0, 2.0f);
		DrawDebugSphere(GetWorld(), TargetPoint, 75.0f, 12, FColor::Green,
			false, 0.1f, 0, 2.0f);
	}
	return true;
}

void UGunAimAssistComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGunAimAssistComponent, bAimAssistEnabled);
}
