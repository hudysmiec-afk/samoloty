#include "PlaneFlightNetworkSimulation.h"

#include "ArcadeFlightComponent.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionTrace.h"

namespace PlaneFlightPrediction
{
	constexpr float LocationToleranceCm = 30.0f;
	constexpr float RotationToleranceDegrees = 0.75f;
	constexpr float VelocityToleranceCmPerSecond = 50.0f;
	constexpr float ScalarTolerance = 0.02f;
	constexpr float TeleportDistanceCm = 5000.0f;

	void SerializeUnitFloat(FArchive& Ar, float& Value)
	{
		int16 Quantized = Ar.IsSaving()
			? static_cast<int16>(FMath::RoundToInt(
				FMath::Clamp(Value, -1.0f, 1.0f) * 32767.0f))
			: 0;
		Ar << Quantized;
		if (Ar.IsLoading())
		{
			Value = static_cast<float>(Quantized) / 32767.0f;
		}
	}

	void SerializeUnitFloat(FArchive& Ar, double& Value)
	{
		float FloatValue = static_cast<float>(Value);
		SerializeUnitFloat(Ar, FloatValue);
		if (Ar.IsLoading())
		{
			Value = FloatValue;
		}
	}

	void SerializeFlightStats(FArchive& Ar, FJetFlightStats& Stats)
	{
		Ar << Stats.ForwardSpeed;
		Ar << Stats.MinForwardSpeed;
		Ar << Stats.ForwardSpeedResponse;
		Ar << Stats.MaxClimbSpeedMultiplier;
		Ar << Stats.MaxDiveSpeedMultiplier;
		Ar << Stats.MaxClimbResponseMultiplier;
		Ar << Stats.MaxDiveResponseMultiplier;
		Ar << Stats.StrafeSpeedMultiplier;
		Ar << Stats.MaxYawTurnRate;
		Ar << Stats.MaxPitchTurnRate;
		Ar << Stats.MinSpeedTurnMultiplier;
		Ar << Stats.BoostSpeedTurnMultiplier;
		Ar << Stats.BrakeInputResponseSpeed;
		Ar << Stats.MaxPitch;
		Ar << Stats.HoverPitch;
		Ar << Stats.HoverRotationResponse;
		Ar << Stats.HoverDecelerationResponse;
		Ar << Stats.HoverExitAccelerationResponse;
		Ar << Stats.BoostSpeedMultiplier;
		Ar << Stats.BoostResponseSpeed;
		Ar << Stats.MaxBoostEnergy;
		Ar << Stats.BoostEnergyDrainPerSecond;
		Ar << Stats.BoostEnergyRegenPerSecond;
		Ar << Stats.BoostRegenDelay;
	}

