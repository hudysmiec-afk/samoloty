#pragma once

#include "CoreMinimal.h"
#include "CombatImpactTypes.generated.h"

class AActor;
class UPrimitiveComponent;

/**
 * Network-safe semantic contact. Static surfaces use WorldLocation. Contacts
 * with moving actors additionally store a point and normal in the hit
 * component's local space, so late cosmetics can be rebuilt on the currently
 * presented target instead of appearing at a stale server world position.
 */
USTRUCT()
struct SAMOLOTY_API FCombatImpactEvent
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Sequence = 0;

	UPROPERTY()
	uint8 SubIndex = 0;

	UPROPERTY()
	bool bBlockingHit = false;

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	FName TargetComponentName = NAME_None;

	UPROPERTY()
	FVector_NetQuantize100 WorldLocation;

	UPROPERTY()
	FVector_NetQuantizeNormal WorldNormal = FVector::UpVector;

	UPROPERTY()
	FVector_NetQuantize100 TargetLocalLocation;

	UPROPERTY()
	FVector_NetQuantizeNormal TargetLocalNormal = FVector::UpVector;

	static FCombatImpactEvent MakeMiss(uint16 InSequence, uint8 InSubIndex,
		const FVector& EndLocation, const FVector& ShotDirection);
	static FCombatImpactEvent MakeFromHit(uint16 InSequence, uint8 InSubIndex,
		const FHitResult& Hit, const FVector& FallbackNormal = FVector::UpVector);
	static FCombatImpactEvent MakeOnTarget(uint16 InSequence, uint8 InSubIndex,
		AActor* Target, const FVector& WorldPoint, const FVector& WorldNormal);

	/** Resolves the contact against the target's current presented component transform. */
	bool Resolve(FVector& OutLocation, FVector& OutNormal) const;

	/** Contact identity used to suppress a server confirmation already predicted locally. */
	bool MatchesPredictedContact(const FCombatImpactEvent& Other,
		float WorldTolerance = 250.0f) const;

private:
	UPrimitiveComponent* ResolveTargetComponent() const;
};
