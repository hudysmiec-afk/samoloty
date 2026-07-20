#include "ArcadeFlightComponent.h"

#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

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
		Owner->SetReplicateMovement(true);
		Owner->SetNetUpdateFrequency(30.0f);
		Owner->SetMinNetUpdateFrequency(15.0f);
	}
}

void UArcadeFlightComponent::SetLocalFlightInput(const FVector2D& Steering, const float Strafe)
{
	RawSteering.X = FMath::Clamp(Steering.X, -1.0f, 1.0f);
	RawSteering.Y = FMath::Clamp(Steering.Y, -1.0f, 1.0f);
	RawStrafe = FMath::Clamp(Strafe, -1.0f, 1.0f);
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
			ServerSetFlightInput(RawSteering, RawStrafe, ++LocalInputSequence);
		}
	}

	if (ShouldSimulate())
	{
		SimulateFlight(DeltaTime);
	}
}

void UArcadeFlightComponent::ServerSetFlightInput_Implementation(
	const FVector2D Steering, const float Strafe, const uint16 InputSequence)
{
	RawSteering.X = FMath::Clamp(Steering.X, -1.0f, 1.0f);
	RawSteering.Y = FMath::Clamp(Steering.Y, -1.0f, 1.0f);
	RawStrafe = FMath::Clamp(Strafe, -1.0f, 1.0f);
}

bool UArcadeFlightComponent::ShouldSimulate() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return GetOwner() && (GetOwner()->HasAuthority() || (OwnerPawn && OwnerPawn->IsLocallyControlled()));
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
	RotateAircraft(DeltaTime, Stats);
	MoveAircraft(DeltaTime, Stats);
}

void UArcadeFlightComponent::RotateAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
	const FRotator CurrentRotation = GetOwner()->GetActorRotation();
	const float NewYaw = CurrentRotation.Yaw + SmoothedSteering.X * Stats.MaxYawTurnRate * DeltaTime;
	const float NewPitch = FMath::Clamp(
		CurrentRotation.Pitch + SmoothedSteering.Y * Stats.MaxPitchTurnRate * DeltaTime,
		-Stats.MaxPitch, Stats.MaxPitch);

	const float VisualBank = -SmoothedSteering.X * Stats.MaxVisualBank;
	const float StrafeBank = -SmoothedStrafe * Stats.MaxStrafeBank;
	const float StrafePriority = FMath::Clamp(FMath::Abs(SmoothedStrafe), 0.0f, 1.0f);
	const float TargetRoll = FMath::Lerp(VisualBank, StrafeBank, StrafePriority);
	const float NewRoll = FMath::FInterpTo(CurrentRotation.Roll, TargetRoll, DeltaTime, Stats.RotationResponsiveness);
	GetOwner()->SetActorRotation(FRotator(NewPitch, NewYaw, NewRoll));
}

void UArcadeFlightComponent::MoveAircraft(const float DeltaTime, const FJetFlightStats& Stats)
{
	const UJetBoostComponent* Boost = GetOwner()->FindComponentByClass<UJetBoostComponent>();
	const float BoostAlpha = Boost ? Boost->GetBoostAlpha() : 0.0f;
	CurrentForwardSpeed = Stats.ForwardSpeed * FMath::Lerp(1.0f, Stats.BoostSpeedMultiplier, BoostAlpha);
	const FVector Velocity = GetOwner()->GetActorForwardVector() * CurrentForwardSpeed
		+ GetOwner()->GetActorRightVector() * SmoothedStrafe * Stats.StrafeSpeed;

	FHitResult Hit;
	GetOwner()->AddActorWorldOffset(Velocity * DeltaTime, true, &Hit);
	if (Hit.IsValidBlockingHit())
	{
		const FVector RemainingMove = Velocity * DeltaTime * (1.0f - Hit.Time);
		GetOwner()->AddActorWorldOffset(FVector::VectorPlaneProject(RemainingMove, Hit.Normal), true);
	}
}
