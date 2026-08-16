#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AircraftCollisionComponent.generated.h"

class UCameraShakeBase;
class UCapsuleComponent;
class UHealthComponent;
class UNiagaraSystem;
class UShapeComponent;
class USoundBase;

/**
 * Shared aircraft collision logic. A rounded capsule is swept against the world for
 * movement, while tagged query-only shapes attached to the visual model receive hits.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UAircraftCollisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAircraftCollisionComponent();

	void SetMovementCapsule(UCapsuleComponent* InMovementCapsule);
	static void ConfigureDamageHitbox(UShapeComponent* Hitbox);

	/** Moves the owning actor and returns velocity after collision response. */
	FVector MoveOwner(const FVector& RequestedMove, const FVector& RequestedVelocity,
		bool bEnableImpactResponse = true);

	static const FName DamageHitboxTag;
	static const FName MovementBodyTag;

protected:
	/** Portion of head-on velocity reflected away from a surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Movement",
		meta=(ClampMin="0", ClampMax="1"))
	float Restitution = 0.40f;

	/** Velocity retained while sliding along a surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Movement",
		meta=(ClampMin="0.05", ClampMax="1"))
	float SurfaceSpeedRetention = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Movement",
		meta=(ClampMin="0"))
	float SeparationDistance = 20.0f;

	/** Gentler response used when two aircraft movement capsules meet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Aircraft",
		meta=(ClampMin="0", ClampMax="1"))
	float AircraftRestitution = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Aircraft",
		meta=(ClampMin="0.05", ClampMax="1"))
	float AircraftSurfaceSpeedRetention = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Aircraft",
		meta=(ClampMin="0"))
	float AircraftSeparationDistance = 30.0f;

	/** Damageable actors do not steer the aircraft or behave like solid walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Movement")
	bool bDamageableActorsUseSoftCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="0", Units="cm/s"))
	float MinDamageSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="1", Units="cm/s"))
	float MaxDamageSpeed = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="0", ClampMax="1"))
	float MinSelfDamageFraction = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="0", ClampMax="1"))
	float MaxSelfDamageFraction = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="0", ClampMax="1"))
	float DamageableActorSelfDamageMultiplier = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="0", ClampMax="2"))
	float OtherActorDamageMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Damage",
		meta=(ClampMin="0.05", Units="s"))
	float DamageCooldown = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Effects",
		meta=(ClampMin="0", Units="cm/s"))
	float MinEffectSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Collision|Effects",
		meta=(ClampMin="0.01", Units="s"))
	float EffectCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Collision|Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Collision|Effects")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Collision|Effects")
	TSubclassOf<UCameraShakeBase> ImpactCameraShake;

private:
	bool SweepMovementCapsule(const FVector& Start, const FVector& End,
		FHitResult& OutHit) const;
	void HandleImpact(const FHitResult& Hit, float ImpactSpeed,
		bool bDamageableActorCollision);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayImpactEffects(FVector_NetQuantize ImpactPoint,
		FVector_NetQuantizeNormal ImpactNormal, float Intensity);

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> MovementCapsule;

	double NextDamageTime = 0.0;
	double NextEffectTime = 0.0;
};
