#include "ArcadeFlightComponent.h"

#include "ArcadeJetPawn.h"
#include "BackwardDashComponent.h"
#include "EvasiveRollComponent.h"
#include "ForwardDashComponent.h"
#include "FlightBoundsVolume.h"
#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "QuickReversalComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
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
}

UArcadeFlightComponent::UArcadeFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UArcadeFlightComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedStatsComponent = Owner->FindComponentByClass<UJetStatsComponent>();
		CachedBoostComponent = Owner->FindComponentByClass<UJetBoostComponent>();
		CachedEvasiveRollComponent = Owner->FindComponentByClass<UEvasiveRollComponent>();
		CachedQuickReversalComponent = Owner->FindComponentByClass<UQuickReversalComponent>();
		CachedBackwardDashComponent = Owner->FindComponentByClass<UBackwardDashComponent>();
		CachedForwardDashComponent = Owner->FindComponentByClass<UForwardDashComponent>();
		for (TActorIterator<AFlightBoundsVolume> It(GetWorld()); It; ++It)
		{
			CachedFlightBounds = *It;
			break;
		}
		// Input is collected by the Pawn first. Flight then updates the authoritative
		// transform before the local camera and weapon aim are refreshed.
		AddTickPrerequisiteActor(Owner);
		if (CachedBoostComponent)
		{
			AddTickPrerequisiteComponent(CachedBoostComponent);
		}
		if (Owner->HasAuthority())
		{
			ServerState.Location = Owner->GetActorLocation();
			ServerState.Rotation = Owner->GetActorRotation();
			ServerState.ServerTimeSeconds = GetWorld()->GetTimeSeconds();
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

void UArcadeFlightComponent::ToggleHover()
{
	if (HoverState == EArcadeHoverState::Flying)
	{
		// Lock local combat immediately instead of waiting for the server round trip.
		bLocalHoverEntryPending = true;
	}
	if (GetOwner()->HasAuthority())
	{
		SetAuthoritativeHoverState(
			HoverState == EArcadeHoverState::Flying || HoverState == EArcadeHoverState::Leaving
				? EArcadeHoverState::Entering : EArcadeHoverState::Leaving);
	}
	else
	{
		ServerToggleHover();
	}
}

void UArcadeFlightComponent::RequestExitHover()
{
	if (GetOwner()->HasAuthority())
	{
		if (HoverState != EArcadeHoverState::Flying)
		{
			SetAuthoritativeHoverState(EArcadeHoverState::Leaving);
		}
	}
	else
	{
		ServerRequestExitHover();
	}
}

void UArcadeFlightComponent::ServerToggleHover_Implementation()
{
	ToggleHover();
}

void UArcadeFlightComponent::ServerRequestExitHover_Implementation()
{
	RequestExitHover();
}

void UArcadeFlightComponent::SetAuthoritativeHoverState(const EArcadeHoverState NewState)
{
	if (!GetOwner()->HasAuthority() || HoverState == NewState)
	{
		return;
	}
	HoverState = NewState;
	if (NewState == EArcadeHoverState::Entering)
	{
		if (CachedBoostComponent)
		{
			CachedBoostComponent->SetBoostRequested(false);
		}
	}
	GetOwner()->ForceNetUpdate();
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
	UpdateBoundaryState();
	const UJetStatsComponent* StatsComponent = CachedStatsComponent;
	if (StatsComponent)
	{
		UpdateHoverPresentation(DeltaTime, StatsComponent->GetFlightStats());
	}
	if (OwnerPawn && OwnerPawn->IsLocallyControlled() && !GetOwner()->HasAuthority())
	{
		TimeSinceInputSent += DeltaTime;
		if (TimeSinceInputSent >= InputReplicationInterval)
		{
			TimeSinceInputSent = 0.0f;
			ServerSetFlightInput(RawSteering, RawStrafe, RawBrake);
		}
	}

	if (GetOwner()->HasAuthority())
	{
		SimulateFlight(DeltaTime);
		UpdateReplicatedState();
	}
	else
	{
		if (OwnerPawn && OwnerPawn->IsLocallyControlled() && StatsComponent)
		{
			UpdateLocalBrakePresentation(DeltaTime, StatsComponent->GetFlightStats());
		}
		InterpolateBufferedState();
	}

	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
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

void UArcadeFlightComponent::UpdateLocalBrakePresentation(
	const float DeltaTime, const FJetFlightStats& Stats)
{
	SmoothedBrake = FMath::FInterpTo(
		SmoothedBrake, RawBrake, DeltaTime, Stats.BrakeInputResponseSpeed);
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

void UArcadeFlightComponent::ServerSetFlightInput_Implementation(
	const FVector2D Steering, const float Strafe, const float Brake)
{
	RawSteering.X = FMath::Clamp(Steering.X, -1.0f, 1.0f);
	RawSteering.Y = FMath::Clamp(Steering.Y, -1.0f, 1.0f);
	RawStrafe = FMath::Clamp(Strafe, -1.0f, 1.0f);
	RawBrake = FMath::Clamp(Brake, 0.0f, 1.0f);
}

void UArcadeFlightComponent::SimulateFlight(const float DeltaTime)
{
	if (!CachedStatsComponent)
	{
		return;
	}

	const FJetFlightStats& Stats = CachedStatsComponent->GetFlightStats();
	UpdateSmoothedInput(DeltaTime, Stats);
	RotateAircraft(DeltaTime, Stats);
	UpdateVisualBank(DeltaTime);
	MoveAircraft(DeltaTime, Stats);
}

void UArcadeFlightComponent::UpdateBoundaryState()
{
	BoundaryWarningAlpha = 0.0f;
	if (!CachedFlightBounds || !GetOwner())
	{
		return;
	}

	bool bOutside = false;
	CachedFlightBounds->GetBoundaryInfo(
		GetOwner()->GetActorLocation(), BoundaryWarningAlpha, bOutside);
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	const bool bWasReturning = bBoundaryReturnActive;
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
	if (bWasReturning != bBoundaryReturnActive)
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UArcadeFlightComponent::UpdateReplicatedState()
{
	ServerState.Location = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = CurrentVelocity;
	ServerState.VisualBankDegrees = CurrentVisualBankDegrees;
	ServerState.BrakeAlpha = static_cast<uint8>(FMath::RoundToInt(
		FMath::Clamp(SmoothedBrake, 0.0f, 1.0f) * 255.0f));
	ServerState.ServerTimeSeconds = GetWorld()->GetTimeSeconds();
	ServerState.bTeleport = bAuthoritativeTeleportPending;
	bAuthoritativeTeleportPending = false;
}

void UArcadeFlightComponent::NotifyAuthoritativeTeleport()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bAuthoritativeTeleportPending = true;
	UpdateReplicatedState();
	GetOwner()->ForceNetUpdate();
}

void UArcadeFlightComponent::PrepareStraightManeuverExit()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	SmoothedSteering = FVector2D::ZeroVector;
}

void UArcadeFlightComponent::OnRep_ServerState()
{
	FBufferedFlightState Snapshot;
	Snapshot.Location = ServerState.Location;
	Snapshot.Rotation = ServerState.Rotation.Quaternion();
	Snapshot.Velocity = ServerState.Velocity;
	Snapshot.VisualBankDegrees = ServerState.VisualBankDegrees;
	Snapshot.BrakeAlpha = static_cast<float>(ServerState.BrakeAlpha) / 255.0f;
	Snapshot.ServerTimeSeconds = ServerState.ServerTimeSeconds;

	if (ServerState.bTeleport)
	{
		SnapshotBuffer.Reset();
		SnapshotBuffer.Add(Snapshot);
		GetOwner()->SetActorLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr,
			ETeleportType::TeleportPhysics);
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			Plane->SetVisualBank(Snapshot.VisualBankDegrees);
		}
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner());
			!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		{
			SmoothedBrake = Snapshot.BrakeAlpha;
		}
		return;
	}

	if (!SnapshotBuffer.IsEmpty() && Snapshot.ServerTimeSeconds <= SnapshotBuffer.Last().ServerTimeSeconds)
	{
		return;
	}
	SnapshotBuffer.Add(Snapshot);
	if (SnapshotBuffer.Num() > 32)
	{
		SnapshotBuffer.RemoveAt(0, SnapshotBuffer.Num() - 32, EAllowShrinking::No);
	}

	if (SnapshotBuffer.Num() == 1)
	{
		GetOwner()->SetActorLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr,
			ETeleportType::TeleportPhysics);
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			Plane->SetVisualBank(Snapshot.VisualBankDegrees);
		}
	}
}

