#include "GroundEnemyAIComponent.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "HealthComponent.h"
#include "HomingMissileWeaponComponent.h"
#include "JetStatsComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "RifleGunComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

namespace GroundEnemyDebug
{
	const TCHAR* GetStateName(const EGroundEnemyState State)
	{
		switch (State)
		{
		case EGroundEnemyState::Roaming: return TEXT("Roaming");
		case EGroundEnemyState::Chasing: return TEXT("Chasing");
		case EGroundEnemyState::Attacking: return TEXT("Attacking");
		case EGroundEnemyState::Returning: return TEXT("Returning");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* GetAggressionName(const EEnemyAggressionMode Mode)
	{
		switch (Mode)
		{
		case EEnemyAggressionMode::Aggressive: return TEXT("Aggressive");
		case EEnemyAggressionMode::Defensive: return TEXT("Defensive");
		case EEnemyAggressionMode::DefensiveUntilLowHealth: return TEXT("DefensiveLowHP");
		case EEnemyAggressionMode::Passive: return TEXT("Passive");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* GetMoveStatusName(const EPathFollowingStatus::Type Status)
	{
		switch (Status)
		{
		case EPathFollowingStatus::Idle: return TEXT("Idle");
		case EPathFollowingStatus::Waiting: return TEXT("Waiting");
		case EPathFollowingStatus::Paused: return TEXT("Paused");
		case EPathFollowingStatus::Moving: return TEXT("Moving");
		default: return TEXT("Unknown");
		}
	}
}

UGroundEnemyAIComponent::UGroundEnemyAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UGroundEnemyAIComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}

	SpawnLocation = GetOwner()->GetActorLocation();
	CacheDependencies();
	GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UGroundEnemyAIComponent::HandleOwnerDamaged);
	ChooseRoamPoint();
}

void UGroundEnemyAIComponent::CacheDependencies()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AIController = OwnerPawn ? Cast<AAIController>(OwnerPawn->GetController()) : nullptr;
	Health = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr;
	JetStats = GetOwner() ? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	RifleGun = GetOwner() ? GetOwner()->FindComponentByClass<URifleGunComponent>() : nullptr;
	HomingMissiles = GetOwner() ? GetOwner()->FindComponentByClass<UHomingMissileWeaponComponent>() : nullptr;
	if (RifleGun)
	{
		RifleGun->SetCameraAimEnabled(true);
	}
}

void UGroundEnemyAIComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || (Health && Health->IsDead()))
	{
		StopWeapons();
		return;
	}
	if (!AIController)
	{
		CacheDependencies();
		if (!AIController)
		{
			return;
		}
	}

	TargetScanTimeRemaining -= DeltaTime;
	RepathTimeRemaining -= DeltaTime;
	if (TargetScanTimeRemaining <= 0.0f)
	{
		TargetScanTimeRemaining = TargetScanInterval;
		if (CanSearchForUnprovokedTargets())
		{
			ScanForTarget();
		}
	}

	if (CurrentTarget.IsValid() && !IsValidPlayerTarget(CurrentTarget.Get()))
	{
		ClearCurrentTarget();
	}

	if (CurrentTarget.IsValid())
	{
		UpdateCombat();
	}
	else if (State == EGroundEnemyState::Returning)
	{
		UpdateReturning();
	}
	else
	{
		UpdateRoaming(DeltaTime);
	}
	DrawDebugState();
}

bool UGroundEnemyAIComponent::CanSearchForUnprovokedTargets() const
{
	if (AggressionMode == EEnemyAggressionMode::Aggressive)
	{
		return true;
	}
	if (AggressionMode != EEnemyAggressionMode::DefensiveUntilLowHealth || !Health)
	{
		return false;
	}
	return Health->GetMaxHealth() > 0.0f
		&& Health->GetCurrentHealth() / Health->GetMaxHealth() <= LowHealthAggressionThreshold;
}

bool UGroundEnemyAIComponent::IsValidPlayerTarget(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || Pawn == GetOwner() || !Pawn->IsPlayerControlled())
	{
		return false;
	}
	const UHealthComponent* TargetHealth = Pawn->FindComponentByClass<UHealthComponent>();
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return false;
	}
	return FVector::DistSquared(GetOwner()->GetActorLocation(), Pawn->GetActorLocation())
		<= FMath::Square(LoseTargetRange);
}

