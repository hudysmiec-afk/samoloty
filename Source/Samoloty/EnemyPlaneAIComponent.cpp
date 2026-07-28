#include "EnemyPlaneAIComponent.h"

#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RifleGunComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UEnemyPlaneAIComponent::UEnemyPlaneAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UEnemyPlaneAIComponent::BeginPlay()
{
	Super::BeginPlay();
	RifleGun = GetOwner()->FindComponentByClass<URifleGunComponent>();
	JetStats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	SpawnLocation = GetOwner()->GetActorLocation();
	TargetScanTimeRemaining = FMath::FRandRange(0.0f, TargetScanInterval);
	ObstacleCheckTimeRemaining = FMath::FRandRange(0.0f, ObstacleCheckInterval);
	ChooseRoamPoint();

	if (GetOwner()->HasAuthority())
	{
		UpdateServerNetworkState();
	}
}

void UEnemyPlaneAIComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority())
	{
		UpdateClientSmoothing(DeltaTime);
		return;
	}
	StateElapsedTime += DeltaTime;

	TargetScanTimeRemaining -= DeltaTime;
	if (TargetScanTimeRemaining <= 0.0f)
	{
		TargetScanTimeRemaining = TargetScanInterval;
		ScanForTarget();
	}

	ObstacleCheckTimeRemaining -= DeltaTime;
	if (ObstacleCheckTimeRemaining <= 0.0f)
	{
		ObstacleCheckTimeRemaining = ObstacleCheckInterval;
		UpdateObstacleAvoidance();
	}

	if (State == EEnemyPlaneState::Roaming
		&& FVector::DistSquared(GetOwner()->GetActorLocation(), RoamPoint)
		<= FMath::Square(RoamAcceptanceRadius))
	{
		ChooseRoamPoint();
	}
	else if (State == EEnemyPlaneState::Extend
		&& FVector::DistSquared(GetOwner()->GetActorLocation(), ExtendStart)
		>= FMath::Square(ExtendDistance))
	{
		SetState(EEnemyPlaneState::TurnBack);
	}

	FVector DesiredDirection = GetDesiredDirection();
	if (!AvoidanceDirection.IsNearlyZero())
	{
		DesiredDirection = (DesiredDirection + AvoidanceDirection * AvoidanceUpWeight).GetSafeNormal();
	}

	FRotator DesiredRotation = DesiredDirection.Rotation();
	DesiredRotation.Pitch = FMath::ClampAngle(DesiredRotation.Pitch, -MaxPitch, MaxPitch);
	DesiredRotation.Roll = 0.0f;
	const float ActiveTurnRate = State == EEnemyPlaneState::TurnBack
		&& StateElapsedTime >= FastTurnDelay ? FastTurnRateDegrees : TurnRateDegrees;
	const FRotator NewRotation = FMath::RInterpConstantTo(
		GetOwner()->GetActorRotation(), DesiredRotation, DeltaTime, ActiveTurnRate);
	GetOwner()->SetActorRotation(NewRotation);

	FHitResult MoveHit;
	GetOwner()->AddActorWorldOffset(
		GetOwner()->GetActorForwardVector() * GetForwardSpeed() * DeltaTime, true, &MoveHit);

	if (State == EEnemyPlaneState::AttackRun && CurrentTarget.IsValid())
	{
		const FVector ToTarget = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
		if (ToTarget.SizeSquared() <= FMath::Square(FlyPastDistance)
			|| FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToTarget) < 0.0f)
		{
			SetState(EEnemyPlaneState::Extend);
		}
	}
	else if (State == EEnemyPlaneState::TurnBack && CurrentTarget.IsValid())
	{
		const FVector ToTarget = (CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
		const float Alignment = FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToTarget);
		if (Alignment >= FMath::Cos(FMath::DegreesToRadians(AttackResumeAngleDegrees)))
		{
			SetState(EEnemyPlaneState::AttackRun);
		}
	}

	UpdateWeapon();
	UpdateServerNetworkState();
}

void UEnemyPlaneAIComponent::ScanForTarget()
{
	if (CurrentTarget.IsValid())
	{
		const float DistanceSquared = FVector::DistSquared(
			GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
		if (IsValidPlayerTarget(CurrentTarget.Get()) && DistanceSquared <= FMath::Square(LoseTargetRange))
		{
			return;
		}
		CurrentTarget.Reset();
		SetState(EEnemyPlaneState::Roaming);
	}

	APawn* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(DetectionRange);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsValidPlayerTarget(Candidate))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	if (BestTarget)
	{
		CurrentTarget = BestTarget;
		SetState(EEnemyPlaneState::AttackRun);
	}
}

