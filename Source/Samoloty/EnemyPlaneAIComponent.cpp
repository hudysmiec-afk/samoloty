#include "EnemyPlaneAIComponent.h"

#include "AircraftCollisionComponent.h"
#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RifleGunComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace EnemyPlaneAI
{
	constexpr float SteeringUpdateInterval = 0.1f;
	constexpr float MaximumBreakAwayTime = 1.75f;
	constexpr float MinimumQuickTurnTime = 0.25f;
	constexpr float AggressiveTurnDelay = 2.0f;
	constexpr float AggressiveTurnMultiplier = 1.4f;
	constexpr float QuickTurnYawBiasDegrees = 12.0f;
	constexpr float QuickTurnBiasFadeTime = 1.0f;
	constexpr float MaxVisualBankDegrees = 42.0f;
	constexpr float VisualBankResponse = 5.0f;

	const TCHAR* GetStateName(const EEnemyPlaneState State)
	{
		switch (State)
		{
		case EEnemyPlaneState::Roaming: return TEXT("Roaming");
		case EEnemyPlaneState::AttackRun: return TEXT("AttackRun");
		case EEnemyPlaneState::Extend: return TEXT("BreakAway");
		case EEnemyPlaneState::TurnBack: return TEXT("QuickTurn");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* GetAggressionName(const EEnemyAggressionMode Mode)
	{
		switch (Mode)
		{
		case EEnemyAggressionMode::Aggressive: return TEXT("Aggressive");
		case EEnemyAggressionMode::Defensive: return TEXT("Defensive");
		case EEnemyAggressionMode::DefensiveUntilLowHealth: return TEXT("LowHealth");
		case EEnemyAggressionMode::Passive: return TEXT("Passive");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* GetTargetPriorityName(const EEnemyTargetPriority Priority)
	{
		switch (Priority)
		{
		case EEnemyTargetPriority::Closest: return TEXT("Closest");
		case EEnemyTargetPriority::FirstAttacker: return TEXT("First");
		case EEnemyTargetPriority::LowestHealth: return TEXT("LowHP");
		case EEnemyTargetPriority::HighestHealth: return TEXT("HighHP");
		case EEnemyTargetPriority::Random: return TEXT("Random");
		case EEnemyTargetPriority::HighestAggro: return TEXT("Aggro");
		default: return TEXT("Unknown");
		}
	}
}

UEnemyPlaneAIComponent::UEnemyPlaneAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UEnemyPlaneAIComponent::BeginPlay()
{
	Super::BeginPlay();
	// Existing Blueprint instances may retain the old one-kilometre patrol default.
	// Keep prototype enemies within 300 m of their placed spawn point.
	RoamRadius = FMath::Min(RoamRadius, 30000.0f);
	RoamAcceptanceRadius = FMath::Min(RoamAcceptanceRadius, 3000.0f);
	RifleGun = GetOwner()->FindComponentByClass<URifleGunComponent>();
	JetStats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	AircraftCollision = GetOwner()->FindComponentByClass<UAircraftCollisionComponent>();
	SpawnLocation = GetOwner()->GetActorLocation();
	ManeuverDirection = GetOwner()->GetActorForwardVector().GetSafeNormal();
	DesiredFlightDirection = ManeuverDirection;
	TargetScanTimeRemaining = FMath::FRandRange(0.0f, TargetScanInterval);
	ObstacleCheckTimeRemaining = FMath::FRandRange(0.0f, ObstacleCheckInterval);
	SteeringUpdateTimeRemaining = FMath::FRandRange(0.0f, EnemyPlaneAI::SteeringUpdateInterval);
	TargetReevaluationTimeRemaining = TargetReevaluationInterval;
	ChooseRoamPoint();

	if (GetOwner()->HasAuthority())
	{
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UEnemyPlaneAIComponent::HandleOwnerDamaged);
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
	TimeWithoutAttackOpportunity += DeltaTime;

	TargetScanTimeRemaining -= DeltaTime;
	if (TargetScanTimeRemaining <= 0.0f)
	{
		TargetScanTimeRemaining = TargetScanInterval;
		ScanForTarget();
	}

	TargetReevaluationTimeRemaining -= DeltaTime;
	if (TargetReevaluationTimeRemaining <= 0.0f)
	{
		TargetReevaluationTimeRemaining = FMath::Max(1.0f, TargetReevaluationInterval);
		ReevaluateTarget();
	}

	ObstacleCheckTimeRemaining -= DeltaTime;
	if (ObstacleCheckTimeRemaining <= 0.0f)
	{
		ObstacleCheckTimeRemaining = ObstacleCheckInterval;
		UpdateObstacleAvoidance();
	}

	SteeringUpdateTimeRemaining -= DeltaTime;
	if (SteeringUpdateTimeRemaining <= 0.0f)
	{
		SteeringUpdateTimeRemaining += EnemyPlaneAI::SteeringUpdateInterval;
		UpdateDesiredFlightDirection();
	}
	TurnTowardDesiredDirection(DeltaTime);

	const FVector RequestedVelocity = GetOwner()->GetActorForwardVector() * GetForwardSpeed();
	if (AircraftCollision)
	{
		AircraftCollision->MoveOwner(RequestedVelocity * DeltaTime, RequestedVelocity);
	}
	else
	{
		GetOwner()->AddActorWorldOffset(RequestedVelocity * DeltaTime, false);
	}

	UpdateStateTransitions();
	UpdateWeapon();
	UpdateServerNetworkState();
	DrawAIDebug();
}

void UEnemyPlaneAIComponent::ScanForTarget()
{
	CleanupAggroData();

	if (AggressionMode == EEnemyAggressionMode::Passive)
	{
		ClearCurrentTarget();
		return;
	}

	if (CurrentTarget.IsValid())
	{
		const float DistanceSquared = FVector::DistSquared(
			GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
		const bool bInsideLoseRange = DistanceSquared <= FMath::Square(LoseTargetRange);
		const bool bRecentlyEngaged = TimeWithoutAttackOpportunity < LoseTargetAfterNoAttackTime;
		if (IsValidPlayerTarget(CurrentTarget.Get()) && (bInsideLoseRange || bRecentlyEngaged))
		{
			return;
		}
		ClearCurrentTarget();
	}

	TArray<APawn*> Candidates;
	CollectTargetCandidates(Candidates);
	SetCurrentTarget(SelectPreferredTarget(Candidates));
}

void UEnemyPlaneAIComponent::ReevaluateTarget()
{
	if (!bCanChangeTarget || !CurrentTarget.IsValid()
		|| TargetPriority == EEnemyTargetPriority::FirstAttacker
		|| AggressionMode == EEnemyAggressionMode::Passive)
	{
		return;
	}

	CleanupAggroData();
	if (TargetPriority == EEnemyTargetPriority::Random && FMath::RandRange(0, 99) >= 30)
	{
		return;
	}

	TArray<APawn*> Candidates;
	CollectTargetCandidates(Candidates);
	if (APawn* PreferredTarget = SelectPreferredTarget(Candidates))
	{
		SetCurrentTarget(PreferredTarget);
	}

	if (TargetPriority == EEnemyTargetPriority::HighestAggro)
	{
		for (TPair<TWeakObjectPtr<APawn>, float>& AggroEntry : AggroByTarget)
		{
			AggroEntry.Value = 0.0f;
		}
	}
}

void UEnemyPlaneAIComponent::SetCurrentTarget(APawn* NewTarget)
{
	if (!IsValidPlayerTarget(NewTarget) || AggressionMode == EEnemyAggressionMode::Passive)
	{
		return;
	}
	if (CurrentTarget.Get() == NewTarget)
	{
		return;
	}

	CurrentTarget = NewTarget;
	TimeWithoutAttackOpportunity = 0.0f;
	TargetReevaluationTimeRemaining = FMath::Max(1.0f, TargetReevaluationInterval);
	SetState(EEnemyPlaneState::AttackRun);
}

void UEnemyPlaneAIComponent::ClearCurrentTarget()
{
	if (!CurrentTarget.IsValid() && State == EEnemyPlaneState::Roaming)
	{
		return;
	}
	CurrentTarget.Reset();
	TimeWithoutAttackOpportunity = 0.0f;
	SetState(EEnemyPlaneState::Roaming);
}

void UEnemyPlaneAIComponent::CollectTargetCandidates(TArray<APawn*>& OutCandidates) const
{
	OutCandidates.Reset();
	if (AggressionMode == EEnemyAggressionMode::Passive)
	{
		return;
	}

	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	if (CanSearchForUnprovokedTargets())
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			APawn* Candidate = *It;
			if (IsValidPlayerTarget(Candidate)
				&& FVector::DistSquared(OwnerLocation, Candidate->GetActorLocation())
				<= FMath::Square(DetectionRange))
			{
				OutCandidates.AddUnique(Candidate);
			}
		}
	}

	for (const TWeakObjectPtr<APawn>& Attacker : AttackerOrder)
	{
		APawn* Candidate = Attacker.Get();
		if (IsValidPlayerTarget(Candidate)
			&& FVector::DistSquared(OwnerLocation, Candidate->GetActorLocation())
			<= FMath::Square(LoseTargetRange))
		{
			OutCandidates.AddUnique(Candidate);
		}
	}

	if (IsValidPlayerTarget(CurrentTarget.Get())
		&& FVector::DistSquared(OwnerLocation, CurrentTarget->GetActorLocation())
		<= FMath::Square(LoseTargetRange))
	{
		OutCandidates.AddUnique(CurrentTarget.Get());
	}
}

APawn* UEnemyPlaneAIComponent::SelectPreferredTarget(const TArray<APawn*>& Candidates) const
{
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	auto FindClosest = [this, &Candidates]() -> APawn*
	{
		APawn* ClosestTarget = nullptr;
		float ClosestDistanceSquared = TNumericLimits<float>::Max();
		for (APawn* Candidate : Candidates)
		{
			const float DistanceSquared = FVector::DistSquared(
				GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestTarget = Candidate;
				ClosestDistanceSquared = DistanceSquared;
			}
		}
		return ClosestTarget;
	};

	switch (TargetPriority)
	{
	case EEnemyTargetPriority::FirstAttacker:
		for (const TWeakObjectPtr<APawn>& Attacker : AttackerOrder)
		{
			if (APawn* AttackerPawn = Attacker.Get(); Candidates.Contains(AttackerPawn))
			{
				return AttackerPawn;
			}
		}
		return FindClosest();

	case EEnemyTargetPriority::LowestHealth:
	case EEnemyTargetPriority::HighestHealth:
		{
			APawn* BestTarget = nullptr;
			float BestHealth = TargetPriority == EEnemyTargetPriority::LowestHealth
				? TNumericLimits<float>::Max() : -1.0f;
			for (APawn* Candidate : Candidates)
			{
				const float Health = GetCurrentHealthValue(Candidate);
				const bool bBetter = TargetPriority == EEnemyTargetPriority::LowestHealth
					? Health < BestHealth : Health > BestHealth;
				if (bBetter)
				{
					BestTarget = Candidate;
					BestHealth = Health;
				}
			}
			return BestTarget;
		}

	case EEnemyTargetPriority::Random:
		return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	case EEnemyTargetPriority::HighestAggro:
		{
			APawn* BestTarget = nullptr;
			float HighestAggro = 0.0f;
			for (APawn* Candidate : Candidates)
			{
				const float Aggro = GetAggroForTarget(Candidate);
				if (Aggro > HighestAggro)
				{
					BestTarget = Candidate;
					HighestAggro = Aggro;
				}
			}
			return BestTarget ? BestTarget : (CurrentTarget.IsValid() ? CurrentTarget.Get() : FindClosest());
		}

	case EEnemyTargetPriority::Closest:
	default:
		return FindClosest();
	}
}

void UEnemyPlaneAIComponent::CleanupAggroData()
{
	AttackerOrder.RemoveAll([this](const TWeakObjectPtr<APawn>& Attacker)
	{
		return !IsValidPlayerTarget(Attacker.Get());
	});

	for (auto It = AggroByTarget.CreateIterator(); It; ++It)
	{
		if (!IsValidPlayerTarget(It.Key().Get()))
		{
			It.RemoveCurrent();
		}
	}
}

bool UEnemyPlaneAIComponent::CanSearchForUnprovokedTargets() const
{
	if (AggressionMode == EEnemyAggressionMode::Aggressive)
	{
		return true;
	}
	if (AggressionMode != EEnemyAggressionMode::DefensiveUntilLowHealth)
	{
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return GetHealthFraction(OwnerPawn) <= LowHealthAggressionThreshold;
}

float UEnemyPlaneAIComponent::GetHealthFraction(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return 1.0f;
	}
	const UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	if (!Health || Health->GetMaxHealth() <= SMALL_NUMBER)
	{
		return 1.0f;
	}
	return Health->GetCurrentHealth() / Health->GetMaxHealth();
}

float UEnemyPlaneAIComponent::GetCurrentHealthValue(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return 0.0f;
	}
	const UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	return Health ? Health->GetCurrentHealth() : 0.0f;
}

float UEnemyPlaneAIComponent::GetAggroForTarget(APawn* Pawn) const
{
	if (!Pawn)
	{
		return 0.0f;
	}
	if (const float* Aggro = AggroByTarget.Find(TWeakObjectPtr<APawn>(Pawn)))
	{
		return *Aggro;
	}
	return 0.0f;
}

void UEnemyPlaneAIComponent::HandleOwnerDamaged(AActor* DamagedActor, const float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (!GetOwner()->HasAuthority() || Damage <= 0.0f
		|| AggressionMode == EEnemyAggressionMode::Passive)
	{
		return;
	}

	APawn* Attacker = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
	if (!Attacker && DamageCauser)
	{
		Attacker = Cast<APawn>(DamageCauser);
		if (!Attacker)
		{
			Attacker = DamageCauser->GetInstigator();
		}
		if (!Attacker)
		{
			Attacker = Cast<APawn>(DamageCauser->GetOwner());
		}
	}
	if (!IsValidPlayerTarget(Attacker) || Attacker == GetOwner())
	{
		return;
	}

	AggroByTarget.FindOrAdd(TWeakObjectPtr<APawn>(Attacker)) += Damage;
	AttackerOrder.AddUnique(TWeakObjectPtr<APawn>(Attacker));
	if (!CurrentTarget.IsValid())
	{
		SetCurrentTarget(Attacker);
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

void UEnemyPlaneAIComponent::UpdateStateTransitions()
{
	if (!CurrentTarget.IsValid())
	{
		if (State != EEnemyPlaneState::Roaming)
		{
			SetState(EEnemyPlaneState::Roaming);
		}
		if (FVector::DistSquared(GetOwner()->GetActorLocation(), RoamPoint)
			<= FMath::Square(RoamAcceptanceRadius))
		{
			ChooseRoamPoint();
		}
		return;
	}

	const FVector ToTarget = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
	const float DistanceSquared = ToTarget.SizeSquared();
	const FVector TargetDirection = ToTarget.GetSafeNormal();
	const float Alignment = FVector::DotProduct(GetOwner()->GetActorForwardVector(), TargetDirection);

	if (State == EEnemyPlaneState::AttackRun)
	{
		if (DistanceSquared <= FMath::Square(FlyPastDistance) || Alignment < -0.05f)
		{
			SetState(EEnemyPlaneState::Extend);
		}
	}
	else if (State == EEnemyPlaneState::Extend)
	{
		const bool bFarEnough = FVector::DistSquared(GetOwner()->GetActorLocation(), ExtendStart)
			>= FMath::Square(ExtendDistance);
		if (bFarEnough || StateElapsedTime >= EnemyPlaneAI::MaximumBreakAwayTime)
		{
			SetState(EEnemyPlaneState::TurnBack);
		}
	}
	else if (State == EEnemyPlaneState::TurnBack)
	{
		const float RequiredAlignment = FMath::Cos(FMath::DegreesToRadians(AttackResumeAngleDegrees));
		if (StateElapsedTime >= EnemyPlaneAI::MinimumQuickTurnTime && Alignment >= RequiredAlignment)
		{
			SetState(EEnemyPlaneState::AttackRun);
		}
	}
}

void UEnemyPlaneAIComponent::UpdateDesiredFlightDirection()
{
	FVector NewDirection = GetDesiredDirection();
	if (!AvoidanceDirection.IsNearlyZero())
	{
		NewDirection = (NewDirection + AvoidanceDirection * AvoidanceUpWeight).GetSafeNormal();
	}
	if (!NewDirection.IsNearlyZero())
	{
		DesiredFlightDirection = NewDirection;
	}
}

void UEnemyPlaneAIComponent::TurnTowardDesiredDirection(const float DeltaTime)
{
	if (DesiredFlightDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentRotation = GetOwner()->GetActorRotation();
	const FRotator DesiredRotation = DesiredFlightDirection.Rotation();
	const float ActiveTurnRate = GetActiveTurnRate();
	float YawDelta = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, DesiredRotation.Yaw);

	// A target directly behind gives two equally short turns. Keep the side selected
	// when QuickTurn began so the NPC does not reverse its turn from one update to another.
	if (State == EEnemyPlaneState::TurnBack && FMath::Abs(YawDelta) > 120.0f)
	{
		if (QuickTurnSide > 0.0f && YawDelta < 0.0f)
		{
			YawDelta += 360.0f;
		}
		else if (QuickTurnSide < 0.0f && YawDelta > 0.0f)
		{
			YawDelta -= 360.0f;
		}
	}

	const float YawStep = FMath::Clamp(YawDelta, -ActiveTurnRate * DeltaTime, ActiveTurnRate * DeltaTime);
	const float PitchDelta = FMath::FindDeltaAngleDegrees(CurrentRotation.Pitch, DesiredRotation.Pitch);
	const float PitchStep = FMath::Clamp(PitchDelta, -ActiveTurnRate * DeltaTime, ActiveTurnRate * DeltaTime);
	const float AppliedYawRate = DeltaTime > SMALL_NUMBER ? YawStep / DeltaTime : 0.0f;
	const float TargetBank = FMath::Clamp(
		AppliedYawRate / FMath::Max(TurnRateDegrees, 1.0f) * EnemyPlaneAI::MaxVisualBankDegrees,
		-EnemyPlaneAI::MaxVisualBankDegrees, EnemyPlaneAI::MaxVisualBankDegrees);

	FRotator NewRotation;
	NewRotation.Yaw = CurrentRotation.Yaw + YawStep;
	NewRotation.Pitch = FMath::Clamp(CurrentRotation.Pitch + PitchStep, -MaxPitch, MaxPitch);
	NewRotation.Roll = FMath::FInterpTo(CurrentRotation.Roll, TargetBank, DeltaTime,
		EnemyPlaneAI::VisualBankResponse);
	GetOwner()->SetActorRotation(NewRotation.GetNormalized());
}

void UEnemyPlaneAIComponent::SetState(const EEnemyPlaneState NewState)
{
	if (State == NewState)
	{
		return;
	}
	const EEnemyPlaneState PreviousState = State;
	State = NewState;
	StateElapsedTime = 0.0f;
	if (State == EEnemyPlaneState::Extend)
	{
		ExtendStart = GetOwner()->GetActorLocation();
		ManeuverDirection = GetOwner()->GetActorForwardVector().GetSafeNormal();
	}
	else if (State == EEnemyPlaneState::TurnBack)
	{
		QuickTurnSide = FMath::RandBool() ? 1.0f : -1.0f;
		QuickTurnPitchBias = FMath::FRandRange(-7.0f, 7.0f);
	}
	if (State == EEnemyPlaneState::Roaming)
	{
		ChooseRoamPoint();
	}
	else if (State == EEnemyPlaneState::AttackRun && PreviousState == EEnemyPlaneState::Roaming)
	{
		TimeWithoutAttackOpportunity = 0.0f;
	}
	UpdateDesiredFlightDirection();
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
	if (State == EEnemyPlaneState::Extend)
	{
		return ManeuverDirection;
	}
	if (!CurrentTarget.IsValid())
	{
		return GetOwner()->GetActorForwardVector();
	}

	FVector TargetDirection = (GetTargetLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
	if (State == EEnemyPlaneState::TurnBack)
	{
		const float BiasAlpha = 1.0f - FMath::Clamp(
			StateElapsedTime / EnemyPlaneAI::QuickTurnBiasFadeTime, 0.0f, 1.0f);
		FRotator BiasedRotation = TargetDirection.Rotation();
		BiasedRotation.Yaw += QuickTurnSide * EnemyPlaneAI::QuickTurnYawBiasDegrees * BiasAlpha;
		BiasedRotation.Pitch = FMath::Clamp(
			BiasedRotation.Pitch + QuickTurnPitchBias * BiasAlpha, -MaxPitch, MaxPitch);
		TargetDirection = BiasedRotation.Vector();
	}
	return TargetDirection;
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

float UEnemyPlaneAIComponent::GetActiveTurnRate() const
{
	if (State == EEnemyPlaneState::TurnBack && StateElapsedTime >= FastTurnDelay)
	{
		return FMath::Min(FastTurnRateDegrees, TurnRateDegrees * 2.0f);
	}
	if (State == EEnemyPlaneState::AttackRun
		&& TimeWithoutAttackOpportunity >= EnemyPlaneAI::AggressiveTurnDelay)
	{
		return TurnRateDegrees * EnemyPlaneAI::AggressiveTurnMultiplier;
	}
	if (State == EEnemyPlaneState::Roaming)
	{
		return TurnRateDegrees * 0.7f;
	}
	return TurnRateDegrees;
}

void UEnemyPlaneAIComponent::UpdateWeapon()
{
	bool bShouldFire = false;
	if (RifleGun && State == EEnemyPlaneState::AttackRun && CurrentTarget.IsValid())
	{
		const FVector ToTarget = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
		const FVector TargetDirection = ToTarget.GetSafeNormal();
		bShouldFire = ToTarget.SizeSquared() <= FMath::Square(FireRange)
			&& FVector::DotProduct(GetOwner()->GetActorForwardVector(), TargetDirection)
			>= FMath::Cos(FMath::DegreesToRadians(FireAimHalfAngleDegrees));
		if (bShouldFire)
		{
			TimeWithoutAttackOpportunity = 0.0f;
		}
	}
	if (RifleGun && bShouldFire != bWeaponFiring)
	{
		bWeaponFiring = bShouldFire;
		RifleGun->SetFireHeld(bShouldFire);
	}
}

void UEnemyPlaneAIComponent::DrawAIDebug() const
{
	if (!bDrawAIDebug || !GetWorld())
	{
		return;
	}

	const FVector Location = GetOwner()->GetActorLocation();
	DrawDebugDirectionalArrow(GetWorld(), Location, Location + DesiredFlightDirection * 2000.0f,
		250.0f, FColor::Cyan, false, 0.0f, 0, 3.0f);
	if (CurrentTarget.IsValid())
	{
		DrawDebugLine(GetWorld(), Location, CurrentTarget->GetActorLocation(),
			bWeaponFiring ? FColor::Green : FColor::Yellow, false, 0.0f, 0, 1.0f);
	}
	DrawDebugString(GetWorld(), Location + FVector(0.0f, 0.0f, 300.0f),
		FString::Printf(TEXT("%s | %s / %s | fire=%s | aggro=%.0f"),
			EnemyPlaneAI::GetStateName(State),
			EnemyPlaneAI::GetAggressionName(AggressionMode),
			EnemyPlaneAI::GetTargetPriorityName(TargetPriority),
			bWeaponFiring ? TEXT("yes") : TEXT("no"),
			GetAggroForTarget(CurrentTarget.Get())),
		nullptr, FColor::White, 0.0f, true);
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