void UArcadeFlightComponent::InterpolateBufferedState()
{
	if (SnapshotBuffer.IsEmpty())
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const double RenderTime = ServerNow - SnapshotInterpolationDelay;

	while (SnapshotBuffer.Num() >= 3 && SnapshotBuffer[1].ServerTimeSeconds <= RenderTime)
	{
		SnapshotBuffer.RemoveAt(0, 1, EAllowShrinking::No);
	}

	FVector RenderLocation = SnapshotBuffer[0].Location;
	FQuat RenderRotation = SnapshotBuffer[0].Rotation;
	float RenderVisualBank = SnapshotBuffer[0].VisualBankDegrees;
	float RenderBrakeAlpha = SnapshotBuffer[0].BrakeAlpha;
	if (SnapshotBuffer.Num() >= 2)
	{
		const FBufferedFlightState& From = SnapshotBuffer[0];
		const FBufferedFlightState& To = SnapshotBuffer[1];
		const double Duration = To.ServerTimeSeconds - From.ServerTimeSeconds;
		if (RenderTime <= To.ServerTimeSeconds && Duration > UE_SMALL_NUMBER)
		{
			const float Alpha = FMath::Clamp(static_cast<float>((RenderTime - From.ServerTimeSeconds) / Duration), 0.0f, 1.0f);
			RenderLocation = FMath::Lerp(From.Location, To.Location, Alpha);
			RenderRotation = FQuat::Slerp(From.Rotation, To.Rotation, Alpha).GetNormalized();
			RenderVisualBank = FMath::Lerp(
				From.VisualBankDegrees, To.VisualBankDegrees, Alpha);
			RenderBrakeAlpha = FMath::Lerp(From.BrakeAlpha, To.BrakeAlpha, Alpha);
		}
		else if (RenderTime > To.ServerTimeSeconds)
		{
			const float Extrapolation = FMath::Min(static_cast<float>(RenderTime - To.ServerTimeSeconds), MaxExtrapolationTime);
			RenderLocation = To.Location + To.Velocity * Extrapolation;
			RenderRotation = To.Rotation;
			RenderVisualBank = To.VisualBankDegrees;
			RenderBrakeAlpha = To.BrakeAlpha;
		}
	}

	GetOwner()->SetActorLocationAndRotation(RenderLocation, RenderRotation, false, nullptr,
		ETeleportType::None);
	if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
	{
		Plane->SetVisualBank(RenderVisualBank);
	}
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		SmoothedBrake = RenderBrakeAlpha;
	}
	const FVector RenderForward = RenderRotation.GetForwardVector();
	CurrentVelocity = SnapshotBuffer.Num() >= 2 ? SnapshotBuffer[1].Velocity : SnapshotBuffer[0].Velocity;
	CurrentForwardSpeed = FVector::DotProduct(CurrentVelocity, RenderForward);
}

void UArcadeFlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArcadeFlightComponent, ServerState);
	DOREPLIFETIME(UArcadeFlightComponent, HoverState);
	DOREPLIFETIME(UArcadeFlightComponent, bBoundaryReturnActive);
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
	FQuat ManeuverRotation = FQuat::Identity;
	if (CachedQuickReversalComponent
		&& CachedQuickReversalComponent->GetAuthoritativeFlightRotation(ManeuverRotation))
	{
		GetOwner()->SetActorRotation(ManeuverRotation);
		return;
	}
	if (CachedBackwardDashComponent
		&& CachedBackwardDashComponent->GetAuthoritativeFlightRotation(ManeuverRotation))
	{
		GetOwner()->SetActorRotation(ManeuverRotation);
		return;
	}
	if (CachedForwardDashComponent
		&& CachedForwardDashComponent->GetAuthoritativeFlightRotation(ManeuverRotation))
	{
		GetOwner()->SetActorRotation(ManeuverRotation);
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
		GetOwner()->SetActorRotation(FRotator(NewPitch, NewYaw, 0.0f));
		if (HoverState == EArcadeHoverState::Leaving
			&& FMath::Abs(NewPitch) <= ArcadeFlightTuning::HoverExitControlPitchTolerance)
		{
			GetOwner()->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
			SetAuthoritativeHoverState(EArcadeHoverState::Flying);
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
	FVector ManeuverVelocity = FVector::ZeroVector;
	float ManeuverForwardSpeed = CurrentForwardSpeed;
	const float MaximumStrafeSpeed = Stats.ForwardSpeed * Stats.StrafeSpeedMultiplier;
	const FVector DesiredLateralVelocity = GetOwner()->GetActorRightVector()
		* SmoothedStrafe * MaximumStrafeSpeed;
	if (CachedForwardDashComponent
		&& CachedForwardDashComponent->ConsumeAuthoritativeMovement(
			Stats.ForwardSpeed, ManeuverVelocity, ManeuverForwardSpeed))
	{
		CurrentForwardSpeed = ManeuverForwardSpeed;
		CurrentVelocity = ManeuverVelocity;
		const FVector RequestedMove = ManeuverVelocity * DeltaTime;
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			CurrentVelocity = Plane->MovePlaneWithCollision(RequestedMove, ManeuverVelocity);
		}
		else
		{
			GetOwner()->AddActorWorldOffset(
				RequestedMove, false, nullptr, ETeleportType::None);
		}
		CurrentForwardSpeed = FVector::DotProduct(
			CurrentVelocity, GetOwner()->GetActorForwardVector());
		return;
	}
	if (CachedBackwardDashComponent
		&& CachedBackwardDashComponent->ConsumeAuthoritativeMovement(
			DeltaTime,
			Stats.ForwardSpeed,
			DesiredLateralVelocity,
			ManeuverVelocity,
			ManeuverForwardSpeed))
	{
		CurrentForwardSpeed = ManeuverForwardSpeed;
		CurrentVelocity = ManeuverVelocity;

		const FVector RequestedMove = ManeuverVelocity * DeltaTime;
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			CurrentVelocity = Plane->MovePlaneWithCollision(RequestedMove, ManeuverVelocity);
		}
		else
		{
			GetOwner()->AddActorWorldOffset(
				RequestedMove, false, nullptr, ETeleportType::None);
		}
		CurrentForwardSpeed = FVector::DotProduct(
			CurrentVelocity, GetOwner()->GetActorForwardVector());
		return;
	}
	if (CachedQuickReversalComponent
		&& CachedQuickReversalComponent->ConsumeAuthoritativeMovement(
			DeltaTime,
			Stats.ForwardSpeed,
			Stats.ForwardSpeed * Stats.BoostSpeedMultiplier,
			DesiredLateralVelocity,
			ManeuverVelocity,
			ManeuverForwardSpeed))
	{
		// Lateral input remains available, but the skill owns all longitudinal
		// deceleration and thrust until its inertial reversal has completed.
		CurrentForwardSpeed = ManeuverForwardSpeed;
		CurrentVelocity = ManeuverVelocity;

		const FVector RequestedMove = ManeuverVelocity * DeltaTime;
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
		{
			CurrentVelocity = Plane->MovePlaneWithCollision(RequestedMove, ManeuverVelocity);
		}
		else
		{
			GetOwner()->AddActorWorldOffset(
				RequestedMove, false, nullptr, ETeleportType::None);
		}
		CurrentForwardSpeed = FVector::DotProduct(
			CurrentVelocity, GetOwner()->GetActorForwardVector());
		return;
	}

	if (HoverState == EArcadeHoverState::Entering)
	{
		CurrentForwardSpeed = FMath::FInterpTo(CurrentForwardSpeed, 0.0f, DeltaTime,
			Stats.HoverDecelerationResponse);
		if (CurrentForwardSpeed <= 5.0f)
		{
			CurrentForwardSpeed = 0.0f;
			SetAuthoritativeHoverState(EArcadeHoverState::Hovering);
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
	const float BoostAlpha = CachedBoostComponent ? CachedBoostComponent->GetBoostAlpha() : 0.0f;
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

	const float RollSpeedMultiplier = CachedEvasiveRollComponent
		&& CachedEvasiveRollComponent->IsRolling() ? (2.0f / 3.0f) : 1.0f;
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
