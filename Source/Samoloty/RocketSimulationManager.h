#pragma once

#include "CoreMinimal.h"
#include "RocketProjectile.h"
#include "RocketSimulationReplicator.h"
#include "Subsystems/WorldSubsystem.h"
#include "RocketSimulationManager.generated.h"

struct FManagedRocketRecord
{
	TWeakObjectPtr<ARocketProjectile> Projectile;
	TWeakObjectPtr<AActor> Owner;
	TWeakObjectPtr<AController> InstigatorController;
	TSubclassOf<ARocketProjectile> RocketClass;
	FVector StartLocation = FVector::ZeroVector;
	FVector ControlPoint1 = FVector::ZeroVector;
	FVector ControlPoint2 = FVector::ZeroVector;
	FVector SeparationEndPoint = FVector::ZeroVector;
	FVector Location = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	float Speed = 0.0f;
	float Damage = 0.0f;
	float MaxTravelDistance = 0.0f;
	float ProximityRadius = 0.0f;
	float ProximityCheckInterval = 0.1f;
	float TimeSinceProximityCheck = 0.0f;
	float DistanceTraveled = 0.0f;
	float PhysicalCollisionRadius = 25.0f;
	float SeparationCurveLength = 0.0f;
	static constexpr int32 CurveSamples = 16;
	float CurveCumulativeDistances[CurveSamples + 1] = {};
	uint32 RocketId = 0;
	bool bUseSeparationCurve = false;
	bool bDrawDebug = false;
	bool bDataOnly = false;
	bool bActive = false;
};

/** Shared world-level scheduler and authoritative data-only rocket simulation. */
UCLASS()
class SAMOLOTY_API URocketSimulationManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	int32 RegisterRocket(ARocketProjectile* Projectile,
		const FRocketLaunchData& LaunchData, AActor* Owner);
	int32 LaunchDataOnlyStraightRocket(TSubclassOf<ARocketProjectile> RocketClass,
		const FRocketLaunchData& LaunchData, AActor* Owner, APawn* Instigator);
	void UnregisterRocket(int32 Handle);

	UFUNCTION(BlueprintPure, Category="Rocket|Simulation")
	int32 GetActiveRocketCount() const { return ActiveRocketCount; }

private:
	void SimulateDataOnlyRocket(int32 Handle, float DeltaTime);
	void BuildCurveCache(FManagedRocketRecord& Record);
	FVector EvaluateCurve(const FManagedRocketRecord& Record, float Parameter) const;
	float FindCurveParameter(const FManagedRocketRecord& Record, float Distance) const;
	FVector GetLocationAtDistance(const FManagedRocketRecord& Record, float Distance) const;
	void ExplodeDataOnlyRocket(int32 Handle, AActor* DamageTarget,
		const FVector& ImpactLocation, const FHitResult* PhysicalHit);

	TArray<FManagedRocketRecord> Records;
	TArray<int32> FreeHandles;
	int32 ActiveRocketCount = 0;
	uint32 NextRocketId = 1;
	TWeakObjectPtr<class ARocketSimulationReplicator> Replicator;
	TArray<FManagedRocketLaunchEvent> PendingLaunchEvents;
	TArray<FManagedRocketExplosionEvent> PendingExplosionEvents;
};