	bool FlightStatsEqual(const FJetFlightStats& A, const FJetFlightStats& B)
	{
		return FMath::IsNearlyEqual(A.ForwardSpeed, B.ForwardSpeed)
			&& FMath::IsNearlyEqual(A.MinForwardSpeed, B.MinForwardSpeed)
			&& FMath::IsNearlyEqual(A.ForwardSpeedResponse, B.ForwardSpeedResponse)
			&& FMath::IsNearlyEqual(A.MaxClimbSpeedMultiplier, B.MaxClimbSpeedMultiplier)
			&& FMath::IsNearlyEqual(A.MaxDiveSpeedMultiplier, B.MaxDiveSpeedMultiplier)
			&& FMath::IsNearlyEqual(A.MaxClimbResponseMultiplier, B.MaxClimbResponseMultiplier)
			&& FMath::IsNearlyEqual(A.MaxDiveResponseMultiplier, B.MaxDiveResponseMultiplier)
			&& FMath::IsNearlyEqual(A.StrafeSpeedMultiplier, B.StrafeSpeedMultiplier)
			&& FMath::IsNearlyEqual(A.MaxYawTurnRate, B.MaxYawTurnRate)
			&& FMath::IsNearlyEqual(A.MaxPitchTurnRate, B.MaxPitchTurnRate)
			&& FMath::IsNearlyEqual(A.MinSpeedTurnMultiplier, B.MinSpeedTurnMultiplier)
			&& FMath::IsNearlyEqual(A.BoostSpeedTurnMultiplier, B.BoostSpeedTurnMultiplier)
			&& FMath::IsNearlyEqual(A.BrakeInputResponseSpeed, B.BrakeInputResponseSpeed)
			&& FMath::IsNearlyEqual(A.MaxPitch, B.MaxPitch)
			&& FMath::IsNearlyEqual(A.HoverPitch, B.HoverPitch)
			&& FMath::IsNearlyEqual(A.HoverRotationResponse, B.HoverRotationResponse)
			&& FMath::IsNearlyEqual(A.HoverDecelerationResponse, B.HoverDecelerationResponse)
			&& FMath::IsNearlyEqual(A.HoverExitAccelerationResponse, B.HoverExitAccelerationResponse)
			&& FMath::IsNearlyEqual(A.BoostSpeedMultiplier, B.BoostSpeedMultiplier)
			&& FMath::IsNearlyEqual(A.BoostResponseSpeed, B.BoostResponseSpeed)
			&& FMath::IsNearlyEqual(A.MaxBoostEnergy, B.MaxBoostEnergy)
			&& FMath::IsNearlyEqual(A.BoostEnergyDrainPerSecond, B.BoostEnergyDrainPerSecond)
			&& FMath::IsNearlyEqual(A.BoostEnergyRegenPerSecond, B.BoostEnergyRegenPerSecond)
			&& FMath::IsNearlyEqual(A.BoostRegenDelay, B.BoostRegenDelay);
	}
}

NP_MODEL_REGISTER(FPlaneFlightModelDef);

void FPlaneFlightInputCmd::NetSerialize(const FNetSerializeParams& P)
{
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, Steering.X);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, Steering.Y);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, Strafe);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, Brake);
	P.Ar << bBoostRequested;
	P.Ar << bHoverRequested;
	P.Ar << AbilitySequence;
	P.Ar << AbilityType;
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, AbilityDirection);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, AbilityMovementInput.X);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, AbilityMovementInput.Y);
}

void FPlaneFlightInputCmd::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Steering=(%.2f, %.2f) Strafe=%.2f Brake=%.2f Boost=%d Hover=%d Ability=%u:%u",
		Steering.X, Steering.Y, Strafe, Brake,
		bBoostRequested ? 1 : 0, bHoverRequested ? 1 : 0,
		AbilitySequence, AbilityType);
}

