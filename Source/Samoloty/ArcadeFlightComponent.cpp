#include "ArcadeFlightComponent.h"

#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

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

void UArcadeFlightComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
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

void UArcadeFlightComponent::UpdateLocalPresentationInput(const float DeltaTime)
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}

	const FJetFlightStats& Stats = StatsComponent->GetFlightStats();
	SmoothedSteering.X = FMath::FInterpTo(SmoothedSteering.X, RawSteering.X, DeltaTime, Stats.InputSmoothingSpeed);
	SmoothedSteering.Y = FMath::FInterpTo(SmoothedSteering.Y, RawSteering.Y, DeltaTime, Stats.InputSmoothingSpeed);
	SmoothedStrafe = FMath::FInterpTo(SmoothedStrafe, RawStrafe, DeltaTime, Stats.StrafeResponseSpeed);
	SmoothedBrake = FMath::FInterpTo(SmoothedBrake, RawBrake, DeltaTime, Stats.InputSmoothingSpeed);
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
	SmoothedSteering.X = FMath::FInterpTo(SmoothedSteering.X, RawSteering.X, DeltaTime, Stats.InputSmoothingSpeed);
	SmoothedSteering.Y = FMath::FInterpTo(SmoothedSteering.Y, RawSteering.Y, DeltaTime, Stats.InputSmoothingSpeed);
	SmoothedStrafe = FMath::FInterpTo(SmoothedStrafe, RawStrafe, DeltaTime, Stats.StrafeResponseSpeed);
	SmoothedBrake = FMath::FInterpTo(SmoothedBrake, RawBrake, DeltaTime, Stats.InputSmoothingSpeed);
	RotateAircraft(DeltaTime, Stats);
	MoveAircraft(DeltaTime, Stats);
}

void UArcadeFlightComponent::UpdateReplicatedState()
{
	ServerState.Location = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = CurrentVelocity;
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
	Snapshot.ServerTimeSeconds = ServerState.ServerTimeSeconds;
	Snapshot.Sequence = ServerState.Sequence;

	if (ServerState.bTeleport)
	{
		SnapshotBuffer.Reset();
		SnapshotBuffer.Add(Snapshot);
		GetOwner()->SetActorLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr,
			ETeleportType::TeleportPhysics);
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
		}
		else if (RenderTime > To.ServerTimeSeconds)
		{
			const float Extrapolation = FMath::Min(static_cast<float>(RenderTime - To.ServerTimeSeconds), MaxExtrapolationTime);
			RenderLocation = To.Location + To.Velocity * Extrapolation;
			RenderRotation = To.Rotation;
		}
	}

	GetOwner()->SetActorLocationAndRotation(RenderLocation, RenderRotation, false, nullptr,
		ETeleportType::None);
	const FVector RenderForward = RenderRotation.GetForwardVector();
	CurrentVelocity = SnapshotBuffer.Num() >= 2 ? SnapshotBuffer[1].Velocity : SnapshotBuffer[0].Velocity;
	CurrentForwardSpeed = FMath::Max(0.0f, FVector::DotProduct(CurrentVelocity, RenderForward));
}

void UArcadeFlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArcadeFlightComponent, ServerState);
}

void UArcadeFlightComponent::RotateAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
	const FRotator CurrentRotation = GetOwner()->GetActorRotation();
	const float TurnRateMultiplier = CalculateTurnRateMultiplier(Stats);

	const float NewYaw = CurrentRotation.Yaw
		+ SmoothedSteering.X * Stats.MaxYawTurnRate * TurnRateMultiplier * DeltaTime;
	const float NewPitch = FMath::Clamp(
		CurrentRotation.Pitch + SmoothedSteering.Y * Stats.MaxPitchTurnRate * TurnRateMultiplier * DeltaTime,
		-Stats.MaxPitch, Stats.MaxPitch);

	const float VisualBank = -SmoothedSteering.X * Stats.MaxVisualBank;
	const float StrafeBank = -SmoothedStrafe * Stats.MaxStrafeBank;
	const float StrafePriority = FMath::Clamp(FMath::Abs(SmoothedStrafe), 0.0f, 1.0f);
	const float TargetRoll = FMath::Lerp(VisualBank, StrafeBank, StrafePriority);
	const float NewRoll = FMath::FInterpTo(CurrentRotation.Roll, TargetRoll, DeltaTime, Stats.RotationResponsiveness);
	GetOwner()->SetActorRotation(FRotator(NewPitch, NewYaw, NewRoll));
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
	const UJetBoostComponent* Boost = GetOwner()->FindComponentByClass<UJetBoostComponent>();
	const float BoostAlpha = Boost ? Boost->GetBoostAlpha() : 0.0f;
	const float MinimumSpeed = FMath::Min(Stats.MinForwardSpeed, Stats.ForwardSpeed);
	const float BrakedSpeed = FMath::Lerp(Stats.ForwardSpeed, MinimumSpeed, SmoothedBrake);
	const float BoostedSpeed = Stats.ForwardSpeed * Stats.BoostSpeedMultiplier;
	// Boost progressively overrides the brake and has full priority at BoostAlpha == 1.
	const float TargetForwardSpeed = FMath::Lerp(BrakedSpeed, BoostedSpeed, BoostAlpha);
	CurrentForwardSpeed = FMath::FInterpTo(CurrentForwardSpeed, TargetForwardSpeed, DeltaTime,
		Stats.ForwardSpeedResponse);
	const FVector Velocity = GetOwner()->GetActorForwardVector() * CurrentForwardSpeed
		+ GetOwner()->GetActorRightVector() * SmoothedStrafe * Stats.StrafeSpeed;
	CurrentVelocity = Velocity;

	FHitResult Hit;
	GetOwner()->AddActorWorldOffset(Velocity * DeltaTime, true, &Hit);
	if (Hit.IsValidBlockingHit())
	{
		const FVector RemainingMove = Velocity * DeltaTime * (1.0f - Hit.Time);
		GetOwner()->AddActorWorldOffset(FVector::VectorPlaneProject(RemainingMove, Hit.Normal), true);
	}
}
