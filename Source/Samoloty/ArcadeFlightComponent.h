#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "PlaneAbilityQueueComponent.h"
#include "Templates/PimplPtr.h"
#include "ArcadeFlightComponent.generated.h"

class UJetBoostComponent;
class UJetStatsComponent;
class UBackwardDashComponent;
class UBlinkComponent;
class UEvasiveRollComponent;
class UForwardDashComponent;
class UQuickReversalComponent;
class AFlightBoundsVolume;
class FPlaneFlightNetworkSimulation;
struct FPlaneFlightInputCmd;
struct FPlaneFlightSyncState;
struct FPlaneFlightAuxState;

UENUM(BlueprintType)
enum class EArcadeHoverState : uint8
{
	Flying,
	Entering,
	Hovering,
	Leaving
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UArcadeFlightComponent : public UNetworkPredictionComponent
{
	GENERATED_BODY()

public:
	UArcadeFlightComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called by the owning Pawn. Values are normalized to -1..1. */
	void SetLocalFlightInput(const FVector2D& Steering, float Strafe, float Brake);
	void SetLocalBoostRequested(bool bRequested);
	void QueuePredictedAbility(EPlaneAbilityType Type, float Direction = 0.0f,
		FVector2D MovementInput = FVector2D::ZeroVector);

	// Network Prediction driver API. These functions are public because the
	// framework invokes them through its templated driver layer.
	void ProduceInput(int32 DeltaTimeMS, FPlaneFlightInputCmd* Cmd);
	void RestoreFrame(const FPlaneFlightSyncState* SyncState,
		const FPlaneFlightAuxState* AuxState);
	void FinalizeFrame(const FPlaneFlightSyncState* SyncState,
		const FPlaneFlightAuxState* AuxState);
	void FinalizeSmoothingFrame(const FPlaneFlightSyncState* SyncState,
		const FPlaneFlightAuxState* AuxState);
	void InitializeSimulationState(FPlaneFlightSyncState* SyncState,
		FPlaneFlightAuxState* AuxState);
	void RunNetworkSimulationStep(float DeltaTime,
		const FPlaneFlightInputCmd& InputCmd,
		const FPlaneFlightSyncState& InputState,
		const FPlaneFlightAuxState& AuxState,
		FPlaneFlightSyncState& OutputState);

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentForwardSpeed() const { return CurrentForwardSpeed; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	FVector GetCurrentVelocity() const { return CurrentVelocity; }

	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	bool IsPredictedBoosting() const { return bCurrentBoosting; }

	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	float GetPredictedBoostAlpha() const { return CurrentBoostAlpha; }

	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	float GetPredictedBoostEnergy() const { return CurrentBoostEnergy; }

	/** Immediately publishes the current authoritative transform as a teleport. */
	void NotifyAuthoritativeTeleport();

	/** Hands control back after a locked maneuver without applying stored turn input at once. */
	void PrepareStraightManeuverExit();

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentTurnRateMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetSmoothedBrake() const { return SmoothedBrake; }

	/** Returns true only when the request was accepted by the flight simulation. */
	UFUNCTION(BlueprintCallable, Category="Plane|Flight|Hover")
	bool ToggleHover();

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

	/** HUD remains visible until the aircraft actually starts entering hover. */
	UFUNCTION(BlueprintPure, Category="Plane|Flight|Hover")
	bool AreCombatElementsVisible() const
	{
		return HoverState == EArcadeHoverState::Flying;
	}

	/** Hover waits for non-roll mobility maneuvers and their camera recovery. */
	bool CanBeginHoverEntry() const;

	/** Cancels an unsafe entry when the movement capsule touches geometry. */
	void CancelHoverEntryFromCollision();

	UFUNCTION(BlueprintPure, Category="Plane|Flight|Boundary")
	float GetBoundaryWarningAlpha() const { return BoundaryWarningAlpha; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight|Boundary")
	bool IsBoundaryReturnActive() const { return bBoundaryReturnActive; }

	bool IsEvasiveRollActive() const { return bEvasiveRollActive; }
	float GetEvasiveRollProgress() const;
	float GetEvasiveRollDirection() const { return EvasiveRollDirection; }
	float GetEvasiveRollCooldownRemaining() const { return EvasiveRollCooldownRemaining; }
	bool IsQuickReversalActive() const;
	bool IsBackwardDashActive() const;
	bool IsForwardDashActive() const;
	float GetMobilityProgress() const;
	float GetMobilityDirection() const { return MobilityDirection; }
	FQuat GetMobilityInitialRotation() const { return MobilityInitialRotation.Quaternion(); }
	float GetQuickReversalCooldownRemaining() const { return QuickReversalCooldownRemaining; }
	float GetBackwardDashCooldownRemaining() const { return BackwardDashCooldownRemaining; }
	float GetForwardDashCooldownRemaining() const { return ForwardDashCooldownRemaining; }
	float GetBlinkCooldownRemaining() const { return BlinkCooldownRemaining; }
	bool IsBlinkCameraRecovering() const { return BlinkCameraRecoveryRemaining > 0.0f; }
	EPlaneAbilityType GetPredictedQueuedAbility() const
	{
		return static_cast<EPlaneAbilityType>(QueuedAbilityType);
	}

private:
	enum class ENetworkStateApplication : uint8
	{
		Simulation,
		Restore,
		Finalize,
		Presentation
	};

	virtual void InitializeNetworkPredictionProxy() override;
	void CacheDependencies();
	void ApplyNetworkSyncState(const FPlaneFlightSyncState& State,
		ETeleportType TeleportType, ENetworkStateApplication Application);
	void CaptureNetworkSyncState(FPlaneFlightSyncState& State) const;

	void SimulateFlight(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdatePredictedHoverRequest();
	void UpdatePredictedAbilities(float DeltaTime, const struct FJetFlightStats& Stats);
	void ProcessAbilityRequest(uint16 Sequence, EPlaneAbilityType Type,
		float Direction, const FVector2D& MovementInput,
		const struct FJetFlightStats& Stats);
	bool TryStartAbility(uint16 Sequence, EPlaneAbilityType Type,
		float Direction, const FVector2D& MovementInput,
		const struct FJetFlightStats& Stats);
	bool IsAbilityBlocked(EPlaneAbilityType Type) const;
	void StartMobilityAbility(uint16 Sequence, EPlaneAbilityType Type,
		float Direction, const struct FJetFlightStats& Stats);
	void ExecuteBlink(uint16 Sequence, const FVector2D& MovementInput);
	void FinishMobilityAbility();
	FQuat BuildQuickReversalRotation(float Progress) const;
	FVector BuildQuickReversalArcOffset(float Progress) const;
	void PlayAbilityEffectIfNeeded();
	void UpdatePredictedBoost(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdateSmoothedInput(float DeltaTime, const struct FJetFlightStats& Stats);
	void RotateAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void MoveAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdateVisualBank(float DeltaTime);
	float CalculateTurnRateMultiplier(const struct FJetFlightStats& Stats) const;
	void SetSimulationHoverState(EArcadeHoverState NewState);
	void UpdateHoverPresentation(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdateBoundaryState();
	void UpdateBoundaryWarning();

	EArcadeHoverState HoverState = EArcadeHoverState::Flying;

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
	TObjectPtr<UBlinkComponent> CachedBlinkComponent;

	UPROPERTY(Transient)
	TObjectPtr<AFlightBoundsVolume> CachedFlightBounds;

	FVector2D RawSteering = FVector2D::ZeroVector;
	FVector2D SmoothedSteering = FVector2D::ZeroVector;
	float RawStrafe = 0.0f;
	float SmoothedStrafe = 0.0f;
	float RawBrake = 0.0f;
	bool bRawBoostRequested = false;
	bool bRawHoverRequested = false;
	// Producer input must not be overwritten by replaying an older predicted frame.
	bool bLocalBoostInputRequested = false;
	bool bLocalHoverInputRequested = false;
	uint16 RawAbilitySequence = 0;
	EPlaneAbilityType RawAbilityType = EPlaneAbilityType::None;
	float RawAbilityDirection = 0.0f;
	FVector2D RawAbilityMovementInput = FVector2D::ZeroVector;
	float SmoothedBrake = 0.0f;
	float CurrentForwardSpeed = 0.0f;
	float CurrentVisualBankDegrees = 0.0f;
	float CurrentBoostEnergy = 0.0f;
	float CurrentBoostAlpha = 0.0f;
	float BoostRegenDelayRemaining = 0.0f;
	bool bCurrentBoosting = false;
	float HoverPresentationAlpha = 0.0f;
	uint16 LastProcessedAbilitySequence = 0;
	EPlaneAbilityType ActiveMobilityAbility = EPlaneAbilityType::None;
	float MobilityElapsed = 0.0f;
	float MobilityDirection = 1.0f;
	FRotator MobilityInitialRotation = FRotator::ZeroRotator;
	FVector MobilityInitialForwardVelocity = FVector::ZeroVector;
	FVector MobilityLateralVelocity = FVector::ZeroVector;
	FVector MobilityPreviousPathOffset = FVector::ZeroVector;
	float MobilityInitialForwardSpeed = 0.0f;
	bool bEvasiveRollActive = false;
	float EvasiveRollElapsed = 0.0f;
	float EvasiveRollDirection = 1.0f;
	float EvasiveRollCooldownRemaining = 0.0f;
	float QuickReversalCooldownRemaining = 0.0f;
	float BackwardDashCooldownRemaining = 0.0f;
	float ForwardDashCooldownRemaining = 0.0f;
	float BlinkCooldownRemaining = 0.0f;
	float BlinkCameraRecoveryRemaining = 0.0f;
	EPlaneAbilityType QueuedAbilityType = EPlaneAbilityType::None;
	float QueuedAbilityDirection = 0.0f;
	FVector2D QueuedAbilityMovementInput = FVector2D::ZeroVector;
	uint16 QueuedAbilitySequence = 0;
	uint16 LastActivatedAbilitySequence = 0;
	EPlaneAbilityType LastActivatedAbilityType = EPlaneAbilityType::None;
	FVector LastBlinkDeparture = FVector::ZeroVector;
	FVector LastBlinkArrival = FVector::ZeroVector;
	uint16 LastPresentedAbilitySequence = 0;
	FVector CurrentVelocity = FVector::ZeroVector;
	bool bLocalHoverEntryPending = false;
	float BoundaryWarningAlpha = 0.0f;
	float BoundaryTurnVisualInput = 0.0f;
	FVector BoundaryReturnTarget = FVector::ZeroVector;

	TPimplPtr<FPlaneFlightNetworkSimulation> OwnedNetworkSimulation;
	uint16 MovementEpoch = 0;
};
