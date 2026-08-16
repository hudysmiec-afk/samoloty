#include "GroundBarrageWeaponComponent.h"

#include "JetStatsComponent.h"
#include "RocketLaunchPattern.h"
#include "RocketProjectile.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UGroundBarrageWeaponComponent::UGroundBarrageWeaponComponent()
{
	SetWeaponDisplayName(TEXT("Ground Barrage"));
}

void UGroundBarrageWeaponComponent::BeginPlay()
{
	// A newly added native component cannot inherit per-instance Blueprint values
	// from its sibling automatically. Reuse the ordinary barrage projectile and
	// launch sound unless this variant has been configured explicitly.
	if (AActor* Owner = GetOwner())
	{
		TInlineComponentArray<URocketWeaponComponent*> BarrageComponents(Owner);
		for (const URocketWeaponComponent* Barrage : BarrageComponents)
		{
			if (Barrage && Barrage != this
				&& Barrage->GetClass() == URocketWeaponComponent::StaticClass())
			{
				InheritMissingPresentationFrom(*Barrage);
				break;
			}
		}
	}
	Super::BeginPlay();
}

void UGroundBarrageWeaponComponent::PrepareSalvo(const FRocketBarrageStats& Stats)
{
	if (!CalculateGroundAimPoint(Stats.MaxTravelDistance, CachedGroundAimPoint))
	{
		const AActor* Owner = GetOwner();
		CachedGroundAimPoint = Owner
			? Owner->GetActorLocation() + Owner->GetActorForwardVector()
				* (Stats.MaxTravelDistance * 0.5f)
				- FVector::UpVector * (Stats.MaxTravelDistance * 0.5f)
			: FVector::ZeroVector;
	}

	if (bDrawRocketDebug && GetWorld())
	{
		DrawDebugSphere(GetWorld(), CachedGroundAimPoint,
			FMath::Max(50.0f, ImpactSpreadRadius), 24, FColor::Orange, false, 2.0f, 0, 5.0f);
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), CachedGroundAimPoint,
			FColor::Orange, false, 2.0f, 0, 2.0f);
	}
}

void UGroundBarrageWeaponComponent::ConfigureRocketLaunchData(FRocketLaunchData& LaunchData,
	const FRocketSeparationPath& SeparationPath, FRandomStream& RandomStream) const
{
	const float Angle = RandomStream.FRandRange(0.0f, 2.0f * UE_PI);
	const float Radius = FMath::Sqrt(RandomStream.FRand()) * FMath::Max(0.0f, ImpactSpreadRadius);
	const FVector GroundOffset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
	const FVector ImpactPoint = CachedGroundAimPoint + GroundOffset;
	const FVector GroundDirection = (ImpactPoint - SeparationPath.EndPoint).GetSafeNormal(
		SMALL_NUMBER, -FVector::UpVector);

	LaunchData.Direction = GroundDirection;
	// For this weapon the stat is the ground-designation range, matching the
	// reference behavior. Ensure the curved launch path can still reach that point.
	const float RequiredPathDistance =
		FVector::Distance(SeparationPath.StartLocation, SeparationPath.EndPoint)
		+ FVector::Distance(SeparationPath.EndPoint, ImpactPoint) + 100.0f;
	LaunchData.MaxTravelDistance = FMath::Max(
		LaunchData.MaxTravelDistance, RequiredPathDistance);
	// Preserve the curved separation, but align its final tangent with the descent.
	// This avoids a visible corner when the projectile changes from curve to straight flight.
	const float FinalHandleLength = FVector::Distance(
		SeparationPath.ControlPoint2, SeparationPath.EndPoint);
	LaunchData.SeparationControlPoint2 = SeparationPath.EndPoint
		- GroundDirection * FinalHandleLength;

	if (bDrawRocketDebug && GetWorld())
	{
		DrawDebugPoint(GetWorld(), ImpactPoint, 12.0f, FColor::Red, false, 2.0f);
	}
}

bool UGroundBarrageWeaponComponent::GetPredictedImpactPoint(FVector& OutImpactPoint) const
{
	const UJetStatsComponent* StatsComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	if (!StatsComponent)
	{
		return false;
	}
	const FRocketBarrageStats& Stats = StatsComponent->GetRocketBarrageStats();
	return CalculateGroundAimPoint(Stats.MaxTravelDistance, OutImpactPoint);
}

bool UGroundBarrageWeaponComponent::CalculateGroundAimPoint(
	const float WeaponRange, FVector& OutImpactPoint) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return false;
	}

	const FVector Start = Owner->GetActorLocation();
	const float SafeWeaponRange = FMath::Max(1000.0f, WeaponRange);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GroundBarrageAim), false, Owner);
	QueryParams.AddIgnoredActor(Owner);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);

	// The reference weapon ignores vertical pitch and visual/strafe velocity.
	// Only the aircraft's horizontal forward heading chooses left/right direction.
	const FVector TravelDirection = FVector::VectorPlaneProject(
		Owner->GetActorForwardVector(), FVector::UpVector).GetSafeNormal(SMALL_NUMBER,
		FRotator(0.0f, Owner->GetActorRotation().Yaw, 0.0f).Vector());
	const FVector DesiredTarget = Start
		+ TravelDirection * SafeWeaponRange
		- FVector::UpVector * (SafeWeaponRange * 0.5f);

	// One diagonal trace finds whichever blocks the bombing line first: terrain
	// or a static object. This is the same decision model as the reference weapon.
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByObjectType(
		GroundHit, Start, DesiredTarget, ObjectQuery, QueryParams))
	{
		OutImpactPoint = GroundHit.ImpactPoint;
		return true;
	}
	OutImpactPoint = DesiredTarget;
	return true;
}
