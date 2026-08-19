#include "ArcadeFlightComponent.h"

#include "ArcadeJetPawn.h"
#include "BackwardDashComponent.h"
#include "BlinkComponent.h"
#include "EvasiveRollComponent.h"
#include "ForwardDashComponent.h"
#include "FlightBoundsVolume.h"
#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "PlaneFlightNetworkSimulation.h"
#include "QuickReversalComponent.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionProxyInit.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace ArcadeFlightTuning
{
	// Flight-feel tuning is intentionally code-only while the prototype is being established.
	constexpr float SteeringResponseSpeed = 4.0f;
	constexpr float TurnRateScale = 1.4f;
	constexpr float StrafeEngageSpeed = 10.0f;
	constexpr float StrafeReleaseSpeed = 1.0f;
	constexpr float CursorSideRateMax = 0.7f;
	constexpr float VisualBankControlRate = 2.0f;
	constexpr float MaxVisualBankDegrees = 72.0f;
	constexpr float HoverExitRotationResponseScale = 1.75f;
	constexpr float HoverExitControlPitchTolerance = 0.5f;
	constexpr float EvasiveRollDuration = 1.0f;
	constexpr float EvasiveRollCooldown = 3.0f;
	constexpr float BackwardTravelDuration = 1.50f;
	constexpr float BackwardRecoveryDuration = 0.35f;
	constexpr float BackwardDuration = BackwardTravelDuration + BackwardRecoveryDuration;
	constexpr float BackwardCooldown = 4.0f;
	constexpr float BackwardTravelDistance = 20000.0f;
	constexpr float BackwardLateralResponse = 5.0f;
	constexpr float ForwardDashDuration = 2.0f;
	constexpr float ForwardDashCooldown = 6.0f;
	constexpr float ForwardDashAccelerationDuration = 0.20f;
	constexpr float ForwardDashSpeedMultiplier = 4.0f;
	constexpr float QuickReversalDuration = 1.20f;
	constexpr float QuickReversalCooldown = 4.0f;
	constexpr float QuickRotationStart = 0.02f;
	constexpr float QuickRotationEnd = 0.62f;
	constexpr float QuickUprightStart = 0.62f;
	constexpr float QuickUprightEnd = 0.88f;
	constexpr float QuickCoastEnd = 0.58f;
	constexpr float QuickThrustStart = 0.54f;
	constexpr float QuickThrustFull = 0.94f;
	constexpr float QuickLateralResponse = 5.0f;
	constexpr float QuickArcRadius = 300.0f;
	constexpr float BlinkDistance = 10000.0f;
	constexpr float BlinkCooldown = 8.0f;
	constexpr float BlinkCameraRecoveryDuration = 0.35f;
}

UArcadeFlightComponent::UArcadeFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void UArcadeFlightComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheDependencies();
	if (AActor* Owner = GetOwner())
	{
		// Input is collected by the Pawn first. Flight then updates the authoritative
		// transform before the local camera and weapon aim are refreshed.
		AddTickPrerequisiteActor(Owner);
		if (CachedBoostComponent)
		{
			AddTickPrerequisiteComponent(CachedBoostComponent);
		}
		if (CachedStatsComponent)
		{
			CurrentForwardSpeed = CachedStatsComponent->GetFlightStats().ForwardSpeed;
		}
	}
}

void UArcadeFlightComponent::SetLocalFlightInput(const FVector2D& Steering, const float Strafe,
	const float Brake)
{
	RawSteering.X = FMath::Clamp(Steering.X, -1.0f, 1.0f);
	RawSteering.Y = FMath::Clamp(Steering.Y, -1.0f, 1.0f);
	RawStrafe = FMath::Clamp(Strafe, -1.0f, 1.0f);
	RawBrake = FMath::Clamp(Brake, 0.0f, 1.0f);
}

void UArcadeFlightComponent::SetLocalBoostRequested(const bool bRequested)
{
	bLocalBoostInputRequested = bRequested;
	bRawBoostRequested = bRequested;
}

void UArcadeFlightComponent::QueuePredictedAbility(
	const EPlaneAbilityType Type, const float Direction, const FVector2D MovementInput)
{
	if (Type == EPlaneAbilityType::None)
	{
		return;
	}
	++RawAbilitySequence;
	if (RawAbilitySequence == 0)
	{
		++RawAbilitySequence;
	}
	RawAbilityType = Type;
	RawAbilityDirection = FMath::Clamp(Direction, -1.0f, 1.0f);
	RawAbilityMovementInput = FVector2D(
		FMath::Clamp(MovementInput.X, -1.0f, 1.0f),
		FMath::Clamp(MovementInput.Y, -1.0f, 1.0f));
}

float UArcadeFlightComponent::GetEvasiveRollProgress() const
{
	return bEvasiveRollActive
		? FMath::Clamp(EvasiveRollElapsed / ArcadeFlightTuning::EvasiveRollDuration, 0.0f, 1.0f)
		: 0.0f;
}

bool UArcadeFlightComponent::IsQuickReversalActive() const
{
	return ActiveMobilityAbility == EPlaneAbilityType::QuickReversal;
}

bool UArcadeFlightComponent::IsBackwardDashActive() const
{
	return ActiveMobilityAbility == EPlaneAbilityType::BackwardDash;
}

bool UArcadeFlightComponent::IsForwardDashActive() const
{
	return ActiveMobilityAbility == EPlaneAbilityType::ForwardDash;
}

float UArcadeFlightComponent::GetMobilityProgress() const
{
	float Duration = 1.0f;
	switch (ActiveMobilityAbility)
	{
	case EPlaneAbilityType::QuickReversal:
		Duration = ArcadeFlightTuning::QuickReversalDuration;
		break;
	case EPlaneAbilityType::BackwardDash:
		Duration = ArcadeFlightTuning::BackwardDuration;
		break;
	case EPlaneAbilityType::ForwardDash:
		Duration = ArcadeFlightTuning::ForwardDashDuration;
		break;
	default:
		return 0.0f;
	}
	return FMath::Clamp(MobilityElapsed / Duration, 0.0f, 1.0f);
}

void UArcadeFlightComponent::CacheDependencies()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	CachedStatsComponent = Owner->FindComponentByClass<UJetStatsComponent>();
	CachedBoostComponent = Owner->FindComponentByClass<UJetBoostComponent>();
	CachedEvasiveRollComponent = Owner->FindComponentByClass<UEvasiveRollComponent>();
	CachedQuickReversalComponent = Owner->FindComponentByClass<UQuickReversalComponent>();
	CachedBackwardDashComponent = Owner->FindComponentByClass<UBackwardDashComponent>();
	CachedForwardDashComponent = Owner->FindComponentByClass<UForwardDashComponent>();
	CachedBlinkComponent = Owner->FindComponentByClass<UBlinkComponent>();
	if (!CachedFlightBounds && GetWorld())
	{
		for (TActorIterator<AFlightBoundsVolume> It(GetWorld()); It; ++It)
		{
			CachedFlightBounds = *It;
			break;
		}
	}
}

