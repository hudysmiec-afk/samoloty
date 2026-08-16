#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BackwardDashComponent.generated.h"

class UArcadeFlightComponent;
class UHealthComponent;
class UForwardDashComponent;
class UQuickReversalComponent;
class USoundBase;

USTRUCT()
struct FBackwardDashNetworkState
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
 * Short server-authoritative movement backwards along the aircraft axis.
 * The aircraft keeps facing forward while the skill smoothly accelerates
 * backwards and comes to rest before regular flight resumes.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UBackwardDashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBackwardDashComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void RequestActivation();
	bool TryActivateFromAbilityQueue();

	bool GetAuthoritativeFlightRotation(FQuat& OutRotation) const;
	bool ConsumeAuthoritativeMovement(
		float DeltaTime, float NormalForwardSpeed,
		const FVector& DesiredLateralVelocity,
		FVector& OutVelocity, float& OutSignedForwardSpeed);

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Backward Dash")
	bool IsDashActive() const { return DashState.bActive; }

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Backward Dash")
	float GetCooldownRemaining() const;

	float GetManeuverProgress() const;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestActivation();

	UFUNCTION(Client, Reliable)
	void ClientRejectActivation();

	UFUNCTION()
	void OnRep_DashState();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayBackwardDashSound();

	bool CanActivate() const;
	void StartAuthoritativeDash();
	double GetSynchronizedTime() const;
	double GetLocalTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_DashState)
	FBackwardDashNetworkState DashState;

	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UQuickReversalComponent> CachedQuickReversalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UForwardDashComponent> CachedForwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> CachedHealthComponent;

	/** Spatial one-shot attached to the aircraft when the backward maneuver starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Mobility|Backward Dash|Effects",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> BackwardDashSound;

	double NextAllowedServerTime = 0.0;
	double LocalNextAllowedTime = 0.0;
	bool bLocalActivationPending = false;
	FQuat InitialFlightRotation = FQuat::Identity;
	FVector CurrentLateralVelocity = FVector::ZeroVector;
	float PreviousTravelDistance = 0.0f;
};