void UGroundEnemyAIComponent::ScanForTarget()
{
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
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}
	if (BestTarget && (!CurrentTarget.IsValid() || BestTarget != CurrentTarget.Get()))
	{
		SetCurrentTarget(BestTarget);
	}
}

void UGroundEnemyAIComponent::SetCurrentTarget(APawn* NewTarget)
{
	if (!IsValidPlayerTarget(NewTarget))
	{
		return;
	}
	CurrentTarget = NewTarget;
	State = EGroundEnemyState::Chasing;
	RepathTimeRemaining = 0.0f;
	if (AIController)
	{
		AIController->SetFocus(NewTarget);
	}
}

void UGroundEnemyAIComponent::ClearCurrentTarget()
{
	CurrentTarget.Reset();
	StopWeapons();
	if (AIController)
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		AIController->StopMovement();
	}
	State = EGroundEnemyState::Returning;
	RepathTimeRemaining = 0.0f;
}

void UGroundEnemyAIComponent::UpdateCombat()
{
	APawn* Target = CurrentTarget.Get();
	if (!Target || !JetStats)
	{
		ClearCurrentTarget();
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = TargetLocation - GetOwner()->GetActorLocation();
	const float Distance = ToTarget.Size();
	const float RifleRange = bUseRifle && RifleGun
		? JetStats->GetRifleGunStats().MaxRange : 0.0f;
	const float MissileRange = bUseHomingMissiles && HomingMissiles
		? JetStats->GetHomingMissileStats().LockRange : 0.0f;
	const bool bRifleInRange = bUseRifle && RifleGun && Distance <= RifleRange
		&& (!bRequireLineOfSightForRifle || HasLineOfSightTo(Target));
	const bool bMissileInRange = bUseHomingMissiles && HomingMissiles && Distance <= MissileRange;

	AIController->SetFocus(Target);
	if (bRifleInRange || bMissileInRange)
	{
		State = EGroundEnemyState::Attacking;
		AIController->StopMovement();
	}
	else
	{
		State = EGroundEnemyState::Chasing;
		if (RepathTimeRemaining <= 0.0f)
		{
			RepathTimeRemaining = RepathInterval;
			const float LongestRange = FMath::Max(RifleRange, MissileRange);
			const float AcceptanceRadius = FMath::Max(100.0f, LongestRange * 0.85f);
			AIController->MoveToActor(Target, AcceptanceRadius, true, true, true, nullptr, true);
		}
	}

	if (RifleGun)
	{
		if (bUseRifle)
		{
			RifleGun->SetCameraAim(GetOwner()->GetActorLocation(), ToTarget.GetSafeNormal());
		}
		RifleGun->SetFireHeld(bRifleInRange);
	}
	if (HomingMissiles)
	{
		HomingMissiles->SetAITargetAndFire(Target, bMissileInRange);
	}
}

void UGroundEnemyAIComponent::UpdateRoaming(const float DeltaTime)
{
	State = EGroundEnemyState::Roaming;
	StopWeapons();
	const bool bReachedRoamPoint = FVector::DistSquared2D(
		GetOwner()->GetActorLocation(), RoamPoint)
		<= FMath::Square(RoamAcceptanceRadius);
	if (bReachedRoamPoint)
	{
		RoamIdleTime = 0.0f;
		ChooseRoamPoint();
	}
	else if (AIController)
	{
		if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			RoamIdleTime += DeltaTime;
			// A partial or failed path can finish several metres before the saved point.
			// Reject it instead of retrying the same unreachable destination forever.
			if (RoamIdleTime >= FMath::Max(0.5f, RepathInterval))
			{
				RoamIdleTime = 0.0f;
				ChooseRoamPoint();
			}
		}
		else
		{
			RoamIdleTime = 0.0f;
		}

		if (RepathTimeRemaining <= 0.0f
			&& AIController->GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			RepathTimeRemaining = RepathInterval;
			const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
				RoamPoint, RoamAcceptanceRadius, false, true, true, false, nullptr, false);
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				RoamIdleTime = FMath::Max(0.5f, RepathInterval);
			}
		}
	}
}