void UArcadeFlightComponent::InitializeNetworkPredictionProxy()
{
	CacheDependencies();
	OwnedNetworkSimulation = MakePimpl<FPlaneFlightNetworkSimulation>(this);
	NetworkPredictionProxy.Init<FPlaneFlightModelDef>(
		GetWorld(), GetReplicationProxies(), OwnedNetworkSimulation.Get(), this);
}

void UArcadeFlightComponent::ProduceInput(
	const int32 DeltaTimeMS, FPlaneFlightInputCmd* Cmd)
{
	if (!Cmd)
	{
		return;
	}
	Cmd->Steering = FVector2D(
		FMath::Clamp(RawSteering.X, -1.0f, 1.0f),
		FMath::Clamp(RawSteering.Y, -1.0f, 1.0f));
	Cmd->Strafe = FMath::Clamp(RawStrafe, -1.0f, 1.0f);
	Cmd->Brake = FMath::Clamp(RawBrake, 0.0f, 1.0f);
	Cmd->bBoostRequested = bLocalBoostInputRequested;
	Cmd->bHoverRequested = bLocalHoverInputRequested;
	Cmd->AbilitySequence = RawAbilitySequence;
	Cmd->AbilityType = static_cast<uint8>(RawAbilityType);
	Cmd->AbilityDirection = RawAbilityDirection;
	Cmd->AbilityMovementInput = RawAbilityMovementInput;
}

void UArcadeFlightComponent::InitializeSimulationState(
	FPlaneFlightSyncState* SyncState, FPlaneFlightAuxState* AuxState)
{
	CacheDependencies();
	if (SyncState)
	{
		if (CachedStatsComponent && CurrentForwardSpeed <= UE_SMALL_NUMBER)
		{
			const FJetFlightStats& Stats = CachedStatsComponent->GetFlightStats();
			CurrentForwardSpeed = Stats.ForwardSpeed;
			CurrentBoostEnergy = Stats.MaxBoostEnergy;
			CurrentVelocity = GetOwner()
				? GetOwner()->GetActorForwardVector() * CurrentForwardSpeed
				: FVector::ZeroVector;
		}
		CaptureNetworkSyncState(*SyncState);
	}
	if (AuxState && CachedStatsComponent)
	{
		AuxState->FlightStats = CachedStatsComponent->GetFlightStats();
	}
}

void UArcadeFlightComponent::CaptureNetworkSyncState(
	FPlaneFlightSyncState& State) const
{
	if (const AActor* Owner = GetOwner())
	{
		State.Location = Owner->GetActorLocation();
		State.Rotation = Owner->GetActorRotation().GetNormalized();
	}
	State.Velocity = CurrentVelocity;
	State.SmoothedSteering = SmoothedSteering;
	State.SmoothedStrafe = SmoothedStrafe;
	State.SmoothedBrake = SmoothedBrake;
	State.ForwardSpeed = CurrentForwardSpeed;
	State.VisualBankDegrees = CurrentVisualBankDegrees;
	State.BoostEnergy = CurrentBoostEnergy;
	State.BoostAlpha = CurrentBoostAlpha;
	State.BoostRegenDelayRemaining = BoostRegenDelayRemaining;
	State.bBoosting = bCurrentBoosting;
	State.HoverPresentationAlpha = HoverPresentationAlpha;
	State.HoverState = static_cast<uint8>(HoverState);
	State.LastProcessedAbilitySequence = LastProcessedAbilitySequence;
	State.ActiveMobilityAbility = static_cast<uint8>(ActiveMobilityAbility);
	State.MobilityElapsed = MobilityElapsed;
	State.MobilityDirection = MobilityDirection;
	State.MobilityInitialRotation = MobilityInitialRotation;
	State.MobilityInitialForwardVelocity = MobilityInitialForwardVelocity;
	State.MobilityLateralVelocity = MobilityLateralVelocity;
	State.MobilityPreviousPathOffset = MobilityPreviousPathOffset;
	State.MobilityInitialForwardSpeed = MobilityInitialForwardSpeed;
	State.bEvasiveRollActive = bEvasiveRollActive;
	State.EvasiveRollElapsed = EvasiveRollElapsed;
	State.EvasiveRollDirection = EvasiveRollDirection;
	State.EvasiveRollCooldownRemaining = EvasiveRollCooldownRemaining;
	State.QuickReversalCooldownRemaining = QuickReversalCooldownRemaining;
	State.BackwardDashCooldownRemaining = BackwardDashCooldownRemaining;
	State.ForwardDashCooldownRemaining = ForwardDashCooldownRemaining;
	State.BlinkCooldownRemaining = BlinkCooldownRemaining;
	State.BlinkCameraRecoveryRemaining = BlinkCameraRecoveryRemaining;
	State.QueuedAbilityType = static_cast<uint8>(QueuedAbilityType);
	State.QueuedAbilityDirection = QueuedAbilityDirection;
	State.QueuedAbilityMovementInput = QueuedAbilityMovementInput;
	State.QueuedAbilitySequence = QueuedAbilitySequence;
	State.LastActivatedAbilitySequence = LastActivatedAbilitySequence;
	State.LastActivatedAbilityType = static_cast<uint8>(LastActivatedAbilityType);
	State.LastBlinkDeparture = LastBlinkDeparture;
	State.LastBlinkArrival = LastBlinkArrival;
	State.bBoundaryReturnActive = bBoundaryReturnActive;
	State.BoundaryReturnTarget = BoundaryReturnTarget;
	State.BoundaryTurnVisualInput = BoundaryTurnVisualInput;
	State.MovementEpoch = MovementEpoch;
}

