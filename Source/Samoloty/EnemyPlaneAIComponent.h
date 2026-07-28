#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyPlaneAIComponent.generated.h"

class APawn;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="100"))
	float FireRange = 80000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0"))
	float FlyPastDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="100"))
	float ExtendDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0", ClampMax="90"))
	float AttackResumeAngleDegrees = 30.0f;

	/** After this long without lining up, TurnBack switches to the faster recovery turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="0.1"))
	float FastTurnDelay = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy Plane|Combat", meta=(ClampMin="1"))
	float FastTurnRateDegrees = 180.0f;

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

private:
	void ScanForTarget();
	void UpdateObstacleAvoidance();
	void SetState(EEnemyPlaneState NewState);
	void ChooseRoamPoint();
	FVector GetDesiredDirection() const;
	FVector GetTargetLocation() const;
	float GetForwardSpeed() const;
	void UpdateWeapon();
	bool IsValidPlayerTarget(const APawn* Pawn) const;
	void UpdateServerNetworkState();
	void UpdateClientSmoothing(float DeltaTime);

	UFUNCTION()
	void OnRep_ServerState();

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CurrentTarget;

	UPROPERTY(Transient)
	TObjectPtr<URifleGunComponent> RifleGun;

	UPROPERTY(Transient)
	TObjectPtr<UJetStatsComponent> JetStats;

	EEnemyPlaneState State = EEnemyPlaneState::Roaming;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector RoamPoint = FVector::ZeroVector;
	FVector ExtendStart = FVector::ZeroVector;
	FVector AvoidanceDirection = FVector::ZeroVector;
	float TargetScanTimeRemaining = 0.0f;
	float ObstacleCheckTimeRemaining = 0.0f;
	float StateElapsedTime = 0.0f;
	bool bWeaponFiring = false;
	bool bReceivedInitialNetworkState = false;
	double LastNetworkStateReceiveTime = 0.0;

	UPROPERTY(ReplicatedUsing=OnRep_ServerState)
	FEnemyPlaneNetworkState ServerState;
};
