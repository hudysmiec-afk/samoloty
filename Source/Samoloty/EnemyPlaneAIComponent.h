#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyPlaneAIComponent.generated.h"

class APawn;
class AController;
class UDamageType;
class UJetStatsComponent;
class URifleGunComponent;

USTRUCT()
struct FEnemyPlaneNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Location;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize100 Velocity;
};

UENUM(BlueprintType)
enum class EEnemyPlaneState : uint8
{
	Roaming,
	AttackRun,
	Extend,
	TurnBack
};

UENUM(BlueprintType)
enum class EEnemyAggressionMode : uint8
{
	Aggressive UMETA(DisplayName="Aggressive"),
	Defensive UMETA(DisplayName="Defensive - retaliates when attacked"),
	DefensiveUntilLowHealth UMETA(DisplayName="Defensive until low health"),
	Passive UMETA(DisplayName="Passive")
};

UENUM(BlueprintType)
enum class EEnemyTargetPriority : uint8
{
	Closest UMETA(DisplayName="Closest"),
	FirstAttacker UMETA(DisplayName="First acquired / attacker"),
	LowestHealth UMETA(DisplayName="Lowest health"),
	HighestHealth UMETA(DisplayName="Highest health"),
	Random UMETA(DisplayName="Random"),
	HighestAggro UMETA(DisplayName="Highest damage aggro")
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UEnemyPlaneAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyPlaneAIComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Enemy Plane|AI")
	EEnemyPlaneState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Enemy Plane|AI")
	APawn* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Enemy Plane|AI")
	EEnemyAggressionMode GetAggressionMode() const { return AggressionMode; }

	UFUNCTION(BlueprintPure, Category="Enemy Plane|AI")
	EEnemyTargetPriority GetTargetPriority() const { return TargetPriority; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Roaming", meta=(ClampMin="100"))
	float RoamRadius = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Roaming", meta=(ClampMin="100"))
	float RoamAcceptanceRadius = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting", meta=(ClampMin="100"))
	float DetectionRange = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting", meta=(ClampMin="100"))
	float LoseTargetRange = 250000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting", meta=(ClampMin="0.05"))
	float TargetScanInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting")
	EEnemyAggressionMode AggressionMode = EEnemyAggressionMode::Aggressive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting")
	EEnemyTargetPriority TargetPriority = EEnemyTargetPriority::Closest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting")
	bool bCanChangeTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting", meta=(ClampMin="1"))
	float TargetReevaluationInterval = 15.0f;

	/** Outside LoseTargetRange, retain a recently engaged target for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting", meta=(ClampMin="0"))
	float LoseTargetAfterNoAttackTime = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Targeting", meta=(ClampMin="0.01", ClampMax="1"))
	float LowHealthAggressionThreshold = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="100"))
	float FireRange = 80000.0f;

	/** Fixed guns fire only while the target is inside this half-angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0.5", ClampMax="45"))
	float FireAimHalfAngleDegrees = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0"))
	float FlyPastDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="100"))
	float ExtendDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0", ClampMax="90"))
	float AttackResumeAngleDegrees = 30.0f;

	/** A short normal-rate start keeps the beginning of a quick turn from looking like a snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0.1"))
	float FastTurnDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="1"))
	float FastTurnRateDegrees = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Flight", meta=(ClampMin="1"))
	float TurnRateDegrees = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Flight", meta=(ClampMin="0", ClampMax="89"))
	float MaxPitch = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Avoidance", meta=(ClampMin="0.05"))
	float ObstacleCheckInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Avoidance", meta=(ClampMin="100"))
	float ObstacleCheckDistance = 30000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Avoidance", meta=(ClampMin="0"))
	float AvoidanceUpWeight = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Network", meta=(ClampMin="0.1"))
	float ClientLocationSmoothingSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Network", meta=(ClampMin="0.1"))
	float ClientRotationSmoothingSpeed = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Network", meta=(ClampMin="0"))
	float MaxClientExtrapolationTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Debug")
	bool bDrawAIDebug = false;

private:
	void ScanForTarget();
	void ReevaluateTarget();
	void SetCurrentTarget(APawn* NewTarget);
	void ClearCurrentTarget();
	void CollectTargetCandidates(TArray<APawn*>& OutCandidates) const;
	APawn* SelectPreferredTarget(const TArray<APawn*>& Candidates) const;
	void CleanupAggroData();
	bool CanSearchForUnprovokedTargets() const;
	float GetHealthFraction(const APawn* Pawn) const;
	float GetCurrentHealthValue(const APawn* Pawn) const;
	float GetAggroForTarget(APawn* Pawn) const;
	void UpdateObstacleAvoidance();
	void UpdateStateTransitions();
	void UpdateDesiredFlightDirection();
	void TurnTowardDesiredDirection(float DeltaTime);
	void SetState(EEnemyPlaneState NewState);
	void ChooseRoamPoint();
	FVector GetDesiredDirection() const;
	FVector GetTargetLocation() const;
	float GetForwardSpeed() const;
	float GetActiveTurnRate() const;
	void UpdateWeapon();
	void DrawAIDebug() const;
	bool IsValidPlayerTarget(const APawn* Pawn) const;
	void UpdateServerNetworkState();
	void UpdateClientSmoothing(float DeltaTime);

	UFUNCTION()
	void OnRep_ServerState();

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CurrentTarget;
	TArray<TWeakObjectPtr<APawn>> AttackerOrder;
	TMap<TWeakObjectPtr<APawn>, float> AggroByTarget;

	UPROPERTY(Transient)
	TObjectPtr<URifleGunComponent> RifleGun;

	UPROPERTY(Transient)
	TObjectPtr<UJetStatsComponent> JetStats;

	EEnemyPlaneState State = EEnemyPlaneState::Roaming;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector RoamPoint = FVector::ZeroVector;
	FVector ExtendStart = FVector::ZeroVector;
	FVector ManeuverDirection = FVector::ForwardVector;
	FVector DesiredFlightDirection = FVector::ForwardVector;
	FVector AvoidanceDirection = FVector::ZeroVector;
	float TargetScanTimeRemaining = 0.0f;
	float TargetReevaluationTimeRemaining = 0.0f;
	float ObstacleCheckTimeRemaining = 0.0f;
	float SteeringUpdateTimeRemaining = 0.0f;
	float StateElapsedTime = 0.0f;
	float TimeWithoutAttackOpportunity = 0.0f;
	float QuickTurnSide = 1.0f;
	float QuickTurnPitchBias = 0.0f;
	bool bWeaponFiring = false;
	bool bReceivedInitialNetworkState = false;
	double LastNetworkStateReceiveTime = 0.0;

	UPROPERTY(ReplicatedUsing=OnRep_ServerState)
	FEnemyPlaneNetworkState ServerState;
};