void UEnemyPlaneAIComponent::UpdateObstacleAvoidance()
{
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyPlaneAvoidance), false, GetOwner());
	const FVector Start = GetOwner()->GetActorLocation();
	const FVector End = Start + GetOwner()->GetActorForwardVector() * ObstacleCheckDistance;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit, Start, End, ECC_Visibility, QueryParams);
	AvoidanceDirection = bBlocked ? FVector::UpVector : FVector::ZeroVector;
}

void UEnemyPlaneAIComponent::SetState(const EEnemyPlaneState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateElapsedTime = 0.0f;
	if (State == EEnemyPlaneState::Extend)
	{
		ExtendStart = GetOwner()->GetActorLocation();
	}
	if (State == EEnemyPlaneState::Roaming)
	{
		ChooseRoamPoint();
	}
}

void UEnemyPlaneAIComponent::ChooseRoamPoint()
{
	const FVector2D RandomCircle = FMath::RandPointInCircle(RoamRadius);
	const float HeightOffset = FMath::FRandRange(-RoamRadius * 0.25f, RoamRadius * 0.25f);
	RoamPoint = SpawnLocation + FVector(RandomCircle.X, RandomCircle.Y, HeightOffset);
}

FVector UEnemyPlaneAIComponent::GetDesiredDirection() const
{
	if (State == EEnemyPlaneState::Roaming)
	{
		return (RoamPoint - GetOwner()->GetActorLocation()).GetSafeNormal();
	}
	if (State == EEnemyPlaneState::Extend || !CurrentTarget.IsValid())
	{
		return GetOwner()->GetActorForwardVector();
	}
	return (GetTargetLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
}

FVector UEnemyPlaneAIComponent::GetTargetLocation() const
{
	if (!CurrentTarget.IsValid())
	{
		return GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector();
	}
	// RifleGun is hitscan: damage is resolved immediately, so projectile lead would aim
	// in front of the target and reduce accuracy instead of compensating travel time.
	return CurrentTarget->GetActorLocation();
}

float UEnemyPlaneAIComponent::GetForwardSpeed() const
{
	return JetStats ? JetStats->GetFlightStats().ForwardSpeed : 2250.0f;
}

void UEnemyPlaneAIComponent::UpdateWeapon()
{
	bool bShouldFire = false;
	if (RifleGun && State == EEnemyPlaneState::AttackRun && CurrentTarget.IsValid())
	{
		const FVector ToTarget = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
		bShouldFire = ToTarget.SizeSquared() <= FMath::Square(FireRange)
			&& FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToTarget) > 0.0f;
	}
	if (RifleGun && bShouldFire != bWeaponFiring)
	{
		bWeaponFiring = bShouldFire;
		RifleGun->SetFireHeld(bShouldFire);
	}
}

void UEnemyPlaneAIComponent::UpdateServerNetworkState()
{
	ServerState.Location = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = GetOwner()->GetActorForwardVector() * GetForwardSpeed();
}

void UEnemyPlaneAIComponent::OnRep_ServerState()
{
	LastNetworkStateReceiveTime = GetWorld()->GetTimeSeconds();
	if (!bReceivedInitialNetworkState)
	{
		bReceivedInitialNetworkState = true;
		GetOwner()->SetActorLocationAndRotation(ServerState.Location, ServerState.Rotation,
			false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UEnemyPlaneAIComponent::UpdateClientSmoothing(const float DeltaTime)
{
	if (!bReceivedInitialNetworkState)
	{
		return;
	}
	const float TimeSinceState = static_cast<float>(
		GetWorld()->GetTimeSeconds() - LastNetworkStateReceiveTime);
	const float ExtrapolationTime = FMath::Clamp(TimeSinceState, 0.0f, MaxClientExtrapolationTime);
	const FVector TargetLocation = FVector(ServerState.Location)
		+ FVector(ServerState.Velocity) * ExtrapolationTime;
	const FVector SmoothedLocation = FMath::VInterpTo(GetOwner()->GetActorLocation(),
		TargetLocation, DeltaTime, ClientLocationSmoothingSpeed);
	const FRotator SmoothedRotation = FMath::RInterpTo(GetOwner()->GetActorRotation(),
		ServerState.Rotation, DeltaTime, ClientRotationSmoothingSpeed);
	GetOwner()->SetActorLocationAndRotation(SmoothedLocation, SmoothedRotation,
		false, nullptr, ETeleportType::None);
}

void UEnemyPlaneAIComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEnemyPlaneAIComponent, ServerState);
}

bool UEnemyPlaneAIComponent::IsValidPlayerTarget(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
	{
		return false;
	}
	const UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	return !Health || !Health->IsDead();
}
