#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GroundEnemyCharacter.generated.h"

class UGroundEnemyAIComponent;
class UBoxComponent;
class UHealthComponent;
class UHitRewindComponent;
class UHomingMissileWeaponComponent;
class UJetStatsComponent;
class URifleGunComponent;
class USceneComponent;
class UWeaponSystemComponent;
class UTargetIntentComponent;
class UWorldNameComponent;

/** Blueprint-ready enemy that walks on navigation and reuses the common combat components. */
UCLASS(Blueprintable)
class SAMOLOTY_API AGroundEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGroundEnemyCharacter();
	virtual void BeginPlay() override;

protected:
	/** Navigation capsule is movement-only; these query shapes receive weapon hits. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Collision")
	TObjectPtr<UBoxComponent> BodyHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Collision")
	TObjectPtr<UBoxComponent> UpperBodyHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UJetStatsComponent> JetStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UHitRewindComponent> HitRewind;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<URifleGunComponent> RifleGun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UHomingMissileWeaponComponent> HomingMissiles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UWeaponSystemComponent> WeaponSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UGroundEnemyAIComponent> EnemyAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UTargetIntentComponent> TargetIntent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Components")
	TObjectPtr<UWorldNameComponent> WorldName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Fire Points")
	TObjectPtr<USceneComponent> GunMuzzleLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Fire Points")
	TObjectPtr<USceneComponent> GunMuzzleRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Fire Points")
	TObjectPtr<USceneComponent> MissileMuzzleLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ground Enemy|Fire Points")
	TObjectPtr<USceneComponent> MissileMuzzleRight;
};
