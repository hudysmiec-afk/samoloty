#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlaneAbilityQueueComponent.generated.h"

class UArcadeFlightComponent;

UENUM(BlueprintType)
enum class EPlaneAbilityType : uint8
{
	None,
	EvasiveRollLeft,
	EvasiveRollRight,
	QuickReversal,
	BackwardDash,
	ForwardDash,
	Blink
};

USTRUCT()
struct FPlaneAbilityRequest
{
	GENERATED_BODY()

	UPROPERTY()
	EPlaneAbilityType Type = EPlaneAbilityType::None;

	UPROPERTY()
	float Direction = 0.0f;

	UPROPERTY()
	FVector2D MovementInput = FVector2D::ZeroVector;

	UPROPERTY()
	uint16 Sequence = 0;
};

/**
 * Lightweight server-authoritative queue for discrete aircraft abilities.
 * One active maneuver may buffer one follow-up request; a newer request replaces
 * the older queued request.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UPlaneAbilityQueueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlaneAbilityQueueComponent();
	virtual void BeginPlay() override;
	void RequestAbility(EPlaneAbilityType Type, float Direction = 0.0f,
		FVector2D MovementInput = FVector2D::ZeroVector);

	UFUNCTION(BlueprintPure, Category="Plane|Abilities")
	EPlaneAbilityType GetQueuedAbility() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;
};