void UGroundEnemyAIComponent::ChooseRoamPoint()
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation NavLocation;
	if (Navigation && Navigation->GetRandomReachablePointInRadius(SpawnLocation, RoamRadius, NavLocation))
	{
		RoamPoint = NavLocation.Location;
		RoamIdleTime = 0.0f;
		RepathTimeRemaining = RepathInterval;
		if (AIController)
		{
			const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
				RoamPoint, RoamAcceptanceRadius, false, true, true, false, nullptr, false);
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				RoamIdleTime = FMath::Max(0.5f, RepathInterval);
			}
		}
	}
}

void UGroundEnemyAIComponent::UpdateReturning()
{
	StopWeapons();
	if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), SpawnLocation)
		<= FMath::Square(RoamAcceptanceRadius))
	{
		State = EGroundEnemyState::Roaming;
		ChooseRoamPoint();
		return;
	}
	if (RepathTimeRemaining <= 0.0f)
	{
		RepathTimeRemaining = RepathInterval;
		AIController->MoveToLocation(SpawnLocation, RoamAcceptanceRadius, true, true, true, false);
	}
}

void UGroundEnemyAIComponent::StopWeapons()
{
	if (RifleGun)
	{
		RifleGun->SetFireHeld(false);
	}
	if (HomingMissiles)
	{
		HomingMissiles->SetAITargetAndFire(nullptr, false);
	}
}

bool UGroundEnemyAIComponent::HasLineOfSightTo(const AActor* TargetActor) const
{
	return AIController && TargetActor && AIController->LineOfSightTo(TargetActor);
}

void UGroundEnemyAIComponent::HandleOwnerDamaged(AActor* DamagedActor, const float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.0f || AggressionMode == EEnemyAggressionMode::Passive)
	{
		return;
	}
	APawn* Attacker = InstigatedBy ? InstigatedBy->GetPawn().Get() : Cast<APawn>(DamageCauser);
	if (IsValidPlayerTarget(Attacker))
	{
		SetCurrentTarget(Attacker);
	}
}

void UGroundEnemyAIComponent::DrawDebugState() const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawAIDebug || !GetWorld() || !GetOwner())
	{
		return;
	}
	const FVector Origin = GetOwner()->GetActorLocation();
	const EPathFollowingStatus::Type MoveStatus = AIController
		? AIController->GetMoveStatus() : EPathFollowingStatus::Idle;
	FNavLocation ProjectedLocation;
	const UNavigationSystemV1* Navigation =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bOnNavMesh = Navigation && Navigation->ProjectPointToNavigation(
		Origin, ProjectedLocation, FVector(150.0f, 150.0f, 300.0f));
	const FString TargetName = CurrentTarget.IsValid()
		? CurrentTarget->GetName() : TEXT("None");
	const float RoamDistance = FVector::Dist2D(Origin, RoamPoint) / 100.0f;
	const FString DebugText = FString::Printf(
		TEXT("GROUND AI\nState: %s  Mode: %s\nMove: %s  NavMesh: %s\nTarget: %s  Roam point: %.1f m"),
		GroundEnemyDebug::GetStateName(State),
		GroundEnemyDebug::GetAggressionName(AggressionMode),
		GroundEnemyDebug::GetMoveStatusName(MoveStatus),
		bOnNavMesh ? TEXT("YES") : TEXT("NO"),
		*TargetName, RoamDistance);
	DrawDebugString(GetWorld(), Origin + FVector(0.0f, 0.0f, 180.0f), DebugText,
		nullptr, bOnNavMesh ? FColor::Green : FColor::Red, 0.15f, true, 1.15f);
	DrawDebugSphere(GetWorld(), RoamPoint, 80.0f, 12, FColor::Cyan,
		false, 0.15f, 0, 3.0f);
	DrawDebugLine(GetWorld(), Origin, RoamPoint, FColor::Cyan,
		false, 0.15f, 0, 1.5f);
	if (CurrentTarget.IsValid())
	{
		DrawDebugLine(GetWorld(), Origin, CurrentTarget->GetActorLocation(), FColor::Red, false, 0.12f, 0, 3.0f);
	}
#endif
}