void UArcadeFlightComponent::ApplyNetworkSyncState(
	const FPlaneFlightSyncState& State, const ETeleportType TeleportType,
	const ENetworkStateApplication Application)
{
	AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner());
	AActor* Owner = GetOwner();
	const bool bRemotePresentation = Plane && Owner
		&& !Owner->HasAuthority() && !Plane->IsLocallyControlled();
	if (bRemotePresentation && Application == ENetworkStateApplication::Presentation)
	{
		// Network Prediction supplies an already interpolated pose here. Keep the
		// actor at its logical simulated state and move only the visible assembly.
		// Applying this pose to the actor as well caused a logical/presentation
		// ping-pong every frame, most visible when lateral velocity changed on A/D.
		Plane->SetRemoteNetworkPresentationTransform(State.Location, State.Rotation);
		Plane->SetVisualBank(State.VisualBankDegrees);
		return;
	}
	if (Owner)
	{
		const bool bTransformDiffers = !Owner->GetActorLocation().Equals(State.Location, 0.01f)
			|| !Owner->GetActorRotation().Equals(State.Rotation, 0.01f);
		if (bTransformDiffers)
		{
			Owner->SetActorLocationAndRotation(
				State.Location, State.Rotation, false, nullptr, TeleportType);
		}
	}
	CurrentVelocity = State.Velocity;
	SmoothedSteering = State.SmoothedSteering;
	SmoothedStrafe = State.SmoothedStrafe;
	SmoothedBrake = State.SmoothedBrake;
	CurrentForwardSpeed = State.ForwardSpeed;
	CurrentVisualBankDegrees = State.VisualBankDegrees;
	CurrentBoostEnergy = State.BoostEnergy;
	CurrentBoostAlpha = State.BoostAlpha;
	BoostRegenDelayRemaining = State.BoostRegenDelayRemaining;
	bCurrentBoosting = State.bBoosting;
	HoverPresentationAlpha = State.HoverPresentationAlpha;
	HoverState = static_cast<EArcadeHoverState>(State.HoverState);
	LastProcessedAbilitySequence = State.LastProcessedAbilitySequence;
	ActiveMobilityAbility = static_cast<EPlaneAbilityType>(State.ActiveMobilityAbility);
	MobilityElapsed = State.MobilityElapsed;
	MobilityDirection = State.MobilityDirection;
	MobilityInitialRotation = State.MobilityInitialRotation;
	MobilityInitialForwardVelocity = State.MobilityInitialForwardVelocity;
	MobilityLateralVelocity = State.MobilityLateralVelocity;
	MobilityPreviousPathOffset = State.MobilityPreviousPathOffset;
	MobilityInitialForwardSpeed = State.MobilityInitialForwardSpeed;
	bEvasiveRollActive = State.bEvasiveRollActive;
	EvasiveRollElapsed = State.EvasiveRollElapsed;
	EvasiveRollDirection = State.EvasiveRollDirection;
	EvasiveRollCooldownRemaining = State.EvasiveRollCooldownRemaining;
	QuickReversalCooldownRemaining = State.QuickReversalCooldownRemaining;
	BackwardDashCooldownRemaining = State.BackwardDashCooldownRemaining;
	ForwardDashCooldownRemaining = State.ForwardDashCooldownRemaining;
	BlinkCooldownRemaining = State.BlinkCooldownRemaining;
	BlinkCameraRecoveryRemaining = State.BlinkCameraRecoveryRemaining;
	QueuedAbilityType = static_cast<EPlaneAbilityType>(State.QueuedAbilityType);
	QueuedAbilityDirection = State.QueuedAbilityDirection;
	QueuedAbilityMovementInput = State.QueuedAbilityMovementInput;
	QueuedAbilitySequence = State.QueuedAbilitySequence;
	LastActivatedAbilitySequence = State.LastActivatedAbilitySequence;
	LastActivatedAbilityType = static_cast<EPlaneAbilityType>(State.LastActivatedAbilityType);
	LastBlinkDeparture = State.LastBlinkDeparture;
	LastBlinkArrival = State.LastBlinkArrival;
	bBoundaryReturnActive = State.bBoundaryReturnActive;
	BoundaryReturnTarget = State.BoundaryReturnTarget;
	BoundaryTurnVisualInput = State.BoundaryTurnVisualInput;
	MovementEpoch = State.MovementEpoch;
	if (Plane)
	{
		Plane->SetVisualBank(CurrentVisualBankDegrees);
	}
	PlayAbilityEffectIfNeeded();
}

void UArcadeFlightComponent::RestoreFrame(
	const FPlaneFlightSyncState* SyncState, const FPlaneFlightAuxState* AuxState)
{
	if (SyncState)
	{
		ApplyNetworkSyncState(*SyncState, ETeleportType::TeleportPhysics,
			ENetworkStateApplication::Restore);
	}
}

void UArcadeFlightComponent::FinalizeFrame(
	const FPlaneFlightSyncState* SyncState, const FPlaneFlightAuxState* AuxState)
{
	if (SyncState)
	{
		ApplyNetworkSyncState(*SyncState, ETeleportType::TeleportPhysics,
			ENetworkStateApplication::Finalize);
	}
}

void UArcadeFlightComponent::FinalizeSmoothingFrame(
	const FPlaneFlightSyncState* SyncState, const FPlaneFlightAuxState* AuxState)
{
	if (SyncState)
	{
		// FixedTickSmoothing calculates an interpolated presentation state every
		// rendered frame. Without this callback the result is never applied and the
		// aircraft remains visibly stepped at the 60 Hz simulation rate.
		ApplyNetworkSyncState(
			*SyncState, ETeleportType::TeleportPhysics,
			ENetworkStateApplication::Presentation);
	}
}

void UArcadeFlightComponent::RunNetworkSimulationStep(
	const float DeltaTime,
	const FPlaneFlightInputCmd& InputCmd,
	const FPlaneFlightSyncState& InputState,
	const FPlaneFlightAuxState& AuxState,
	FPlaneFlightSyncState& OutputState)
{
	if (!GetOwner() || DeltaTime <= UE_SMALL_NUMBER)
	{
		OutputState = InputState;
		return;
	}

	ApplyNetworkSyncState(InputState, ETeleportType::TeleportPhysics,
		ENetworkStateApplication::Simulation);
	RawSteering = InputCmd.Steering;
	RawStrafe = InputCmd.Strafe;
	RawBrake = InputCmd.Brake;
	bRawBoostRequested = InputCmd.bBoostRequested;
	bRawHoverRequested = InputCmd.bHoverRequested;
	RawAbilitySequence = InputCmd.AbilitySequence;
	RawAbilityType = static_cast<EPlaneAbilityType>(InputCmd.AbilityType);
	RawAbilityDirection = InputCmd.AbilityDirection;
	RawAbilityMovementInput = InputCmd.AbilityMovementInput;

	SimulateFlight(DeltaTime, AuxState.FlightStats);
	CaptureNetworkSyncState(OutputState);
}

bool UArcadeFlightComponent::CanBeginHoverEntry() const
{
	const EPlaneAbilityType QueuedType =
		static_cast<EPlaneAbilityType>(QueuedAbilityType);
	const bool bNonRollAbilityQueued = QueuedType != EPlaneAbilityType::None
		&& QueuedType != EPlaneAbilityType::EvasiveRollLeft
		&& QueuedType != EPlaneAbilityType::EvasiveRollRight;
	return !bBoundaryReturnActive
		&& ActiveMobilityAbility == EPlaneAbilityType::None
		&& !bNonRollAbilityQueued
		&& BlinkCameraRecoveryRemaining <= UE_SMALL_NUMBER;
}

bool UArcadeFlightComponent::ToggleHover()
{
	// Boundary return owns the aircraft until it is safely back inside. Reject
	// new hover requests locally instead of letting prediction start an entry.
	if (!bLocalHoverInputRequested && !CanBeginHoverEntry())
	{
		return false;
	}

	bLocalHoverInputRequested = !bLocalHoverInputRequested;
	bRawHoverRequested = bLocalHoverInputRequested;
	if (bRawHoverRequested && HoverState == EArcadeHoverState::Flying)
	{
		// Lock combat for the fraction of a frame before the next predicted step.
		bLocalHoverEntryPending = true;
	}
	return true;
}

void UArcadeFlightComponent::RequestExitHover()
{
	bLocalHoverInputRequested = false;
	bRawHoverRequested = false;
	bLocalHoverEntryPending = false;
}

void UArcadeFlightComponent::CancelHoverEntryFromCollision()
{
	bRawHoverRequested = false;
	bLocalHoverEntryPending = false;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		bLocalHoverInputRequested = false;
	}
	if (HoverState == EArcadeHoverState::Entering)
	{
		SetSimulationHoverState(EArcadeHoverState::Leaving);
	}
}

