#pragma once

#include "CoreMinimal.h"
#include "JetStatsComponent.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTickState.h"

class UArcadeFlightComponent;

/** Input sampled once for every fixed 60 Hz simulation frame. */
struct FPlaneFlightInputCmd
{
	FVector2D Steering = FVector2D::ZeroVector;
	float Strafe = 0.0f;
	float Brake = 0.0f;
	bool bBoostRequested = false;
	bool bHoverRequested = false;
	uint16 AbilitySequence = 0;
	uint8 AbilityType = 0;
	float AbilityDirection = 0.0f;
	FVector2D AbilityMovementInput = FVector2D::ZeroVector;

	void NetSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
};

/**
 * Complete rewindable state of the regular flight model. Presentation-only camera
 * state deliberately does not live here.
 */
struct FPlaneFlightSyncState
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Velocity = FVector::ZeroVector;
	FVector2D SmoothedSteering = FVector2D::ZeroVector;
	float SmoothedStrafe = 0.0f;
	float SmoothedBrake = 0.0f;
	float ForwardSpeed = 0.0f;
	float VisualBankDegrees = 0.0f;
	float BoostEnergy = 0.0f;
	float BoostAlpha = 0.0f;
	float BoostRegenDelayRemaining = 0.0f;
	bool bBoosting = false;
	float HoverPresentationAlpha = 0.0f;
	uint8 HoverState = 0;
	uint16 LastProcessedAbilitySequence = 0;
	uint8 ActiveMobilityAbility = 0;
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
	uint8 QueuedAbilityType = 0;
	float QueuedAbilityDirection = 0.0f;
	FVector2D QueuedAbilityMovementInput = FVector2D::ZeroVector;
	uint16 QueuedAbilitySequence = 0;
	uint16 LastActivatedAbilitySequence = 0;
	uint8 LastActivatedAbilityType = 0;
	FVector LastBlinkDeparture = FVector::ZeroVector;
	FVector LastBlinkArrival = FVector::ZeroVector;
	bool bBoundaryReturnActive = false;
	FVector BoundaryReturnTarget = FVector::ZeroVector;
	float BoundaryTurnVisualInput = 0.0f;
	uint16 MovementEpoch = 0;

	bool ShouldReconcile(const FPlaneFlightSyncState& AuthorityState) const;
	void NetSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	void Interpolate(const FPlaneFlightSyncState* From,
		const FPlaneFlightSyncState* To, float PCT);
};

/** Rarely-changing, server-authoritative tuning used by every replayed frame. */
struct FPlaneFlightAuxState
{
	FJetFlightStats FlightStats;

	bool ShouldReconcile(const FPlaneFlightAuxState& AuthorityState) const;
	void NetSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	void Interpolate(const FPlaneFlightAuxState* From,
		const FPlaneFlightAuxState* To, float PCT);
};

using FPlaneFlightStateTypes = TNetworkPredictionStateTypes<
	FPlaneFlightInputCmd, FPlaneFlightSyncState, FPlaneFlightAuxState>;

class FPlaneFlightNetworkSimulation
{
public:
	explicit FPlaneFlightNetworkSimulation(UArcadeFlightComponent* InDriver)
		: Driver(InDriver)
	{
	}

	void SimulationTick(const FNetSimTimeStep& TimeStep,
		const TNetSimInput<FPlaneFlightStateTypes>& Input,
		const TNetSimOutput<FPlaneFlightStateTypes>& Output);

private:
	UArcadeFlightComponent* Driver = nullptr;
};

class FPlaneFlightModelDef : public FNetworkPredictionModelDef
{
public:
	NP_MODEL_BODY();

	using Simulation = FPlaneFlightNetworkSimulation;
	using StateTypes = FPlaneFlightStateTypes;
	using Driver = UArcadeFlightComponent;

	static const TCHAR* GetName() { return TEXT("PlaneFlight"); }
	static constexpr int32 GetSortPriority()
	{
		// Keep a stable order without colliding with the engine's generic kinematic model.
		return static_cast<int32>(ENetworkPredictionSortPriority::PreKinematicMovers) + 10;
	}
};
