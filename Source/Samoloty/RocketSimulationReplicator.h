#pragma once

#include "CoreMinimal.h"
#include "CombatImpactTypes.h"
#include "GameFramework/Actor.h"
#include "RocketProjectile.h"
#include "RocketSimulationReplicator.generated.h"

USTRUCT()
struct FManagedRocketLaunchEvent
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 RocketId = 0;

	UPROPERTY()
	TSubclassOf<ARocketProjectile> RocketClass;

	UPROPERTY()
	FRocketLaunchData LaunchData;
};

USTRUCT()
struct FManagedRocketExplosionEvent
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 RocketId = 0;

	UPROPERTY()
	FCombatImpactEvent Impact;
};

USTRUCT()
struct FManagedRocketHomingUpdateEvent
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 RocketId = 0;

	UPROPERTY()
	FVector_NetQuantize100 Location;

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	UPROPERTY()
	float DistanceTraveled = 0.0f;

	UPROPERTY()
	double ServerTime = 0.0;

	UPROPERTY()
	bool bHomingDisabled = false;
};

struct FRocketVisualPerfCounters
{
	double WindowStartSeconds = 0.0;
	double TotalTickMilliseconds = 0.0;
	double MaxTickMilliseconds = 0.0;
	double TotalNdcMilliseconds = 0.0;
	double MaxNdcMilliseconds = 0.0;
	uint64 TickCount = 0;
	uint64 LaunchEventsReceived = 0;
	uint64 VisualRocketsAdded = 0;
	uint64 ExplosionEventsReceived = 0;
	uint64 HomingUpdatesReceived = 0;
	uint64 InstanceUpdates = 0;
	uint64 DirtyGroups = 0;
	uint64 NdcPublishes = 0;
	uint64 NdcRows = 0;
	uint64 DistanceExpired = 0;
	int32 MaxVisualRockets = 0;
	int32 MaxLaunchBatch = 0;
	int32 MaxUpdateBatch = 0;

	void Reset(const double StartSeconds)
	{
		*this = FRocketVisualPerfCounters();
		WindowStartSeconds = StartSeconds;
	}
};

/** One replicated event channel for every data-only rocket in a world. */
UCLASS(NotBlueprintable)
class SAMOLOTY_API ARocketSimulationReplicator : public AActor
{
	GENERATED_BODY()

public:
	ARocketSimulationReplicator();
	virtual void Tick(float DeltaSeconds) override;
	void BroadcastLaunchBatch(const TArray<FManagedRocketLaunchEvent>& Events);
	void BroadcastUpdateBatch(const TArray<FManagedRocketExplosionEvent>& ExplosionEvents,
		const TArray<FManagedRocketHomingUpdateEvent>& HomingUpdateEvents);

private:
	struct FVisualRocketRecord
	{
		TSubclassOf<ARocketProjectile> RocketClass;
		FRocketLaunchData LaunchData;
		float DistanceTraveled = 0.0f;
		float CurveLength = 0.0f;
		static constexpr int32 CurveSamples = 16;
		float CurveDistances[CurveSamples + 1] = {};
		int32 VisualGroupIndex = INDEX_NONE;
		int32 InstanceIndex = INDEX_NONE;
		int32 TrailProfileId = 0;
		TWeakObjectPtr<AActor> HomingTarget;
		TWeakObjectPtr<AActor> WarningTarget;
		FVector Location = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		bool bHomingDisabled = false;
	};

	struct FVisualRocketGroup
	{
		TObjectPtr<class UInstancedStaticMeshComponent> Instances;
		FTransform MeshRelativeTransform = FTransform::Identity;
		FTransform TrailRelativeTransform = FTransform::Identity;
		TArray<int32> FreeInstances;
	};

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastLaunchBatch(const TArray<FManagedRocketLaunchEvent>& Events);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastUpdateBatch(const TArray<FManagedRocketExplosionEvent>& ExplosionEvents,
		const TArray<FManagedRocketHomingUpdateEvent>& HomingUpdateEvents);

	int32 FindOrCreateVisualGroup(TSubclassOf<ARocketProjectile> RocketClass);
	void BuildCurveCache(FVisualRocketRecord& Record) const;
	FVector EvaluateCurve(const FVisualRocketRecord& Record, float Parameter) const;
	float FindCurveParameter(const FVisualRocketRecord& Record, float Distance) const;
	FTransform GetRocketTransform(const FVisualRocketRecord& Record) const;
	FTransform GetPathTransform(const FVisualRocketRecord& Record, float Distance) const;
	void AdvanceVisualRocket(FVisualRocketRecord& Record, float DeltaSeconds) const;
	FVector RotateDirectionTowards(const FVector& CurrentDirection,
		const FVector& DesiredDirection, float MaxTurnRateDegreesPerSecond,
		float DeltaSeconds) const;
	void ApplyHomingUpdate(const FManagedRocketHomingUpdateEvent& Event);
	FTransform GetTrailTransform(const FVisualRocketRecord& Record) const;
	void PublishTrailData();
	void ReleaseVisualRocket(uint32 RocketId, const FCombatImpactEvent* Impact);
	void FinishPerfTick(double TickStartSeconds);

	UPROPERTY()
	TObjectPtr<class UNiagaraDataChannelAsset> TrailDataChannel;

	UPROPERTY()
	TObjectPtr<class UNiagaraSystem> BatchedTrailSystem;

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> BatchedTrailComponent;

	TArray<FVisualRocketGroup> VisualGroups;
	TMap<TSubclassOf<ARocketProjectile>, int32> VisualGroupByClass;
	TMap<uint32, FVisualRocketRecord> VisualRockets;
	FRocketVisualPerfCounters PerfCounters;
	bool bPerfLoggingThisTick = false;
};
