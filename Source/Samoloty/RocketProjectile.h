#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RocketProjectile.generated.h"

class UHealthComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
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

	/** Assigned target is fixed for the projectile lifetime; missiles never auto-retarget. */
	UPROPERTY()
	TObjectPtr<AActor> HomingTarget;

	UPROPERTY()
	float MaxTurnRateDegreesPerSecond = 0.0f;

	UPROPERTY()
	bool bDrawDebug = false;
};

/** Sparse authoritative correction used only by moving-target homing projectiles. */
USTRUCT()
struct FHomingMissileNetState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Location;

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	UPROPERTY()
	float DistanceTraveled = 0.0f;

	UPROPERTY()
	double ServerTime = 0.0;
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

	/** Local 3D sound played wherever the replicated explosion occurs. Accepts Sound Wave or Sound Cue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Effects")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Collision", meta=(ClampMin="1"))
	float PhysicalCollisionRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Effects", meta=(ClampMin="0.05"))
	float ExplosionReplicationDelay = 0.25f;

	/** Keeps the Niagara explosion visible even when the gameplay detonation radius is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rocket|Effects", meta=(ClampMin="0", Units="cm"))
	float MinimumExplosionVisualRadius = 150.0f;

private:
	UFUNCTION()
	void OnRep_LaunchData();

	UFUNCTION()
	void OnRep_Exploded();

	UFUNCTION()
	void OnRep_HomingNetState();

	UFUNCTION()
	void OnRep_HomingDisabled();

	void ApplyLaunchState();
	void SimulateRocketMovement(float DeltaSeconds);
	void SimulateHomingMovement(float DeltaSeconds);
	void UpdateHomingNetState();
	void BuildSeparationCurveCache();
	FVector EvaluateSeparationCurve(float Parameter) const;
	float FindCurveParameterForDistance(float Distance) const;
	FVector GetLocationAtDistance(float Distance) const;
	FVector GetDirectionAtDistance(float Distance) const;
	bool CheckPhysicalCollision(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	AActor* FindProximityTarget(const FVector& QueryLocation) const;
	AActor* GetLiveHomingTarget() const;
	void RefreshMissileWarningRegistration();
	void ClearMissileWarningRegistration();
	void BreakHomingAfterEvade(AActor* EvadingActor);
	FVector RotateDirectionTowards(const FVector& CurrentDirection,
		const FVector& DesiredDirection, float DeltaSeconds) const;
	void Explode(AActor* DamageTarget, const FVector& ImpactLocation);
	void PlayExplosionCosmetics();

	UPROPERTY(ReplicatedUsing=OnRep_LaunchData)
	FRocketLaunchData LaunchData;

	UPROPERTY(ReplicatedUsing=OnRep_Exploded)
	bool bExploded = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize100 ExplosionLocation;

	UPROPERTY(ReplicatedUsing=OnRep_HomingNetState)
	FHomingMissileNetState HomingNetState;

	/** Set only when this missile actually reaches an evading target during its roll. */
	UPROPERTY(ReplicatedUsing=OnRep_HomingDisabled)
	bool bHomingDisabled = false;

	TWeakObjectPtr<AActor> RegisteredWarningTarget;

	float DistanceTraveled = 0.0f;
	float TimeSinceProximityCheck = 0.0f;
	float HomingNetUpdateAccumulator = 0.0f;
	float SeparationCurveLength = 0.0f;
	static constexpr int32 SeparationCurveSamples = 16;
	float SeparationCurveCumulativeDistances[SeparationCurveSamples + 1] = {};
	bool bInitialized = false;
	bool bCountedOnServer = false;
	bool bHasHomingCorrection = false;
	FVector CurrentHomingDirection = FVector::ForwardVector;
	FVector HomingCorrectionLocation = FVector::ZeroVector;
	FVector HomingCorrectionDirection = FVector::ForwardVector;

	static int32 ServerActiveRocketCount;
};
