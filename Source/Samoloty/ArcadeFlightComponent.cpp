#include "ArcadeFlightComponent.h"

#include "ArcadeJetPawn.h"
#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace ArcadeFlightTuning
{
	// Flight-feel tuning is intentionally code-only while the prototype is being established.
	constexpr float SteeringResponseSpeed = 4.0f;
	constexpr float StrafeEngageSpeed = 10.0f;
	constexpr float StrafeReleaseSpeed = 1.0f;
	constexpr float MaxVisualBankDegrees = 72.0f;
	constexpr float VisualBankResponseSpeed = 1.0f;
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
		Owner->SetReplicates(true);
		// A single custom server state is used for every client, including the owner.
		// Standard ReplicatedMovement skips autonomous proxies and caused two truths.
		Owner->SetReplicateMovement(false);
		Owner->SetNetUpdateFrequency(30.0f);
		Owner->SetMinNetUpdateFrequency(15.0f);
		Owner->SetNetCullDistanceSquared(FMath::Square(500000.0f)); // 5 km in Unreal units.
		if (Owner->HasAuthority())
		{
			ServerState.Location = Owner->GetActorLocation();
			ServerState.Rotation = Owner->GetActorRotation();
			ServerState.ServerTimeSeconds = GetWorld()->GetTimeSeconds();
		}
		if (const UJetStatsComponent* StatsComponent = Owner->FindComponentByClass<UJetStatsComponent>())
		{
			CurrentForwardSpeed = StatsComponent->GetFlightStats().ForwardSpeed;
		}
		if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(Owner))
		{
			Plane->SetVisualBank(0.0f);
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
		if (UJetBoostComponent* Boost = GetOwner()->FindComponentByClass<UJetBoostComponent>())
		{
			Boost->SetBoostRequested(false);
		}
	}
	GetOwner()->ForceNetUpdate();
}

void UArcadeFlightComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
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
			ServerSetFlightInput(RawSteering, RawStrafe, RawBrake, ++LocalInputSequence);
		}
	}

	if (GetOwner()->HasAuthority())
	{
		SimulateFlight(DeltaTime);
		UpdateReplicatedState();
	}
	else
	{
		// This only keeps the owning client's camera responsive. It never moves the aircraft.
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			UpdateLocalPresentationInput(DeltaTime);
		}
		InterpolateBufferedState();
	}
}

void UArcadeFlightComponent::UpdateHoverPresentation(const float DeltaTime, const FJetFlightStats& Stats)
{
	const bool bHoverActive = HoverState == EArcadeHoverState::Entering
		|| HoverState == EArcadeHoverState::Hovering;
	HoverPresentationAlpha = FMath::FInterpTo(HoverPresentationAlpha, bHoverActive ? 1.0f : 0.0f,
		DeltaTime, Stats.HoverRotationResponse);
}

void UArcadeFlightComponent::UpdateLocalPresentationInput(const float DeltaTime)
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}

	UpdateSmoothedInput(DeltaTime, StatsComponent->GetFlightStats());
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
		SmoothedBrake, RawBrake, DeltaTime, Stats.InputSmoothingSpeed);
}

void UArcadeFlightComponent::ServerSetFlightInput_Implementation(
	const FVector2D Steering, const float Strafe, const float Brake, const uint16 InputSequence)
{
	RawSteering.X = FMath::Clamp(Steering.X, -1.0f, 1.0f);
	RawSteering.Y = FMath::Clamp(Steering.Y, -1.0f, 1.0f);
	RawStrafe = FMath::Clamp(Strafe, -1.0f, 1.0f);
	RawBrake = FMath::Clamp(Brake, 0.0f, 1.0f);
}

void UArcadeFlightComponent::SimulateFlight(const float DeltaTime)
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}

	const FJetFlightStats& Stats = StatsComponent->GetFlightStats();
	UpdateSmoothedInput(DeltaTime, Stats);
	RotateAircraft(DeltaTime, Stats);
	UpdateVisualBank(DeltaTime);
	MoveAircraft(DeltaTime, Stats);
}

void UArcadeFlightComponent::UpdateReplicatedState()
{
	ServerState.Location = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = CurrentVelocity;
	ServerState.VisualBankDegrees = CurrentVisualBankDegrees;
	ServerState.ServerTimeSeconds = GetWorld()->GetTimeSeconds();
	ServerState.bTeleport = false;
	++ServerState.Sequence;
}

