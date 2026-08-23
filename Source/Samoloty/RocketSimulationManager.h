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
	TWeakObjectPtr<UObject> SourceWeapon;
	TWeakObjectPtr<AController> InstigatorController;
	TSubclassOf<ARocketProjectile> RocketClass;
	TWeakObjectPtr<AActor> HomingTarget;
	FVector StartLocation = FVector::ZeroVector;
	FVector ControlPoint1 = FVector::ZeroVector;
	FVector ControlPoint2 = FVector::ZeroVector;
	FVector SeparationEndPoint = FVector::ZeroVector;
	FVector Location = FVector::ZeroVector;
	/** Immutable direction used after the authored launch-separation curve. */
	FVector PathDirection = FVector::ForwardVector;
	/** Current movement direction; mutable only for simulation/corrections. */
	FVector Direction = FVector::ForwardVector;
	float Speed = 0.0f;
	float Damage = 0.0f;
	float MaxTravelDistance = 0.0f;
	float ProximityRadius = 0.0f;
	float ProximityCheckInterval = 0.1f;
	float TimeSinceProximityCheck = 0.0f;
	float DistanceTraveled = 0.0f;
	float PhysicalCollisionRadius = 25.0f;
	float MaxTurnRateDegreesPerSecond = 0.0f;
	float SeparationCurveLength = 0.0f;
	static constexpr int32 CurveSamples = 16;
	float CurveCumulativeDistances[CurveSamples + 1] = {};
	uint32 RocketId = 0;
	ERocketGuidanceMode GuidanceMode = ERocketGuidanceMode::Straight;
	bool bUseSeparationCurve = false;
	bool bHomingDisabled = false;
	bool bTerminalCorrectionHandled = false;
	bool bDrawDebug = false;
	bool bDataOnly = false;
	bool bActive = false;
};

struct FRocketSimulationPerfCounters
{
	double WindowStartSeconds = 0.0;
	double TotalTickMilliseconds = 0.0;
	double MaxTickMilliseconds = 0.0;
	uint64 TickCount = 0;
	uint64 DataOnlySimulations = 0;
	uint64 ActorSimulations = 0;
	uint64 Sweeps = 0;
	uint64 SweepHits = 0;
	uint64 ProximityChecks = 0;
	uint64 OverlapQueries = 0;
	uint64 LaunchEvents = 0;
	uint64 LaunchBatches = 0;
	uint64 LargeLaunchBatches = 0;
	uint64 ExplosionEvents = 0;
	uint64 HomingUpdateEvents = 0;
	uint64 TerminalCorrectionsSent = 0;
	uint64 TerminalCorrectionsSkipped = 0;
	uint64 UpdateBatches = 0;
	int32 MaxActiveRockets = 0;
	int32 MaxLaunchBatch = 0;
	int32 MaxUpdateBatch = 0;

	void Reset(const double StartSeconds)
	{
		*this = FRocketSimulationPerfCounters();
		WindowStartSeconds = StartSeconds;
	}
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
	int32 LaunchDataOnlyRocket(TSubclassOf<ARocketProjectile> RocketClass,
		const FRocketLaunchData& LaunchData, AActor* Owner, APawn* Instigator,
		UObject* SourceWeapon);
	void UnregisterRocket(int32 Handle);

	UFUNCTION(BlueprintPure, Category="Rocket|Simulation")
	int32 GetActiveRocketCount() const { return ActiveRocketCount; }
	int32 GetActiveRocketCountForSource(const UObject* SourceWeapon) const;

private:
	void SimulateDataOnlyRocket(int32 Handle, float DeltaTime);
	void BuildCurveCache(FManagedRocketRecord& Record);
	FVector EvaluateCurve(const FManagedRocketRecord& Record, float Parameter) const;
	float FindCurveParameter(const FManagedRocketRecord& Record, float Distance) const;
	FVector GetLocationAtDistance(const FManagedRocketRecord& Record, float Distance) const;
	void ExplodeDataOnlyRocket(int32 Handle, AActor* DamageTarget,
		const FVector& ImpactLocation, const FHitResult* PhysicalHit);
	FVector RotateDirectionTowards(const FVector& CurrentDirection,
		const FVector& DesiredDirection, float MaxTurnRateDegreesPerSecond,
		float DeltaTime) const;
	AActor* GetLiveHomingTarget(const FManagedRocketRecord& Record) const;
	void QueueHomingUpdate(FManagedRocketRecord& Record);
	void FinishPerfTick(double TickStartSeconds);

	TArray<FManagedRocketRecord> Records;
	TArray<int32> FreeHandles;
	int32 ActiveRocketCount = 0;
	uint32 NextRocketId = 1;
	TWeakObjectPtr<class ARocketSimulationReplicator> Replicator;
	TArray<FManagedRocketLaunchEvent> PendingLaunchEvents;
	TArray<FManagedRocketExplosionEvent> PendingExplosionEvents;
	TArray<FManagedRocketHomingUpdateEvent> PendingHomingUpdateEvents;
	FRocketSimulationPerfCounters PerfCounters;
	int32 TerminalHomingCorrectionsThisTick = 0;
	bool bPerfLoggingThisTick = false;
};