void UArcadeFlightComponent::SetSimulationHoverState(const EArcadeHoverState NewState)
{
	if (HoverState == NewState)
	{
		return;
	}
	HoverState = NewState;
}

void UArcadeFlightComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bLocalHoverEntryPending && HoverState != EArcadeHoverState::Flying)
	{
		bLocalHoverEntryPending = false;
	}
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UpdateBoundaryWarning();
	const UJetStatsComponent* StatsComponent = CachedStatsComponent;
	if (GetOwner() && GetOwner()->HasAuthority() && StatsComponent)
	{
		const FPlaneFlightAuxState* CurrentAux =
			NetworkPredictionProxy.ReadAuxState<FPlaneFlightAuxState>();
		FPlaneFlightAuxState DesiredAux;
		DesiredAux.FlightStats = StatsComponent->GetFlightStats();
		if (!CurrentAux || DesiredAux.ShouldReconcile(*CurrentAux))
		{
			NetworkPredictionProxy.WriteAuxState<FPlaneFlightAuxState>(
				[&DesiredAux](FPlaneFlightAuxState& State)
				{
					State = DesiredAux;
				}, "FlightStatsChanged");
		}
	}
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (bBoundaryReturnActive && bLocalHoverInputRequested)
		{
			RequestExitHover();
		}
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			Plane->UpdateCameraAfterFlight(DeltaTime);
		}
	}
}

void UArcadeFlightComponent::UpdateHoverPresentation(const float DeltaTime, const FJetFlightStats& Stats)
{
	const bool bHoverActive = HoverState == EArcadeHoverState::Entering
		|| HoverState == EArcadeHoverState::Hovering;
	HoverPresentationAlpha = FMath::FInterpTo(HoverPresentationAlpha, bHoverActive ? 1.0f : 0.0f,
		DeltaTime, Stats.HoverRotationResponse);
}

void UArcadeFlightComponent::UpdateSmoothedInput(
	const float DeltaTime, const FJetFlightStats& Stats)
{
	SmoothedSteering.X = FMath::FInterpConstantTo(
		SmoothedSteering.X, RawSteering.X, DeltaTime, ArcadeFlightTuning::SteeringResponseSpeed);
	SmoothedSteering.Y = FMath::FInterpConstantTo(
		SmoothedSteering.Y, RawSteering.Y, DeltaTime, ArcadeFlightTuning::SteeringResponseSpeed);

	// Arcade lateral control: A/D engages quickly, releases slowly and a reversal
	// always travels linearly through zero instead of snapping between bank directions.
	const bool bStrafePressed = FMath::Abs(RawStrafe) > KINDA_SMALL_NUMBER;
	SmoothedStrafe = FMath::FInterpConstantTo(
		SmoothedStrafe,
		bStrafePressed ? RawStrafe : 0.0f,
		DeltaTime,
		bStrafePressed ? ArcadeFlightTuning::StrafeEngageSpeed : ArcadeFlightTuning::StrafeReleaseSpeed);
	SmoothedBrake = FMath::FInterpTo(
		SmoothedBrake, RawBrake, DeltaTime, Stats.BrakeInputResponseSpeed);
}

void UArcadeFlightComponent::SimulateFlight(
	const float DeltaTime, const FJetFlightStats& Stats)
{
	UpdateBoundaryState();
	UpdatePredictedHoverRequest();
	UpdatePredictedAbilities(DeltaTime, Stats);
	UpdateHoverPresentation(DeltaTime, Stats);
	UpdateSmoothedInput(DeltaTime, Stats);
	UpdatePredictedBoost(DeltaTime, Stats);
	RotateAircraft(DeltaTime, Stats);
	UpdateVisualBank(DeltaTime);
	MoveAircraft(DeltaTime, Stats);
}

void UArcadeFlightComponent::UpdatePredictedAbilities(
	const float DeltaTime, const FJetFlightStats& Stats)
{
	EvasiveRollCooldownRemaining = FMath::Max(0.0f,
		EvasiveRollCooldownRemaining - DeltaTime);
	QuickReversalCooldownRemaining = FMath::Max(0.0f,
		QuickReversalCooldownRemaining - DeltaTime);
	BackwardDashCooldownRemaining = FMath::Max(0.0f,
		BackwardDashCooldownRemaining - DeltaTime);
	ForwardDashCooldownRemaining = FMath::Max(0.0f,
		ForwardDashCooldownRemaining - DeltaTime);
	BlinkCooldownRemaining = FMath::Max(0.0f, BlinkCooldownRemaining - DeltaTime);
	BlinkCameraRecoveryRemaining = FMath::Max(0.0f,
		BlinkCameraRecoveryRemaining - DeltaTime);

	if (bEvasiveRollActive)
	{
		EvasiveRollElapsed += DeltaTime;
		if (EvasiveRollElapsed >= ArcadeFlightTuning::EvasiveRollDuration)
		{
			bEvasiveRollActive = false;
			EvasiveRollElapsed = ArcadeFlightTuning::EvasiveRollDuration;
		}
	}
	if (ActiveMobilityAbility != EPlaneAbilityType::None)
	{
		MobilityElapsed += DeltaTime;
	}

	if (RawAbilitySequence != 0
		&& RawAbilitySequence != LastProcessedAbilitySequence)
	{
		ProcessAbilityRequest(RawAbilitySequence, RawAbilityType,
			RawAbilityDirection, RawAbilityMovementInput, Stats);
	}

	if (QueuedAbilityType != EPlaneAbilityType::None
		&& !IsAbilityBlocked(QueuedAbilityType))
	{
		const uint16 Sequence = QueuedAbilitySequence;
		const EPlaneAbilityType Type = QueuedAbilityType;
		const float Direction = QueuedAbilityDirection;
		const FVector2D MovementInput = QueuedAbilityMovementInput;
		QueuedAbilityType = EPlaneAbilityType::None;
		QueuedAbilitySequence = 0;
		TryStartAbility(Sequence, Type, Direction, MovementInput, Stats);
	}
}

void UArcadeFlightComponent::ProcessAbilityRequest(
	const uint16 Sequence, const EPlaneAbilityType Type, const float Direction,
	const FVector2D& MovementInput, const FJetFlightStats& Stats)
{
	LastProcessedAbilitySequence = Sequence;
	if (Type == EPlaneAbilityType::None
		|| HoverState != EArcadeHoverState::Flying || bBoundaryReturnActive)
	{
		return;
	}
	if (IsAbilityBlocked(Type))
	{
		QueuedAbilityType = Type;
		QueuedAbilityDirection = Direction;
		QueuedAbilityMovementInput = MovementInput;
		QueuedAbilitySequence = Sequence;
		return;
	}
	TryStartAbility(Sequence, Type, Direction, MovementInput, Stats);
}

