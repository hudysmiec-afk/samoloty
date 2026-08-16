#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyAITypes.h"
#include "GroundEnemyAIComponent.generated.h"

class AAIController;
class APawn;
class UDamageType;
class UHealthComponent;
class UHomingMissileWeaponComponent;
class UJetStatsComponent;
class URifleGunComponent;

UENUM(BlueprintType)
enum class EGroundEnemyState : uint8
{
	Roaming,
	Chasing,
	Attacking,
	Returning
};

/** Lightweight, server-authoritative combat AI for enemies constrained to a NavMesh. */
UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UGroundEnemyAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGroundEnemyAIComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Ground Enemy|AI")
	EGroundEnemyState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Ground Enemy|AI")
	APawn* GetCurrentTarget() const { return CurrentTarget.Get(); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Perception", meta=(ClampMin="100"))
	float DetectionRange = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Perception", meta=(ClampMin="100"))
	float LoseTargetRange = 75000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Perception", meta=(ClampMin="0.1"))
	float TargetScanInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Perception")
	EEnemyAggressionMode AggressionMode = EEnemyAggressionMode::Aggressive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Perception", meta=(ClampMin="0.01", ClampMax="1"))
	float LowHealthAggressionThreshold = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Roaming", meta=(ClampMin="100"))
	float RoamRadius = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Roaming", meta=(ClampMin="10"))
	float RoamAcceptanceRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Movement", meta=(ClampMin="0.1"))
	float RepathInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Combat")
	bool bRequireLineOfSightForRifle = true;

	/** Allows this enemy archetype to use its RifleGun component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Combat")
	bool bUseRifle = true;

	/** Allows this enemy archetype to use its HomingMissile weapon component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Combat")
	bool bUseHomingMissiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Enemy|Debug")
	bool bDrawAIDebug = false;

private:
	void CacheDependencies();
	void ScanForTarget();
	void SetCurrentTarget(APawn* NewTarget);
	void ClearCurrentTarget();
	void UpdateRoaming(float DeltaTime);
	void UpdateCombat();
	void UpdateReturning();
	void ChooseRoamPoint();
	void StopWeapons();
	bool CanSearchForUnprovokedTargets() const;
	bool IsValidPlayerTarget(const APawn* Pawn) const;
	bool HasLineOfSightTo(const AActor* TargetActor) const;
	void DrawDebugState() const;

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	TWeakObjectPtr<APawn> CurrentTarget;
	TObjectPtr<AAIController> AIController;
	TObjectPtr<UHealthComponent> Health;
	TObjectPtr<UJetStatsComponent> JetStats;
	TObjectPtr<URifleGunComponent> RifleGun;
	TObjectPtr<UHomingMissileWeaponComponent> HomingMissiles;
	EGroundEnemyState State = EGroundEnemyState::Roaming;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector RoamPoint = FVector::ZeroVector;
	float TargetScanTimeRemaining = 0.0f;
	float RepathTimeRemaining = 0.0f;
	float RoamIdleTime = 0.0f;
};
