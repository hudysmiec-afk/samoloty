#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightBoundsVolume.generated.h"

class UBoxComponent;

/** Soft flight boundary: warns before the edge and guides aircraft back after crossing it. */
UCLASS(Blueprintable)
class SAMOLOTY_API AFlightBoundsVolume : public AActor
{
	GENERATED_BODY()

public:
	AFlightBoundsVolume();

	/** Returns proximity to the boundary (0..1) and whether the point is outside. */
	void GetBoundaryInfo(const FVector& WorldLocation, float& OutWarningAlpha,
		bool& bOutOutside) const;

	/** True only after the point has moved ReleaseInset inside every enabled face. */
	bool IsSafelyInside(const FVector& WorldLocation) const;

	/** A world-space target beyond the crossed face, used to produce a broad return turn. */
	FVector GetReturnTarget(const FVector& WorldLocation) const;

	float GetReturnTurnRate() const { return ReturnTurnRate; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Flight Boundary")
	TObjectPtr<UBoxComponent> BoundsBox;

	/** Distance before the edge at which the HUD warning begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flight Boundary",
		meta=(ClampMin="0", Units="cm"))
	float WarningDistance = 50000.0f;

	/** Distance inside the edge required before player control is restored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flight Boundary",
		meta=(ClampMin="0", Units="cm"))
	float ReleaseInset = 15000.0f;

	/** How far behind a crossed face the automatic flight target is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flight Boundary",
		meta=(ClampMin="100", Units="cm"))
	float ReturnTargetDepth = 30000.0f;

	/** Maximum automatic rotation speed. Lower values create a wider turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flight Boundary",
		meta=(ClampMin="5", ClampMax="180", Units="deg/s"))
	float ReturnTurnRate = 65.0f;

	/** If disabled, only horizontal X/Y map edges are enforced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flight Boundary")
	bool bLimitAltitude = false;

private:
	FVector GetLocalExtent() const;
	FVector GetAxisWorldScale() const;
};