bool UArcadeFlightComponent::IsAbilityBlocked(const EPlaneAbilityType Type) const
{
	if (ActiveMobilityAbility == EPlaneAbilityType::QuickReversal)
	{
		return true;
	}
	switch (Type)
	{
	case EPlaneAbilityType::EvasiveRollLeft:
	case EPlaneAbilityType::EvasiveRollRight:
		return bEvasiveRollActive;
	case EPlaneAbilityType::QuickReversal:
		return bEvasiveRollActive || ActiveMobilityAbility != EPlaneAbilityType::None;
	case EPlaneAbilityType::BackwardDash:
	case EPlaneAbilityType::ForwardDash:
	case EPlaneAbilityType::Blink:
		return ActiveMobilityAbility != EPlaneAbilityType::None;
	default:
		return false;
	}
}

bool UArcadeFlightComponent::TryStartAbility(
	const uint16 Sequence, const EPlaneAbilityType Type, const float Direction,
	const FVector2D& MovementInput, const FJetFlightStats& Stats)
{
	switch (Type)
	{
	case EPlaneAbilityType::EvasiveRollLeft:
	case EPlaneAbilityType::EvasiveRollRight:
		if (EvasiveRollCooldownRemaining > UE_SMALL_NUMBER || bEvasiveRollActive)
		{
			return false;
		}
		bEvasiveRollActive = true;
		EvasiveRollElapsed = 0.0f;
		EvasiveRollDirection = Type == EPlaneAbilityType::EvasiveRollLeft ? -1.0f : 1.0f;
		EvasiveRollCooldownRemaining = ArcadeFlightTuning::EvasiveRollCooldown;
		break;
	case EPlaneAbilityType::QuickReversal:
		if (QuickReversalCooldownRemaining > UE_SMALL_NUMBER)
		{
			return false;
		}
		StartMobilityAbility(Sequence, Type, Direction, Stats);
		QuickReversalCooldownRemaining = ArcadeFlightTuning::QuickReversalCooldown;
		break;
	case EPlaneAbilityType::BackwardDash:
		if (BackwardDashCooldownRemaining > UE_SMALL_NUMBER)
		{
			return false;
		}
		StartMobilityAbility(Sequence, Type, Direction, Stats);
		BackwardDashCooldownRemaining = ArcadeFlightTuning::BackwardCooldown;
		break;
	case EPlaneAbilityType::ForwardDash:
		if (ForwardDashCooldownRemaining > UE_SMALL_NUMBER)
		{
			return false;
		}
		StartMobilityAbility(Sequence, Type, Direction, Stats);
		ForwardDashCooldownRemaining = ArcadeFlightTuning::ForwardDashCooldown;
		break;
	case EPlaneAbilityType::Blink:
		if (BlinkCooldownRemaining > UE_SMALL_NUMBER)
		{
			return false;
		}
		ExecuteBlink(Sequence, MovementInput);
		BlinkCooldownRemaining = ArcadeFlightTuning::BlinkCooldown;
		break;
	default:
		return false;
	}

	LastActivatedAbilitySequence = Sequence;
	LastActivatedAbilityType = Type;
	return true;
}

void UArcadeFlightComponent::StartMobilityAbility(
	const uint16 Sequence, const EPlaneAbilityType Type, const float Direction,
	const FJetFlightStats& Stats)
{
	ActiveMobilityAbility = Type;
	MobilityElapsed = 0.0f;
	MobilityDirection = Direction < -0.05f ? -1.0f : 1.0f;
	MobilityInitialRotation = GetOwner()->GetActorRotation();
	const FVector InitialForward = MobilityInitialRotation.Vector();
	MobilityInitialForwardVelocity = InitialForward
		* FVector::DotProduct(CurrentVelocity, InitialForward);
	MobilityLateralVelocity = CurrentVelocity - MobilityInitialForwardVelocity;
	MobilityPreviousPathOffset = FVector::ZeroVector;
	MobilityInitialForwardSpeed = FMath::Max(0.0f,
		FVector::DotProduct(CurrentVelocity, InitialForward));
	if (MobilityInitialForwardVelocity.IsNearlyZero())
	{
		MobilityInitialForwardVelocity = InitialForward * CurrentForwardSpeed;
	}
}

void UArcadeFlightComponent::ExecuteBlink(
	const uint16 Sequence, const FVector2D& MovementInput)
{
	AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner());
	if (!Plane)
	{
		return;
	}
	FVector2D SafeInput(
		FMath::Clamp(MovementInput.X, -1.0f, 1.0f),
		FMath::Clamp(MovementInput.Y, -1.0f, 1.0f));
	if (SafeInput.IsNearlyZero())
	{
		SafeInput.X = 1.0f;
	}
	SafeInput.Normalize();
	const FVector Direction = (
		GetOwner()->GetActorForwardVector() * SafeInput.X
		+ GetOwner()->GetActorRightVector() * SafeInput.Y).GetSafeNormal();
	LastBlinkDeparture = GetOwner()->GetActorLocation();
	Plane->MovePlaneWithCollision(Direction * ArcadeFlightTuning::BlinkDistance,
		Direction * CurrentForwardSpeed, false);
	LastBlinkArrival = GetOwner()->GetActorLocation();
	BlinkCameraRecoveryRemaining = ArcadeFlightTuning::BlinkCameraRecoveryDuration;
	++MovementEpoch;
}

void UArcadeFlightComponent::FinishMobilityAbility()
{
	if (ActiveMobilityAbility == EPlaneAbilityType::QuickReversal)
	{
		SmoothedSteering = FVector2D::ZeroVector;
	}
	ActiveMobilityAbility = EPlaneAbilityType::None;
	MobilityElapsed = 0.0f;
}

void UArcadeFlightComponent::PlayAbilityEffectIfNeeded()
{
	if (LastActivatedAbilitySequence == 0
		|| LastActivatedAbilitySequence == LastPresentedAbilitySequence)
	{
		return;
	}
	LastPresentedAbilitySequence = LastActivatedAbilitySequence;

	switch (LastActivatedAbilityType)
	{
	case EPlaneAbilityType::QuickReversal:
		if (CachedQuickReversalComponent)
		{
			CachedQuickReversalComponent->PlayPredictedActivationEffect();
		}
		break;
	case EPlaneAbilityType::BackwardDash:
		if (CachedBackwardDashComponent)
		{
			CachedBackwardDashComponent->PlayPredictedActivationEffect();
		}
		break;
	case EPlaneAbilityType::ForwardDash:
		if (CachedForwardDashComponent)
		{
			CachedForwardDashComponent->PlayPredictedActivationEffect();
		}
		break;
	case EPlaneAbilityType::Blink:
		if (CachedBlinkComponent)
		{
			CachedBlinkComponent->PlayPredictedActivationEffect(
				LastBlinkDeparture, LastBlinkArrival);
		}
		break;
	default:
		break;
	}
}

FQuat UArcadeFlightComponent::BuildQuickReversalRotation(const float Progress) const
{
	const float FlipAlpha = FMath::SmoothStep(
		ArcadeFlightTuning::QuickRotationStart,
		ArcadeFlightTuning::QuickRotationEnd, Progress);
	const FQuat ForwardFlip(FVector::RightVector,
		FMath::DegreesToRadians(180.0f * FlipAlpha));
	const float UprightAlpha = FMath::SmoothStep(
		ArcadeFlightTuning::QuickUprightStart,
		ArcadeFlightTuning::QuickUprightEnd, Progress);
	const FQuat UprightRoll(FVector::ForwardVector,
		FMath::DegreesToRadians(180.0f * MobilityDirection * UprightAlpha));
	return (MobilityInitialRotation.Quaternion()
		* ForwardFlip * UprightRoll).GetNormalized();
}

