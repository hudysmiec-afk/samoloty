#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlaneAbilityQueueComponent.generated.h"

class UArcadeFlightComponent;
class UBackwardDashComponent;
class UBlinkComponent;
class UEvasiveRollComponent;
class UForwardDashComponent;
class UHealthComponent;
class UQuickReversalComponent;

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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void RequestAbility(EPlaneAbilityType Type, float Direction = 0.0f,
		FVector2D MovementInput = FVector2D::ZeroVector);

	UFUNCTION(BlueprintPure, Category="Plane|Abilities")
	EPlaneAbilityType GetQueuedAbility() const { return QueuedRequest.Type; }

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestAbility(EPlaneAbilityType Type, float Direction,
		FVector2D MovementInput, uint16 Sequence);

	void HandleAuthoritativeRequest(const FPlaneAbilityRequest& Request);
	bool HasBlockingAbilityFor(EPlaneAbilityType RequestedType) const;
	bool TryExecuteAbility(const FPlaneAbilityRequest& Request);
	void ClearQueue();

	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBackwardDashComponent> CachedBackwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBlinkComponent> CachedBlinkComponent;

	UPROPERTY(Transient)
	TObjectPtr<UForwardDashComponent> CachedForwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<UEvasiveRollComponent> CachedEvasiveRollComponent;

	UPROPERTY(Transient)
	TObjectPtr<UQuickReversalComponent> CachedQuickReversalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> CachedHealthComponent;

	FPlaneAbilityRequest QueuedRequest;
	uint16 LocalRequestSequence = 0;
	double QueueReadySince = 0.0;
};
