#include "EnemySpawner.h"

#include "AI/NavigationSystemBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	const int32 SpawnNow = FMath::Min(InitialSpawnCount, MaxAliveEnemies);
	for (int32 Index = 0; Index < SpawnNow; ++Index)
	{
		SpawnEnemy();
	}

	if (GetAliveEnemyCount() < MaxAliveEnemies)
	{
		NextSpawnTime = GetWorld()->GetTimeSeconds() + RespawnDelay;
	}

	GetWorldTimerManager().SetTimer(
		PopulationTimer, this, &AEnemySpawner::MaintainPopulation, 0.25f, true);
}

void AEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PopulationTimer);
	Super::EndPlay(EndPlayReason);
}

bool AEnemySpawner::SpawnEnemy()
{
	if (!HasAuthority() || !EnemyClass || GetAliveEnemyCount() >= MaxAliveEnemies)
	{
		return false;
	}

	APawn* SpawnedEnemy = nullptr;
	for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxSpawnAttempts); ++Attempt)
	{
		FTransform SpawnTransform;
		if (!FindSpawnTransform(SpawnTransform))
		{
			continue;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		SpawnedEnemy = GetWorld()->SpawnActor<APawn>(
			EnemyClass, SpawnTransform, SpawnParameters);
		if (SpawnedEnemy)
		{
			break;
		}
	}
	if (!SpawnedEnemy)
	{
		return false;
	}

	SpawnedEnemies.Add(SpawnedEnemy);
	SpawnedEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	return true;
}

int32 AEnemySpawner::GetAliveEnemyCount() const
{
	int32 AliveCount = 0;
	for (const APawn* Enemy : SpawnedEnemies)
	{
		if (IsValid(Enemy) && !Enemy->IsActorBeingDestroyed())
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

void AEnemySpawner::HandleSpawnedEnemyDestroyed(AActor* DestroyedActor)
{
	SpawnedEnemies.RemoveAll(
		[DestroyedActor](const TObjectPtr<APawn>& Enemy)
		{
			return !IsValid(Enemy) || Enemy == DestroyedActor;
		});

	if (HasAuthority() && NextSpawnTime < 0.0)
	{
		NextSpawnTime = GetWorld()->GetTimeSeconds() + RespawnDelay;
	}
}

void AEnemySpawner::MaintainPopulation()
{
	if (!HasAuthority())
	{
		return;
	}

	RemoveInvalidEnemies();
	if (GetAliveEnemyCount() >= MaxAliveEnemies)
	{
		NextSpawnTime = -1.0;
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (NextSpawnTime < 0.0)
	{
		NextSpawnTime = Now + RespawnDelay;
		return;
	}
	if (Now < NextSpawnTime)
	{
		return;
	}

	if (SpawnEnemy() && GetAliveEnemyCount() >= MaxAliveEnemies)
	{
		NextSpawnTime = -1.0;
	}
	else
	{
		// Also prevents a blocked spawn point from retrying every frame.
		NextSpawnTime = Now + RespawnDelay;
	}
}

void AEnemySpawner::RemoveInvalidEnemies()
{
	SpawnedEnemies.RemoveAll(
		[](const TObjectPtr<APawn>& Enemy)
		{
			return !IsValid(Enemy) || Enemy->IsActorBeingDestroyed();
		});
}

bool AEnemySpawner::FindSpawnTransform(FTransform& OutTransform) const
{
	if (!GetWorld())
	{
		return false;
	}

	FVector SpawnLocation = GetActorLocation();
	const FVector2D RandomOffset = FMath::RandPointInCircle(SpawnRadius);
	SpawnLocation.X += RandomOffset.X;
	SpawnLocation.Y += RandomOffset.Y;

	if (bProjectToNavigation)
	{
		const UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		FNavLocation ProjectedLocation;
		if (!NavigationSystem || !NavigationSystem->ProjectPointToNavigation(
			SpawnLocation, ProjectedLocation, NavigationProjectionExtent))
		{
			return false;
		}
		SpawnLocation = ProjectedLocation.Location;
		if (const APawn* DefaultPawn = EnemyClass->GetDefaultObject<APawn>())
		{
			if (const UCapsuleComponent* Capsule =
				DefaultPawn->FindComponentByClass<UCapsuleComponent>())
			{
				SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight()
					+ GroundSpawnClearance;
			}
		}
	}

	SpawnLocation.Z += SpawnHeightOffset;
	const float MinSeparationSquared = FMath::Square(MinimumSpawnSeparation);
	for (const APawn* ExistingEnemy : SpawnedEnemies)
	{
		if (IsValid(ExistingEnemy)
			&& FVector::DistSquared2D(ExistingEnemy->GetActorLocation(), SpawnLocation)
				< MinSeparationSquared)
		{
			return false;
		}
	}
	FRotator SpawnRotation = GetActorRotation();
	if (bRandomizeYaw)
	{
		SpawnRotation.Yaw = FMath::FRandRange(-180.0f, 180.0f);
	}
	OutTransform = FTransform(SpawnRotation, SpawnLocation);
	return true;
}