void UArcadeFlightComponent::OnRep_ServerState()
{
	FBufferedFlightState Snapshot;
	Snapshot.Location = ServerState.Location;
	Snapshot.Rotation = ServerState.Rotation.Quaternion();
	Snapshot.Velocity = ServerState.Velocity;
	Snapshot.VisualBankDegrees = ServerState.VisualBankDegrees;
	Snapshot.ServerTimeSeconds = ServerState.ServerTimeSeconds;
	Snapshot.Sequence = ServerState.Sequence;

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
		}
		else if (RenderTime > To.ServerTimeSeconds)
		{
			const float Extrapolation = FMath::Min(static_cast<float>(RenderTime - To.ServerTimeSeconds), MaxExtrapolationTime);
			RenderLocation = To.Location + To.Velocity * Extrapolation;
			RenderRotation = To.Rotation;
			RenderVisualBank = To.VisualBankDegrees;
		}
	}

	GetOwner()->SetActorLocationAndRotation(RenderLocation, RenderRotation, false, nullptr,
		ETeleportType::None);
	if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
	{
		Plane->SetVisualBank(RenderVisualBank);
	}
	const FVector RenderForward = RenderRotation.GetForwardVector();
	CurrentVelocity = SnapshotBuffer.Num() >= 2 ? SnapshotBuffer[1].Velocity : SnapshotBuffer[0].Velocity;
	CurrentForwardSpeed = FMath::Max(0.0f, FVector::DotProduct(CurrentVelocity, RenderForward));
}

void UArcadeFlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArcadeFlightComponent, ServerState);
	DOREPLIFETIME(UArcadeFlightComponent, HoverState);
}

void UArcadeFlightComponent::RotateAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
	const FRotator CurrentRotation = GetOwner()->GetActorRotation();
	if (HoverState != EArcadeHoverState::Flying)
	{
		const float TargetPitch = HoverState == EArcadeHoverState::Leaving ? 0.0f : Stats.HoverPitch;
		const float NewPitch = HoverState == EArcadeHoverState::Hovering
			? Stats.HoverPitch
			: FMath::FInterpTo(CurrentRotation.Pitch, TargetPitch, DeltaTime, Stats.HoverRotationResponse);
		const float NewYaw = CurrentRotation.Yaw;
		GetOwner()->SetActorRotation(FRotator(NewPitch, NewYaw, 0.0f));
		if (HoverState == EArcadeHoverState::Leaving && FMath::Abs(NewPitch) <= 0.5f)
		{
			GetOwner()->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
			SetAuthoritativeHoverState(EArcadeHoverState::Flying);
		}
		return;
	}
	const float TurnRateMultiplier = CalculateTurnRateMultiplier(Stats);

	const float NewYaw = CurrentRotation.Yaw
		+ SmoothedSteering.X * Stats.MaxYawTurnRate * TurnRateMultiplier * DeltaTime;
	const float NewPitch = FMath::Clamp(
		CurrentRotation.Pitch + SmoothedSteering.Y * Stats.MaxPitchTurnRate * TurnRateMultiplier * DeltaTime,
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
	// Unreal and the original DirectX renderer use opposite roll signs.
	// Positive screen yaw must bank the visible model in the intended direction.
	const float TurnBank = SmoothedSteering.X * ArcadeFlightTuning::MaxVisualBankDegrees;
	const float StrafeBank = SmoothedStrafe * ArcadeFlightTuning::MaxVisualBankDegrees;
	const bool bStrafeInfluencesBank = FMath::Abs(RawStrafe) > KINDA_SMALL_NUMBER
		|| FMath::Abs(SmoothedStrafe) > 0.01f;
	const float TargetBankDegrees = (bStrafeInfluencesBank ? StrafeBank : TurnBank) * FlightAlpha;

	// The original game applies bank only to the render matrix. Position and logical
	// direction stay unified; the camera therefore never inherits this roll.
	CurrentVisualBankDegrees = FMath::FInterpTo(
		CurrentVisualBankDegrees,
		TargetBankDegrees,
		DeltaTime,
		ArcadeFlightTuning::VisualBankResponseSpeed);
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
	const UJetStatsComponent* StatsComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	return StatsComponent ? CalculateTurnRateMultiplier(StatsComponent->GetFlightStats()) : 1.0f;
}

void UArcadeFlightComponent::MoveAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
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
	const UJetBoostComponent* Boost = GetOwner()->FindComponentByClass<UJetBoostComponent>();
	const float BoostAlpha = Boost ? Boost->GetBoostAlpha() : 0.0f;
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

	const float TargetForwardSpeed = PlayerTargetSpeed * DirectionSpeedMultiplier;
	CurrentForwardSpeed = FMath::FInterpTo(CurrentForwardSpeed, TargetForwardSpeed, DeltaTime,
		Stats.ForwardSpeedResponse * DirectionResponseMultiplier);
	}
	const float EffectiveStrafe = HoverState == EArcadeHoverState::Flying ? SmoothedStrafe : 0.0f;
	const float MaximumStrafeSpeed = Stats.ForwardSpeed * Stats.StrafeSpeedMultiplier;
	const FVector Velocity = GetOwner()->GetActorForwardVector() * CurrentForwardSpeed
		+ GetOwner()->GetActorRightVector() * EffectiveStrafe * MaximumStrafeSpeed;
	CurrentVelocity = Velocity;

	const FVector RequestedMove = Velocity * DeltaTime;
	if (AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner()))
	{
		const FVector AppliedMove = Plane->MovePlaneWithCollision(RequestedMove);
		CurrentVelocity = DeltaTime > UE_SMALL_NUMBER ? AppliedMove / DeltaTime : FVector::ZeroVector;
	}
	else
	{
		GetOwner()->AddActorWorldOffset(RequestedMove, false, nullptr, ETeleportType::None);
	}
}
