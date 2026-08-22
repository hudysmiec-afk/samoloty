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

/** One replicated event channel for every data-only rocket in a world. */
UCLASS(NotBlueprintable)
class SAMOLOTY_API ARocketSimulationReplicator : public AActor
{
	GENERATED_BODY()

public:
	ARocketSimulationReplicator();
	virtual void Tick(float DeltaSeconds) override;
	void BroadcastLaunchBatch(const TArray<FManagedRocketLaunchEvent>& Events);
	void BroadcastExplosionBatch(const TArray<FManagedRocketExplosionEvent>& Events);

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
	void MulticastExplosionBatch(const TArray<FManagedRocketExplosionEvent>& Events);

	int32 FindOrCreateVisualGroup(TSubclassOf<ARocketProjectile> RocketClass);
	void BuildCurveCache(FVisualRocketRecord& Record) const;
	FVector EvaluateCurve(const FVisualRocketRecord& Record, float Parameter) const;
	float FindCurveParameter(const FVisualRocketRecord& Record, float Distance) const;
	FTransform GetRocketTransform(const FVisualRocketRecord& Record) const;
	FTransform GetTrailTransform(const FVisualRocketRecord& Record) const;
	void PublishTrailData();
	void ReleaseVisualRocket(uint32 RocketId, const FCombatImpactEvent* Impact);

	UPROPERTY()
	TObjectPtr<class UNiagaraDataChannelAsset> TrailDataChannel;

	UPROPERTY()
	TObjectPtr<class UNiagaraSystem> BatchedTrailSystem;

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> BatchedTrailComponent;

	TArray<FVisualRocketGroup> VisualGroups;
	TMap<TSubclassOf<ARocketProjectile>, int32> VisualGroupByClass;
	TMap<uint32, FVisualRocketRecord> VisualRockets;
};
