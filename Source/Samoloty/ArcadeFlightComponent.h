#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcadeFlightComponent.generated.h"

class UJetBoostComponent;
class UJetStatsComponent;
class UBackwardDashComponent;
class UEvasiveRollComponent;
class UForwardDashComponent;
class UQuickReversalComponent;
class AFlightBoundsVolume;

UENUM(BlueprintType)
enum class EArcadeHoverState : uint8
{
	Flying,
	Entering,
	Hovering,
	Leaving
};

USTRUCT()
struct FArcadeFlightNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Location;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize100 Velocity;

	UPROPERTY()
	float VisualBankDegrees = 0.0f;

	UPROPERTY()
	uint8 BrakeAlpha = 0;

	UPROPERTY()
	double ServerTimeSeconds = 0.0;

	UPROPERTY()
	bool bTeleport = false;
};

struct FBufferedFlightState
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Velocity = FVector::ZeroVector;
	float VisualBankDegrees = 0.0f;
	float BrakeAlpha = 0.0f;
	double ServerTimeSeconds = 0.0;
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UArcadeFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcadeFlightComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called by the owning Pawn. Values are normalized to -1..1. */
	void SetLocalFlightInput(const FVector2D& Steering, float Strafe, float Brake);

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentForwardSpeed() const { return CurrentForwardSpeed; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	FVector GetCurrentVelocity() const { return CurrentVelocity; }

	/** Immediately publishes the current authoritative transform as a teleport. */
	void NotifyAuthoritativeTeleport();

	/** Hands control back after a locked maneuver without applying stored turn input at once. */
	void PrepareStraightManeuverExit();

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentTurnRateMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetSmoothedBrake() const { return SmoothedBrake; }

	UFUNCTION(BlueprintCallable, Category="Plane|Flight|Hover")
	void ToggleHover();

	UFUNCTION(BlueprintCallable, Category="Plane|Flight|Hover")
	void RequestExitHover();

	UFUNCTION(BlueprintPure, Category="Plane|Flight|Hover")
	EArcadeHoverState GetHoverState() const { return HoverState; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight|Hover")
	float GetHoverAlpha() const { return HoverPresentationAlpha; }

	/** Weapons are allowed only in the fully restored Flying state. */
	UFUNCTION(BlueprintPure, Category="Plane|Flight|Hover")
	bool IsCombatFlightEnabled() const
	{
		return HoverState == EArcadeHoverState::Flying && !bLocalHoverEntryPending;
	}

	UFUNCTION(BlueprintPure, Category="Plane|Flight|Boundary")
	float GetBoundaryWarningAlpha() const { return BoundaryWarningAlpha; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight|Boundary")
	bool IsBoundaryReturnActive() const { return bBoundaryReturnActive; }

private:
	UFUNCTION(Server, Unreliable)
	void ServerSetFlightInput(FVector2D Steering, float Strafe, float Brake);

	UFUNCTION(Server, Reliable)
	void ServerToggleHover();

	UFUNCTION(Server, Reliable)
	void ServerRequestExitHover();

	void SimulateFlight(float DeltaTime);
	void UpdateSmoothedInput(float DeltaTime, const struct FJetFlightStats& Stats);
	void RotateAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void MoveAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdateVisualBank(float DeltaTime);
	void UpdateReplicatedState();
	void InterpolateBufferedState();
	void UpdateLocalBrakePresentation(float DeltaTime, const struct FJetFlightStats& Stats);
	float CalculateTurnRateMultiplier(const struct FJetFlightStats& Stats) const;
	void SetAuthoritativeHoverState(EArcadeHoverState NewState);
	void UpdateHoverPresentation(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdateBoundaryState();

	UFUNCTION()
	void OnRep_ServerState();

	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0.016", ClampMax="0.25"))
	float InputReplicationInterval = 0.033f;

	/** How far behind server time clients render, leaving two snapshots to blend between. */
	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0.05", ClampMax="0.3"))
	float SnapshotInterpolationDelay = 0.1f;

	/** Prevents a long freeze if several packets are lost. */
	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0", ClampMax="0.25"))
	float MaxExtrapolationTime = 0.1f;

	UPROPERTY(ReplicatedUsing=OnRep_ServerState)
	FArcadeFlightNetworkState ServerState;

	UPROPERTY(Replicated)
	EArcadeHoverState HoverState = EArcadeHoverState::Flying;

	UPROPERTY(Replicated)
	bool bBoundaryReturnActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UJetStatsComponent> CachedStatsComponent;

	UPROPERTY(Transient)
	TObjectPtr<UJetBoostComponent> CachedBoostComponent;

	UPROPERTY(Transient)
	TObjectPtr<UEvasiveRollComponent> CachedEvasiveRollComponent;

	UPROPERTY(Transient)
	TObjectPtr<UQuickReversalComponent> CachedQuickReversalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBackwardDashComponent> CachedBackwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<UForwardDashComponent> CachedForwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<AFlightBoundsVolume> CachedFlightBounds;

	FVector2D RawSteering = FVector2D::ZeroVector;
	FVector2D SmoothedSteering = FVector2D::ZeroVector;
	float RawStrafe = 0.0f;
	float SmoothedStrafe = 0.0f;
	float RawBrake = 0.0f;
	float SmoothedBrake = 0.0f;
	float CurrentForwardSpeed = 0.0f;
	float CurrentVisualBankDegrees = 0.0f;
	float HoverPresentationAlpha = 0.0f;
	FVector CurrentVelocity = FVector::ZeroVector;
	float TimeSinceInputSent = 0.0f;
	bool bLocalHoverEntryPending = false;
	bool bAuthoritativeTeleportPending = false;
	float BoundaryWarningAlpha = 0.0f;
	float BoundaryTurnVisualInput = 0.0f;
	FVector BoundaryReturnTarget = FVector::ZeroVector;
	TArray<FBufferedFlightState> SnapshotBuffer;
};