bool FPlaneFlightSyncState::ShouldReconcile(
	const FPlaneFlightSyncState& AuthorityState) const
{
	UE_NP_TRACE_RECONCILE(MovementEpoch != AuthorityState.MovementEpoch, "Epoch");
	UE_NP_TRACE_RECONCILE(
		FVector::DistSquared(Location, AuthorityState.Location)
			> FMath::Square(PlaneFlightPrediction::LocationToleranceCm), "Location");
	const float RotationError = FMath::RadiansToDegrees(
		Rotation.Quaternion().AngularDistance(AuthorityState.Rotation.Quaternion()));
	UE_NP_TRACE_RECONCILE(
		RotationError > PlaneFlightPrediction::RotationToleranceDegrees, "Rotation");
	UE_NP_TRACE_RECONCILE(
		FVector::DistSquared(Velocity, AuthorityState.Velocity)
			> FMath::Square(PlaneFlightPrediction::VelocityToleranceCmPerSecond), "Velocity");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(ForwardSpeed - AuthorityState.ForwardSpeed) > 10.0f, "ForwardSpeed");
	UE_NP_TRACE_RECONCILE(
		!SmoothedSteering.Equals(AuthorityState.SmoothedSteering,
			PlaneFlightPrediction::ScalarTolerance), "SmoothedSteering");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(SmoothedStrafe - AuthorityState.SmoothedStrafe)
			> PlaneFlightPrediction::ScalarTolerance, "SmoothedStrafe");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(SmoothedBrake - AuthorityState.SmoothedBrake)
			> PlaneFlightPrediction::ScalarTolerance, "SmoothedBrake");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(BoostEnergy - AuthorityState.BoostEnergy) > 0.25f, "BoostEnergy");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(BoostAlpha - AuthorityState.BoostAlpha) > 0.02f, "BoostAlpha");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(BoostRegenDelayRemaining - AuthorityState.BoostRegenDelayRemaining)
			> 0.02f, "BoostRegenDelay");
	UE_NP_TRACE_RECONCILE(bBoosting != AuthorityState.bBoosting, "Boosting");
	UE_NP_TRACE_RECONCILE(
		FMath::Abs(HoverPresentationAlpha - AuthorityState.HoverPresentationAlpha)
			> 0.02f, "HoverAlpha");
	UE_NP_TRACE_RECONCILE(HoverState != AuthorityState.HoverState, "HoverState");
	UE_NP_TRACE_RECONCILE(
		LastProcessedAbilitySequence != AuthorityState.LastProcessedAbilitySequence,
		"AbilitySequence");
	UE_NP_TRACE_RECONCILE(ActiveMobilityAbility != AuthorityState.ActiveMobilityAbility,
		"MobilityType");
	UE_NP_TRACE_RECONCILE(FMath::Abs(MobilityElapsed - AuthorityState.MobilityElapsed) > 0.02f,
		"MobilityElapsed");
	UE_NP_TRACE_RECONCILE(bEvasiveRollActive != AuthorityState.bEvasiveRollActive,
		"RollActive");
	UE_NP_TRACE_RECONCILE(FMath::Abs(EvasiveRollElapsed - AuthorityState.EvasiveRollElapsed) > 0.02f,
		"RollElapsed");
	UE_NP_TRACE_RECONCILE(QueuedAbilitySequence != AuthorityState.QueuedAbilitySequence,
		"QueuedAbility");
	UE_NP_TRACE_RECONCILE(bBoundaryReturnActive != AuthorityState.bBoundaryReturnActive,
		"BoundaryReturn");
	return false;
}

void FPlaneFlightSyncState::NetSerialize(const FNetSerializeParams& P)
{
	FVector_NetQuantize100 QuantizedLocation(Location);
	FVector_NetQuantize10 QuantizedVelocity(Velocity);
	bool bSuccess = true;
	QuantizedLocation.NetSerialize(P.Ar, P.Map, bSuccess);
	Rotation.SerializeCompressedShort(P.Ar);
	QuantizedVelocity.NetSerialize(P.Ar, P.Map, bSuccess);
	if (P.Ar.IsLoading())
	{
		Location = QuantizedLocation;
		Velocity = QuantizedVelocity;
	}
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, SmoothedSteering.X);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, SmoothedSteering.Y);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, SmoothedStrafe);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, SmoothedBrake);
	P.Ar << ForwardSpeed;
	P.Ar << VisualBankDegrees;
	P.Ar << BoostEnergy;
	P.Ar << BoostAlpha;
	P.Ar << BoostRegenDelayRemaining;
	P.Ar << bBoosting;
	P.Ar << HoverPresentationAlpha;
	P.Ar << HoverState;
	P.Ar << LastProcessedAbilitySequence;
	P.Ar << ActiveMobilityAbility;
	P.Ar << MobilityElapsed;
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, MobilityDirection);
	MobilityInitialRotation.SerializeCompressedShort(P.Ar);
	P.Ar << MobilityInitialForwardVelocity;
	P.Ar << MobilityLateralVelocity;
	P.Ar << MobilityPreviousPathOffset;
	P.Ar << MobilityInitialForwardSpeed;
	P.Ar << bEvasiveRollActive;
	P.Ar << EvasiveRollElapsed;
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, EvasiveRollDirection);
	P.Ar << EvasiveRollCooldownRemaining;
	P.Ar << QuickReversalCooldownRemaining;
	P.Ar << BackwardDashCooldownRemaining;
	P.Ar << ForwardDashCooldownRemaining;
	P.Ar << BlinkCooldownRemaining;
	P.Ar << BlinkCameraRecoveryRemaining;
	P.Ar << QueuedAbilityType;
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, QueuedAbilityDirection);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, QueuedAbilityMovementInput.X);
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, QueuedAbilityMovementInput.Y);
	P.Ar << QueuedAbilitySequence;
	P.Ar << LastActivatedAbilitySequence;
	P.Ar << LastActivatedAbilityType;
	P.Ar << LastBlinkDeparture;
	P.Ar << LastBlinkArrival;
	P.Ar << bBoundaryReturnActive;
	P.Ar << BoundaryReturnTarget;
	PlaneFlightPrediction::SerializeUnitFloat(P.Ar, BoundaryTurnVisualInput);
	P.Ar << MovementEpoch;
}

void FPlaneFlightSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Loc=(%.1f, %.1f, %.1f) Vel=(%.1f, %.1f, %.1f) Rot=(%.1f, %.1f, %.1f) Speed=%.1f Boost=%.2f/%.1f Hover=%u/%.2f Ability=%u/%.2f Roll=%d/%.2f Epoch=%u",
		Location.X, Location.Y, Location.Z, Velocity.X, Velocity.Y, Velocity.Z,
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll, ForwardSpeed,
		BoostAlpha, BoostEnergy, HoverState, HoverPresentationAlpha,
		ActiveMobilityAbility, MobilityElapsed, bEvasiveRollActive ? 1 : 0,
		EvasiveRollElapsed, MovementEpoch);
}

void FPlaneFlightSyncState::Interpolate(const FPlaneFlightSyncState* From,
	const FPlaneFlightSyncState* To, const float PCT)
{
	if (From->MovementEpoch != To->MovementEpoch
		|| FVector::DistSquared(From->Location, To->Location)
			> FMath::Square(PlaneFlightPrediction::TeleportDistanceCm))
	{
		*this = *To;
		return;
	}
	Location = FMath::Lerp(From->Location, To->Location, PCT);
	Velocity = FMath::Lerp(From->Velocity, To->Velocity, PCT);
	Rotation = FQuat::Slerp(
		From->Rotation.Quaternion(), To->Rotation.Quaternion(), PCT).Rotator();
	SmoothedSteering = FMath::Lerp(From->SmoothedSteering, To->SmoothedSteering, PCT);
	SmoothedStrafe = FMath::Lerp(From->SmoothedStrafe, To->SmoothedStrafe, PCT);
	SmoothedBrake = FMath::Lerp(From->SmoothedBrake, To->SmoothedBrake, PCT);
	ForwardSpeed = FMath::Lerp(From->ForwardSpeed, To->ForwardSpeed, PCT);
	VisualBankDegrees = FMath::Lerp(
		From->VisualBankDegrees, To->VisualBankDegrees, PCT);
	BoostEnergy = FMath::Lerp(From->BoostEnergy, To->BoostEnergy, PCT);
	BoostAlpha = FMath::Lerp(From->BoostAlpha, To->BoostAlpha, PCT);
	BoostRegenDelayRemaining = FMath::Lerp(
		From->BoostRegenDelayRemaining, To->BoostRegenDelayRemaining, PCT);
	bBoosting = To->bBoosting;
	HoverPresentationAlpha = FMath::Lerp(
		From->HoverPresentationAlpha, To->HoverPresentationAlpha, PCT);
	HoverState = To->HoverState;
	LastProcessedAbilitySequence = To->LastProcessedAbilitySequence;
	ActiveMobilityAbility = To->ActiveMobilityAbility;
	MobilityElapsed = From->ActiveMobilityAbility == To->ActiveMobilityAbility
		? FMath::Lerp(From->MobilityElapsed, To->MobilityElapsed, PCT)
		: To->MobilityElapsed;
	MobilityDirection = To->MobilityDirection;
	MobilityInitialRotation = To->MobilityInitialRotation;
	MobilityInitialForwardVelocity = To->MobilityInitialForwardVelocity;
	MobilityLateralVelocity = To->MobilityLateralVelocity;
	MobilityPreviousPathOffset = To->MobilityPreviousPathOffset;
	MobilityInitialForwardSpeed = To->MobilityInitialForwardSpeed;
	bEvasiveRollActive = To->bEvasiveRollActive;
	EvasiveRollElapsed = From->bEvasiveRollActive && To->bEvasiveRollActive
		? FMath::Lerp(From->EvasiveRollElapsed, To->EvasiveRollElapsed, PCT)
		: To->EvasiveRollElapsed;
	EvasiveRollDirection = To->EvasiveRollDirection;
	EvasiveRollCooldownRemaining = FMath::Lerp(
		From->EvasiveRollCooldownRemaining, To->EvasiveRollCooldownRemaining, PCT);
	QuickReversalCooldownRemaining = FMath::Lerp(
		From->QuickReversalCooldownRemaining, To->QuickReversalCooldownRemaining, PCT);
	BackwardDashCooldownRemaining = FMath::Lerp(
		From->BackwardDashCooldownRemaining, To->BackwardDashCooldownRemaining, PCT);
	ForwardDashCooldownRemaining = FMath::Lerp(
		From->ForwardDashCooldownRemaining, To->ForwardDashCooldownRemaining, PCT);
	BlinkCooldownRemaining = FMath::Lerp(
		From->BlinkCooldownRemaining, To->BlinkCooldownRemaining, PCT);
	BlinkCameraRecoveryRemaining = FMath::Lerp(
		From->BlinkCameraRecoveryRemaining, To->BlinkCameraRecoveryRemaining, PCT);
	QueuedAbilityType = To->QueuedAbilityType;
	QueuedAbilityDirection = To->QueuedAbilityDirection;
	QueuedAbilityMovementInput = To->QueuedAbilityMovementInput;
	QueuedAbilitySequence = To->QueuedAbilitySequence;
	LastActivatedAbilitySequence = To->LastActivatedAbilitySequence;
	LastActivatedAbilityType = To->LastActivatedAbilityType;
	LastBlinkDeparture = To->LastBlinkDeparture;
	LastBlinkArrival = To->LastBlinkArrival;
	bBoundaryReturnActive = To->bBoundaryReturnActive;
	BoundaryReturnTarget = To->BoundaryReturnTarget;
	BoundaryTurnVisualInput = FMath::Lerp(
		From->BoundaryTurnVisualInput, To->BoundaryTurnVisualInput, PCT);
	MovementEpoch = To->MovementEpoch;
}

