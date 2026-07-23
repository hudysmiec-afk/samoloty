#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RocketProjectile.generated.h"

class UHealthComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FRocketFinishedSignature, class ARocketProjectile*);

UENUM(BlueprintType)
enum class ERocketGuidanceMode : uint8
{
	Straight,
	Homing
};

USTRUCT()
struct FRocketLaunchData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 StartLocation;

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	UPROPERTY()
	FVector_NetQuantize100 SeparationControlPoint1;

	UPROPERTY()
	FVector_NetQuantize100 SeparationControlPoint2;

	UPROPERTY()
	FVector_NetQuantize100 SeparationEndPoint;

	UPROPERTY()
	bool bUseSeparationCurve = false;

	UPROPERTY()
	float Speed = 0.0f;

	UPROPERTY()
	float Damage = 0.0f;

	UPROPERTY()
	float MaxTravelDistance = 0.0f;

	UPROPERTY()
	float ProximityRadius = 0.0f;

	UPROPERTY()
	float ProximityCheckInterval = 0.1f;

	UPROPERTY()
	double ServerStartTime = 0.0;

	UPROPERTY()
	ERocketGuidanceMode GuidanceMode = ERocketGuidanceMode::Straight;

	UPROPERTY()
	bool bDrawDebug = false;
};

UCLASS(Blueprintable)
class SAMOLOTY_API ARocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARocketProjectile();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeRocket(const FRocketLaunchData& InLaunchData);

	static int32 GetServerActiveRocketCount() { return ServerActiveRocketCount; }

	FRocketFinishedSignature OnRocketFinished;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rocket|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rocket|Components")
	TObjectPtr<UStaticMeshComponent> RocketMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rocket|Components")
	TObjectPtr<UNiagaraComponent> RocketTrail;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Effects")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Collision", meta=(ClampMin="1"))
	float PhysicalCollisionRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Effects", meta=(ClampMin="0.05"))
	float ExplosionReplicationDelay = 0.25f;

private:
	UFUNCTION()
	void OnRep_LaunchData();

	UFUNCTION()
	void OnRep_Exploded();

	void ApplyLaunchState();
	void SimulateRocketMovement(float DeltaSeconds);
	void BuildSeparationCurveCache();
	FVector EvaluateSeparationCurve(float Parameter) const;
	float FindCurveParameterForDistance(float Distance) const;
	FVector GetLocationAtDistance(float Distance) const;
	FVector GetDirectionAtDistance(float Distance) const;
	bool CheckPhysicalCollision(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	AActor* FindProximityTarget() const;
	void Explode(AActor* DamageTarget, const FVector& ImpactLocation);
	void PlayExplosionCosmetics();

	UPROPERTY(ReplicatedUsing=OnRep_LaunchData)
	FRocketLaunchData LaunchData;

	UPROPERTY(ReplicatedUsing=OnRep_Exploded)
	bool bExploded = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize100 ExplosionLocation;

	float DistanceTraveled = 0.0f;
	float TimeSinceProximityCheck = 0.0f;
	float SeparationCurveLength = 0.0f;
	static constexpr int32 SeparationCurveSamples = 16;
	float SeparationCurveCumulativeDistances[SeparationCurveSamples + 1] = {};
	bool bInitialized = false;
	bool bCountedOnServer = false;

	static int32 ServerActiveRocketCount;
};
