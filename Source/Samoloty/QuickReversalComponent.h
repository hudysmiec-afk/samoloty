#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuickReversalComponent.generated.h"

class UArcadeFlightComponent;
class UEvasiveRollComponent;
class UHealthComponent;

UENUM(BlueprintType)
enum class EQuickReversalDirection : uint8
{
	Left,
	Right
};

USTRUCT()
struct FQuickReversalNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	EQuickReversalDirection Direction = EQuickReversalDirection::Right;

	UPROPERTY()
	double ServerStartTime = 0.0;

	/** Full 3D orientation captured on the server at activation. */
	UPROPERTY()
	FRotator InitialFlightRotation = FRotator::ZeroRotator;

	UPROPERTY()
	uint16 Sequence = 0;
};

/**
 * Server-authoritative mobility skill that performs a fast horizontal reversal.
 * Normal steering and lateral movement remain available while the maneuver
 * performs the reversal. Weapon fire stays blocked until the maneuver camera
 * has completed its turn.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UQuickReversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuickReversalComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Requests the skill. Negative direction turns left; zero or positive turns right. */
	void RequestActivation(float PreferredDirection);

	/** Absolute 3D flight-root rotation for the active maneuver. */
	bool GetAuthoritativeFlightRotation(FQuat& OutRotation) const;

	/**
	 * Builds the server-authoritative inertial velocity for the maneuver. Returns
	 * false outside the active maneuver so normal flight can resume.
	 */
	bool ConsumeAuthoritativeMovement(
		float DeltaTime, float NormalSpeed, float BoostedSpeed,
		const FVector& DesiredLateralVelocity,
		FVector& OutVelocity, float& OutSignedForwardSpeed);

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Quick Reversal")
	bool IsReversalActive() const { return ReversalState.bActive; }

	/** True from the local activation request until the maneuver camera finishes turning. */
	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Quick Reversal")
	bool IsWeaponFireBlocked() const
	{
		return ReversalState.bActive || bLocalActivationPending;
	}

	UFUNCTION(BlueprintPure, Category="Plane|Mobility|Quick Reversal")
	float GetCooldownRemaining() const;

	/** Synchronized zero-to-one progress used by local presentation and camera paths. */
	float GetManeuverProgress() const;

	/** Returns -1 for a left reversal and +1 for a right reversal. */
	float GetDirectionSign() const;

	/** Local visual rotation that performs the nose-down half-loop in full 3D. */
	FQuat GetPresentationRelativeRotation(const FQuat& FlightRootRotation) const;

	/** Local up axis captured when the maneuver starts, used by its camera path. */
	FVector GetManeuverUpAxis() const;

	/** Zero-to-one engine cut used by audio and visual presentation. */
	float GetEngineCutAlpha() const;

	/** Visual-only afterburner alpha. It never consumes normal boost energy. */
	float GetSkillBoostAlpha() const;

	/** Restores regular turn/strafe bank after the nose-down part is complete. */
	float GetFlightBankBlendAlpha() const;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestActivation(EQuickReversalDirection Direction);

	UFUNCTION(Client, Reliable)
	void ClientRejectActivation();

	UFUNCTION()
	void OnRep_ReversalState();

	bool CanActivate() const;
	void StartAuthoritativeReversal(EQuickReversalDirection Direction);
	double GetSynchronizedTime() const;
	double GetLocalTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_ReversalState)
	FQuickReversalNetworkState ReversalState;

	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UEvasiveRollComponent> CachedEvasiveRollComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> CachedHealthComponent;

	double NextAllowedServerTime = 0.0;
	double LocalNextAllowedTime = 0.0;
	bool bLocalActivationPending = false;
	FVector InitialForwardVelocity = FVector::ZeroVector;
	FVector CurrentLateralVelocity = FVector::ZeroVector;
	FQuat InitialFlightRotation = FQuat::Identity;
	FQuat TargetFlightRotation = FQuat::Identity;
};