bool FPlaneFlightAuxState::ShouldReconcile(
	const FPlaneFlightAuxState& AuthorityState) const
{
	return !PlaneFlightPrediction::FlightStatsEqual(FlightStats, AuthorityState.FlightStats);
}

void FPlaneFlightAuxState::NetSerialize(const FNetSerializeParams& P)
{
	PlaneFlightPrediction::SerializeFlightStats(P.Ar, FlightStats);
}

void FPlaneFlightAuxState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("NormalSpeed=%.1f MinSpeed=%.1f YawRate=%.1f PitchRate=%.1f",
		FlightStats.ForwardSpeed, FlightStats.MinForwardSpeed,
		FlightStats.MaxYawTurnRate, FlightStats.MaxPitchTurnRate);
}

void FPlaneFlightAuxState::Interpolate(const FPlaneFlightAuxState* From,
	const FPlaneFlightAuxState* To, const float PCT)
{
	*this = *To;
}

void FPlaneFlightNetworkSimulation::SimulationTick(
	const FNetSimTimeStep& TimeStep,
	const TNetSimInput<FPlaneFlightStateTypes>& Input,
	const TNetSimOutput<FPlaneFlightStateTypes>& Output)
{
	*Output.Sync = *Input.Sync;
	if (Driver)
	{
		Driver->RunNetworkSimulationStep(
			static_cast<float>(TimeStep.StepMS) / 1000.0f,
			*Input.Cmd, *Input.Sync, *Input.Aux, *Output.Sync);
	}
}
