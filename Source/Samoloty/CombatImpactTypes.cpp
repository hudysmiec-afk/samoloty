#include "CombatImpactTypes.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

FCombatImpactEvent FCombatImpactEvent::MakeMiss(
	const uint16 InSequence, const uint8 InSubIndex,
	const FVector& EndLocation, const FVector& ShotDirection)
{
	FCombatImpactEvent Result;
	Result.Sequence = InSequence;
	Result.SubIndex = InSubIndex;
	Result.WorldLocation = EndLocation;
	Result.WorldNormal = -ShotDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	return Result;
}

FCombatImpactEvent FCombatImpactEvent::MakeFromHit(
	const uint16 InSequence, const uint8 InSubIndex,
	const FHitResult& Hit, const FVector& FallbackNormal)
{
	FCombatImpactEvent Result;
	Result.Sequence = InSequence;
	Result.SubIndex = InSubIndex;
	Result.bBlockingHit = Hit.bBlockingHit;
	Result.TargetActor = Hit.GetActor();
	Result.WorldLocation = Hit.ImpactPoint;
	const FVector SafeNormal = Hit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FallbackNormal);
	Result.WorldNormal = SafeNormal;

	if (UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		Result.TargetComponentName = HitComponent->GetFName();
		const FTransform ComponentTransform = HitComponent->GetComponentTransform();
		Result.TargetLocalLocation = ComponentTransform.InverseTransformPosition(Hit.ImpactPoint);
		Result.TargetLocalNormal = ComponentTransform
			.InverseTransformVectorNoScale(SafeNormal)
			.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	}
	else if (Result.TargetActor)
	{
		const FTransform ActorTransform = Result.TargetActor->GetActorTransform();
		Result.TargetLocalLocation = ActorTransform.InverseTransformPosition(Hit.ImpactPoint);
		Result.TargetLocalNormal = ActorTransform
			.InverseTransformVectorNoScale(SafeNormal)
			.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	}
	return Result;
}

FCombatImpactEvent FCombatImpactEvent::MakeOnTarget(
	const uint16 InSequence, const uint8 InSubIndex, AActor* Target,
	const FVector& WorldPoint, const FVector& InWorldNormal)
{
	FCombatImpactEvent Result;
	Result.Sequence = InSequence;
	Result.SubIndex = InSubIndex;
	Result.bBlockingHit = Target != nullptr;
	Result.TargetActor = Target;
	Result.WorldLocation = WorldPoint;
	Result.WorldNormal = InWorldNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (Target)
	{
		const FTransform ActorTransform = Target->GetActorTransform();
		Result.TargetLocalLocation = ActorTransform.InverseTransformPosition(WorldPoint);
		Result.TargetLocalNormal = ActorTransform
			.InverseTransformVectorNoScale(Result.WorldNormal)
			.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	}
	return Result;
}

UPrimitiveComponent* FCombatImpactEvent::ResolveTargetComponent() const
{
	if (!IsValid(TargetActor) || TargetComponentName.IsNone())
	{
		return nullptr;
	}
	TInlineComponentArray<UPrimitiveComponent*> Components(TargetActor);
	for (UPrimitiveComponent* Component : Components)
	{
		if (Component && Component->GetFName() == TargetComponentName)
		{
			return Component;
		}
	}
	return nullptr;
}

bool FCombatImpactEvent::Resolve(FVector& OutLocation, FVector& OutNormal) const
{
	OutLocation = WorldLocation;
	OutNormal = FVector(WorldNormal).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (!bBlockingHit || !IsValid(TargetActor))
	{
		return bBlockingHit;
	}

	const UPrimitiveComponent* TargetComponent = ResolveTargetComponent();
	const FTransform TargetTransform = TargetComponent
		? TargetComponent->GetComponentTransform()
		: TargetActor->GetActorTransform();
	OutLocation = TargetTransform.TransformPosition(TargetLocalLocation);
	OutNormal = TargetTransform.TransformVectorNoScale(TargetLocalNormal)
		.GetSafeNormal(UE_SMALL_NUMBER, OutNormal);
	return true;
}

bool FCombatImpactEvent::MatchesPredictedContact(
	const FCombatImpactEvent& Other, const float WorldTolerance) const
{
	if (bBlockingHit != Other.bBlockingHit)
	{
		return false;
	}
	if (!bBlockingHit)
	{
		return true;
	}
	if (TargetActor && Other.TargetActor)
	{
		return TargetActor == Other.TargetActor
			&& (TargetComponentName == Other.TargetComponentName
				|| TargetComponentName.IsNone() || Other.TargetComponentName.IsNone());
	}
	return FVector::DistSquared(WorldLocation, Other.WorldLocation)
		<= FMath::Square(WorldTolerance);
}
