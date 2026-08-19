#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlinkComponent.generated.h"

class UArcadeFlightComponent;
class UHealthComponent;
class UNiagaraSystem;
class USoundBase;

USTRUCT()
struct FBlinkNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	double ServerActivationTime = -1000.0;

	UPROPERTY()
	uint16 Sequence = 0;
};

/** Server-authoritative directional teleport with collision-safe placement. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UBlinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlinkComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool TryActivateFromAbilityQueue(const FVector2D& DirectionInput);

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Blink")
	float GetCooldownRemaining() const;

	/** Short recovery window used only to let the local camera catch the teleported plane. */
	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Blink")
	bool IsCameraRecovering() const;
	void PlayPredictedActivationEffect(
		const FVector& DepartureLocation, const FVector& ArrivalLocation);

private:
	bool CanActivate() const;
	void StartAuthoritativeBlink(const FVector2D& DirectionInput);
	double GetSynchronizedTime() const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayBlinkEffects(
		FVector_NetQuantize DepartureLocation, FVector_NetQuantize ArrivalLocation);

	UPROPERTY(Replicated)
	FBlinkNetworkState BlinkState;

	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> CachedHealthComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Blink|Effects",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNiagaraSystem> BlinkDepartureEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Blink|Effects",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNiagaraSystem> BlinkArrivalEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Blink|Effects",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> BlinkSound;

	double NextAllowedServerTime = 0.0;
};