FVector UArcadeFlightComponent::BuildQuickReversalArcOffset(const float Progress) const
{
	const float FlipAlpha = FMath::SmoothStep(
		ArcadeFlightTuning::QuickRotationStart,
		ArcadeFlightTuning::QuickRotationEnd, Progress);
	const FQuat InitialRotation = MobilityInitialRotation.Quaternion();
	const FVector PivotOffset = InitialRotation.GetForwardVector()
		* ArcadeFlightTuning::QuickArcRadius;
	const FQuat ArcRotation(InitialRotation.GetRightVector(),
		FMath::DegreesToRadians(180.0f * FlipAlpha));
	return PivotOffset + ArcRotation.RotateVector(-PivotOffset);
}

void UArcadeFlightComponent::UpdatePredictedHoverRequest()
{
	// Enforce the rule inside deterministic simulation as well. This keeps the
	// server authoritative and also covers crossing the boundary on the same
	// frame in which the player requested hover.
	if (bBoundaryReturnActive)
	{
		bRawHoverRequested = false;
	}

	if (bRawHoverRequested)
	{
		if (HoverState == EArcadeHoverState::Flying
			|| HoverState == EArcadeHoverState::Leaving)
		{
			SetSimulationHoverState(EArcadeHoverState::Entering);
		}
	}
	else if (HoverState == EArcadeHoverState::Entering
		|| HoverState == EArcadeHoverState::Hovering)
	{
		SetSimulationHoverState(EArcadeHoverState::Leaving);
	}
}

void UArcadeFlightComponent::UpdatePredictedBoost(
	const float DeltaTime, const FJetFlightStats& Stats)
{
	const bool bForwardDashActive = IsForwardDashActive();
	const bool bMobilityOverride = bForwardDashActive
		|| IsBackwardDashActive() || IsQuickReversalActive();
	const bool bCanBoost = HoverState == EArcadeHoverState::Flying
		&& !bMobilityOverride && bRawBoostRequested
		&& CurrentBoostEnergy > KINDA_SMALL_NUMBER;
	bCurrentBoosting = bCanBoost;

	if (bCurrentBoosting)
	{
		CurrentBoostEnergy = FMath::Max(
			0.0f, CurrentBoostEnergy - Stats.BoostEnergyDrainPerSecond * DeltaTime);
		BoostRegenDelayRemaining = Stats.BoostRegenDelay;
		if (CurrentBoostEnergy <= KINDA_SMALL_NUMBER)
		{
			bCurrentBoosting = false;
		}
	}
	else if (!bForwardDashActive)
	{
		BoostRegenDelayRemaining = FMath::Max(
			0.0f, BoostRegenDelayRemaining - DeltaTime);
		if (BoostRegenDelayRemaining <= UE_SMALL_NUMBER)
		{
			CurrentBoostEnergy = FMath::Min(
				Stats.MaxBoostEnergy,
				CurrentBoostEnergy + Stats.BoostEnergyRegenPerSecond * DeltaTime);
		}
	}

	CurrentBoostAlpha = FMath::FInterpTo(
		CurrentBoostAlpha, bCurrentBoosting ? 1.0f : 0.0f,
		DeltaTime, Stats.BoostResponseSpeed);
}

void UArcadeFlightComponent::UpdateBoundaryState()
{
	if (!CachedFlightBounds || !GetOwner())
	{
		return;
	}

	float IgnoredWarningAlpha = 0.0f;
	bool bOutside = false;
	CachedFlightBounds->GetBoundaryInfo(
		GetOwner()->GetActorLocation(), IgnoredWarningAlpha, bOutside);

	if (!bBoundaryReturnActive && bOutside)
	{
		bBoundaryReturnActive = true;
	}
	else if (bBoundaryReturnActive
		&& CachedFlightBounds->IsSafelyInside(GetOwner()->GetActorLocation()))
	{
		bBoundaryReturnActive = false;
		BoundaryTurnVisualInput = 0.0f;
		SmoothedSteering = FVector2D::ZeroVector;
	}
	if (bBoundaryReturnActive)
	{
		BoundaryReturnTarget = CachedFlightBounds->GetReturnTarget(
			GetOwner()->GetActorLocation());
	}
}

void UArcadeFlightComponent::UpdateBoundaryWarning()
{
	BoundaryWarningAlpha = 0.0f;
	if (!CachedFlightBounds || !GetOwner())
	{
		return;
	}
	bool bOutside = false;
	CachedFlightBounds->GetBoundaryInfo(
		GetOwner()->GetActorLocation(), BoundaryWarningAlpha, bOutside);
}

void UArcadeFlightComponent::NotifyAuthoritativeTeleport()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	++MovementEpoch;
	NetworkPredictionProxy.WriteSyncState<FPlaneFlightSyncState>(
		[this](FPlaneFlightSyncState& State)
		{
			CaptureNetworkSyncState(State);
		}, "AuthoritativeTeleport");
	GetOwner()->ForceNetUpdate();
}

void UArcadeFlightComponent::PrepareStraightManeuverExit()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	SmoothedSteering = FVector2D::ZeroVector;
	NetworkPredictionProxy.WriteSyncState<FPlaneFlightSyncState>(
		[](FPlaneFlightSyncState& State)
		{
			State.SmoothedSteering = FVector2D::ZeroVector;
		}, "StraightManeuverExit");
}

void UArcadeFlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UArcadeFlightComponent::RotateAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
	const FRotator CurrentRotation = GetOwner()->GetActorRotation();
	if (bBoundaryReturnActive && CachedFlightBounds)
	{
		const FVector ToTarget = BoundaryReturnTarget - GetOwner()->GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			FRotator TargetRotation = ToTarget.Rotation();
			TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch, -Stats.MaxPitch, Stats.MaxPitch);
			TargetRotation.Roll = 0.0f;
			const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw);
			BoundaryTurnVisualInput = FMath::Clamp(
				DeltaYaw / FMath::Max(1.0f, CachedFlightBounds->GetReturnTurnRate()), -1.0f, 1.0f);
			GetOwner()->SetActorRotation(FMath::RInterpConstantTo(
				CurrentRotation, TargetRotation, DeltaTime,
				CachedFlightBounds->GetReturnTurnRate()));
		}
		return;
	}
	if (IsQuickReversalActive())
	{
		GetOwner()->SetActorRotation(BuildQuickReversalRotation(GetMobilityProgress()));
		return;
	}
	if (IsBackwardDashActive() || IsForwardDashActive())
	{
		GetOwner()->SetActorRotation(MobilityInitialRotation);
		return;
	}
	if (HoverState != EArcadeHoverState::Flying)
	{
		const float TargetPitch = HoverState == EArcadeHoverState::Leaving ? 0.0f : Stats.HoverPitch;
		const float RotationResponse = HoverState == EArcadeHoverState::Leaving
			? Stats.HoverRotationResponse * ArcadeFlightTuning::HoverExitRotationResponseScale
			: Stats.HoverRotationResponse;
		const float NewPitch = HoverState == EArcadeHoverState::Hovering
			? Stats.HoverPitch
			: FMath::FInterpTo(CurrentRotation.Pitch, TargetPitch, DeltaTime, RotationResponse);
		const float NewYaw = CurrentRotation.Yaw;
		const FRotator ProposedRotation(NewPitch, NewYaw, 0.0f);
		if (HoverState == EArcadeHoverState::Entering)
		{
			if (const AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner());
				Plane && Plane->WouldPlaneRotationCollide(ProposedRotation))
			{
				CancelHoverEntryFromCollision();
				return;
			}
		}
		GetOwner()->SetActorRotation(ProposedRotation);
		if (HoverState == EArcadeHoverState::Leaving
			&& FMath::Abs(NewPitch) <= ArcadeFlightTuning::HoverExitControlPitchTolerance)
		{
			GetOwner()->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
			SetSimulationHoverState(EArcadeHoverState::Flying);
		}
		return;
	}
	const float TurnRateMultiplier = CalculateTurnRateMultiplier(Stats)
		* ArcadeFlightTuning::TurnRateScale;

	const float NewYaw = CurrentRotation.Yaw
		+ SmoothedSteering.X * Stats.MaxYawTurnRate * TurnRateMultiplier * DeltaTime;
	const float NewPitch = FMath::Clamp(
		CurrentRotation.Pitch + SmoothedSteering.Y * Stats.MaxPitchTurnRate
			* TurnRateMultiplier * DeltaTime,
		-Stats.MaxPitch, Stats.MaxPitch);

	// The actor is the invisible flight model and never receives visual bank.
	GetOwner()->SetActorRotation(FRotator(NewPitch, NewYaw, 0.0f));
}

void UArcadeFlightComponent::UpdateVisualBank(const float DeltaTime)
{
	AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner());
	if (!Plane || DeltaTime <= UE_SMALL_NUMBER)
	{
		return;
	}
	const float FlightAlpha = 1.0f - FMath::Clamp(HoverPresentationAlpha, 0.0f, 1.0f);
	// Keep the visual roll as its own persistent state. The steering/strafe values
	// choose an unclamped destination, but the model only travels a frame-sized
	// fraction toward it. Clamping happens afterwards. This is important: changing
	// direction must carry the existing bank smoothly through zero instead of
	// replacing it with the new side's angle in one frame.
	const float EffectiveTurnInput = bBoundaryReturnActive
		? BoundaryTurnVisualInput : SmoothedSteering.X;
	const float TurnBankTarget = FMath::RadiansToDegrees(
		2.0f
		* ArcadeFlightTuning::CursorSideRateMax
		* ArcadeFlightTuning::VisualBankControlRate
		* -EffectiveTurnInput);
	const float StrafeBankTarget = FMath::RadiansToDegrees(
		ArcadeFlightTuning::VisualBankControlRate * -SmoothedStrafe);
	const bool bStrafeInfluencesBank = !bBoundaryReturnActive
		&& (FMath::Abs(RawStrafe) > KINDA_SMALL_NUMBER
		|| FMath::Abs(SmoothedStrafe) > 0.01f);
	const float TargetBankDegrees =
		(bStrafeInfluencesBank ? StrafeBankTarget : TurnBankTarget) * FlightAlpha;
	CurrentVisualBankDegrees += (TargetBankDegrees - CurrentVisualBankDegrees)
		* FMath::Clamp(DeltaTime, 0.0f, 1.0f);
	CurrentVisualBankDegrees = FMath::Clamp(
		CurrentVisualBankDegrees,
		-ArcadeFlightTuning::MaxVisualBankDegrees,
		ArcadeFlightTuning::MaxVisualBankDegrees);
	Plane->SetVisualBank(CurrentVisualBankDegrees);
}

float UArcadeFlightComponent::CalculateTurnRateMultiplier(const FJetFlightStats& Stats) const
{
	const float MinimumSpeed = FMath::Min(Stats.MinForwardSpeed, Stats.ForwardSpeed);
	const float MaximumSpeed = Stats.ForwardSpeed * Stats.BoostSpeedMultiplier;
	float TurnRateMultiplier = 1.0f;
	if (CurrentForwardSpeed < Stats.ForwardSpeed)
	{
		const float NormalSpeedAlpha = FMath::GetMappedRangeValueClamped(
			FVector2D(MinimumSpeed, Stats.ForwardSpeed), FVector2D(0.0f, 1.0f), CurrentForwardSpeed);
		TurnRateMultiplier = FMath::Lerp(Stats.MinSpeedTurnMultiplier, 1.0f, NormalSpeedAlpha);
	}
	else if (MaximumSpeed > Stats.ForwardSpeed + KINDA_SMALL_NUMBER)
	{
		const float BoostSpeedAlpha = FMath::GetMappedRangeValueClamped(
			FVector2D(Stats.ForwardSpeed, MaximumSpeed), FVector2D(0.0f, 1.0f), CurrentForwardSpeed);
		TurnRateMultiplier = FMath::Lerp(1.0f, Stats.BoostSpeedTurnMultiplier, BoostSpeedAlpha);
	}
	return TurnRateMultiplier;
}

float UArcadeFlightComponent::GetCurrentTurnRateMultiplier() const
{
	return CachedStatsComponent
		? CalculateTurnRateMultiplier(CachedStatsComponent->GetFlightStats()) * ArcadeFlightTuning::TurnRateScale
		: ArcadeFlightTuning::TurnRateScale;
}

