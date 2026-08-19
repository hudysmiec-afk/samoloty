#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForwardDashComponent.generated.h"

class UArcadeFlightComponent;
class UBackwardDashComponent;
class UHealthComponent;
class UQuickReversalComponent;
class USoundBase;
class UCameraShakeBase;

USTRUCT()
struct FForwardDashNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	double ServerStartTime = 0.0;

	UPROPERTY()
	FRotator InitialFlightRotation = FRotator::ZeroRotator;

	UPROPERTY()
	uint16 Sequence = 0;
};

/**
 * Server-authoritative straight-line acceleration skill. The direction is
 * captured on activation and remains fixed for the complete dash.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UForwardDashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForwardDashComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool TryActivateFromAbilityQueue();

	bool GetAuthoritativeFlightRotation(FQuat& OutRotation) const;
	bool ConsumeAuthoritativeMovement(
		float NormalForwardSpeed, FVector& OutVelocity,
		float& OutSignedForwardSpeed);

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Forward Dash")
	bool IsDashActive() const;

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Forward Dash")
	float GetCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Forward Dash")
	float GetManeuverProgress() const;

	float GetMaximumSpeed(float NormalForwardSpeed) const;
	void PlayPredictedActivationEffect();

private:
	UFUNCTION()
	void OnRep_DashState();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayForwardDashSound();

	bool CanActivate() const;
	void StartAuthoritativeDash();
	double GetSynchronizedTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_DashState)
	FForwardDashNetworkState DashState;

	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBackwardDashComponent> CachedBackwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<UQuickReversalComponent> CachedQuickReversalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> CachedHealthComponent;

	/** Spatial one-shot played when the dash begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Forward Dash|Effects",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> ForwardDashSound;

	/** Local screen shake started for the player using the forward dash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Forward Dash|Effects",
		meta=(AllowPrivateAccess="true"))
	TSubclassOf<UCameraShakeBase> ForwardDashCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Forward Dash|Effects",
		meta=(AllowPrivateAccess="true", ClampMin="0", ClampMax="2"))
	float ForwardDashCameraShakeScale = 0.6f;

	double NextAllowedServerTime = 0.0;
	FQuat InitialFlightRotation = FQuat::Identity;
	float InitialForwardSpeed = 0.0f;
};