void UArcadeFlightComponent::MoveAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
	const float MaximumStrafeSpeed = Stats.ForwardSpeed * Stats.StrafeSpeedMultiplier;
	const FVector DesiredLateralVelocity = GetOwner()->GetActorRightVector()
		* SmoothedStrafe * MaximumStrafeSpeed;
	const auto ApplyManeuverVelocity = [this, DeltaTime](const FVector& Velocity)
	{
		CurrentVelocity = Velocity;
		const FVector RequestedMove = Velocity * DeltaTime;
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			CurrentVelocity = Plane->MovePlaneWithCollision(RequestedMove, Velocity);
		}
		else
		{
			GetOwner()->AddActorWorldOffset(
				RequestedMove, false, nullptr, ETeleportType::None);
		}
		CurrentForwardSpeed = FVector::DotProduct(
			CurrentVelocity, GetOwner()->GetActorForwardVector());
	};

	if (IsForwardDashActive())
	{
		const float AccelerationProgress = FMath::Clamp(
			MobilityElapsed / ArcadeFlightTuning::ForwardDashAccelerationDuration,
			0.0f, 1.0f);
		const float Speed = FMath::Lerp(
			MobilityInitialForwardSpeed,
			Stats.ForwardSpeed * ArcadeFlightTuning::ForwardDashSpeedMultiplier,
			FMath::SmoothStep(0.0f, 1.0f, AccelerationProgress));
		ApplyManeuverVelocity(MobilityInitialRotation.Vector() * Speed);
		if (MobilityElapsed >= ArcadeFlightTuning::ForwardDashDuration)
		{
			FinishMobilityAbility();
		}
		return;
	}
	if (IsBackwardDashActive())
	{
		const float BackwardProgress = FMath::Clamp(
			MobilityElapsed / ArcadeFlightTuning::BackwardTravelDuration, 0.0f, 1.0f);
		const FVector PathOffset = -MobilityInitialRotation.Vector()
			* ArcadeFlightTuning::BackwardTravelDistance
			* FMath::SmoothStep(0.0f, 1.0f, BackwardProgress);
		const FVector DashVelocity = DeltaTime > UE_SMALL_NUMBER
			? (PathOffset - MobilityPreviousPathOffset) / DeltaTime
			: FVector::ZeroVector;
		MobilityPreviousPathOffset = PathOffset;
		MobilityLateralVelocity = FMath::VInterpTo(
			MobilityLateralVelocity, DesiredLateralVelocity, DeltaTime,
			ArcadeFlightTuning::BackwardLateralResponse);
		const float RecoveryProgress = FMath::Clamp(
			(MobilityElapsed - ArcadeFlightTuning::BackwardTravelDuration)
			/ ArcadeFlightTuning::BackwardRecoveryDuration, 0.0f, 1.0f);
		const float RecoverySpeed = Stats.ForwardSpeed
			* FMath::SmoothStep(0.0f, 1.0f, RecoveryProgress);
		ApplyManeuverVelocity(DashVelocity
			+ MobilityInitialRotation.Vector() * RecoverySpeed
			+ MobilityLateralVelocity);
		if (MobilityElapsed >= ArcadeFlightTuning::BackwardDuration)
		{
			FinishMobilityAbility();
		}
		return;
	}
	if (IsQuickReversalActive())
	{
		const float Progress = GetMobilityProgress();
		const float CoastProgress = FMath::SmoothStep(
			0.0f, ArcadeFlightTuning::QuickCoastEnd, Progress);
		const float ThrustProgress = FMath::SmoothStep(
			ArcadeFlightTuning::QuickThrustStart,
			ArcadeFlightTuning::QuickThrustFull, Progress);
		MobilityLateralVelocity = FMath::VInterpTo(
			MobilityLateralVelocity, DesiredLateralVelocity, DeltaTime,
			ArcadeFlightTuning::QuickLateralResponse);
		const FVector ArcOffset = BuildQuickReversalArcOffset(Progress);
		const FVector ArcVelocity = DeltaTime > UE_SMALL_NUMBER
			? (ArcOffset - MobilityPreviousPathOffset) / DeltaTime
			: FVector::ZeroVector;
		MobilityPreviousPathOffset = ArcOffset;
		const FVector ExitDirection = BuildQuickReversalRotation(1.0f).GetForwardVector();
		const float TargetSpeed = FMath::Max(
			Stats.ForwardSpeed, Stats.ForwardSpeed * Stats.BoostSpeedMultiplier);
		ApplyManeuverVelocity(MobilityInitialForwardVelocity * (1.0f - CoastProgress)
			+ ArcVelocity
			+ ExitDirection * TargetSpeed * ThrustProgress
			+ MobilityLateralVelocity);
		if (Progress >= 1.0f)
		{
			FinishMobilityAbility();
		}
		return;
	}

	if (HoverState == EArcadeHoverState::Entering)
	{
		CurrentForwardSpeed = FMath::FInterpTo(CurrentForwardSpeed, 0.0f, DeltaTime,
			Stats.HoverDecelerationResponse);
		if (CurrentForwardSpeed <= 5.0f)
		{
			CurrentForwardSpeed = 0.0f;
			SetSimulationHoverState(EArcadeHoverState::Hovering);
		}
	}
	else if (HoverState == EArcadeHoverState::Hovering)
	{
		CurrentForwardSpeed = 0.0f;
	}
	else if (HoverState == EArcadeHoverState::Leaving)
	{
		const float ExitTargetSpeed = Stats.ForwardSpeed * (1.0f - HoverPresentationAlpha);
		CurrentForwardSpeed = FMath::FInterpTo(CurrentForwardSpeed, ExitTargetSpeed, DeltaTime,
			Stats.HoverExitAccelerationResponse);
	}
	else
	{
	const float BoostAlpha = CurrentBoostAlpha;
	const float MinimumSpeed = FMath::Min(Stats.MinForwardSpeed, Stats.ForwardSpeed);
	const float BrakedSpeed = FMath::Lerp(Stats.ForwardSpeed, MinimumSpeed, SmoothedBrake);
	const float BoostedSpeed = Stats.ForwardSpeed * Stats.BoostSpeedMultiplier;
	// Boost progressively overrides the brake and has full priority at BoostAlpha == 1.
	const float PlayerTargetSpeed = FMath::Lerp(BrakedSpeed, BoostedSpeed, BoostAlpha);

	// Arcade gravity: direction changes speed/acceleration only. It never adds downward movement.
	const float VerticalDirection = FVector::DotProduct(
		GetOwner()->GetActorForwardVector(), FVector::UpVector);
	float DirectionSpeedMultiplier = 1.0f;
	float DirectionResponseMultiplier = 1.0f;
	if (VerticalDirection >= 0.0f)
	{
		DirectionSpeedMultiplier = FMath::Lerp(
			1.0f, Stats.MaxClimbSpeedMultiplier, VerticalDirection);
		DirectionResponseMultiplier = FMath::Lerp(
			1.0f, Stats.MaxClimbResponseMultiplier, VerticalDirection);
	}
	else
	{
		const float DiveAmount = -VerticalDirection;
		DirectionSpeedMultiplier = FMath::Lerp(
			1.0f, Stats.MaxDiveSpeedMultiplier, DiveAmount);
		DirectionResponseMultiplier = FMath::Lerp(
			1.0f, Stats.MaxDiveResponseMultiplier, DiveAmount);
	}

	const float RollSpeedMultiplier = bEvasiveRollActive ? (2.0f / 3.0f) : 1.0f;
	const float TargetForwardSpeed = FMath::Max(
		MinimumSpeed, PlayerTargetSpeed * DirectionSpeedMultiplier * RollSpeedMultiplier);
	CurrentForwardSpeed = FMath::FInterpTo(CurrentForwardSpeed, TargetForwardSpeed, DeltaTime,
		Stats.ForwardSpeedResponse * DirectionResponseMultiplier);
	}
	const float EffectiveStrafe = HoverState == EArcadeHoverState::Flying
		? SmoothedStrafe : 0.0f;
	const FVector Velocity = GetOwner()->GetActorForwardVector() * CurrentForwardSpeed
		+ GetOwner()->GetActorRightVector() * EffectiveStrafe * MaximumStrafeSpeed;
	CurrentVelocity = Velocity;

	const FVector RequestedMove = Velocity * DeltaTime;
	if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
	{
		CurrentVelocity = Plane->MovePlaneWithCollision(RequestedMove, Velocity);
		CurrentForwardSpeed = FMath::Max(0.0f, FVector::DotProduct(
			CurrentVelocity, GetOwner()->GetActorForwardVector()));
	}
	else
	{
		GetOwner()->AddActorWorldOffset(RequestedMove, false, nullptr, ETeleportType::None);
	}
}
