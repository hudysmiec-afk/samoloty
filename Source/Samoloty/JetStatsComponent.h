#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JetStatsComponent.generated.h"

USTRUCT(BlueprintType)
struct FJetFlightStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float ForwardSpeed = 3000.0f;

	/** Lowest forward speed reached while the brake key is fully held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MinForwardSpeed = 1200.0f;

	/** Responsiveness of forward-speed changes, including braking and releasing the brake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.1"))
	float ForwardSpeedResponse = 2.5f;

	/** Target-speed multiplier while flying vertically upward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight|Direction Speed", meta=(ClampMin="0.1", ClampMax="1"))
	float MaxClimbSpeedMultiplier = 0.75f;

	/** Target-speed multiplier while flying vertically downward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight|Direction Speed", meta=(ClampMin="1", ClampMax="3"))
	float MaxDiveSpeedMultiplier = 1.25f;

	/** ForwardSpeedResponse multiplier while flying vertically upward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight|Direction Speed", meta=(ClampMin="0.1", ClampMax="1"))
	float MaxClimbResponseMultiplier = 0.65f;

	/** ForwardSpeedResponse multiplier while flying vertically downward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight|Direction Speed", meta=(ClampMin="1", ClampMax="3"))
	float MaxDiveResponseMultiplier = 1.35f;

	/** Maximum lateral speed as a fraction of ForwardSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0", ClampMax="1"))
	float StrafeSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MaxYawTurnRate = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MaxPitchTurnRate = 42.0f;

	/** Turn-rate multiplier at MinForwardSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight|Turning", meta=(ClampMin="0.1", ClampMax="3"))
	float MinSpeedTurnMultiplier = 1.25f;

	/** Turn-rate multiplier at full boost speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight|Turning", meta=(ClampMin="0.1", ClampMax="1"))
	float BoostSpeedTurnMultiplier = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.1"))
	float BrakeInputResponseSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0", ClampMax="89"))
	float MaxPitch = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover", meta=(ClampMin="0", ClampMax="89"))
	float HoverPitch = 89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover", meta=(ClampMin="0.1"))
	float HoverRotationResponse = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover", meta=(ClampMin="0.1"))
	float HoverDecelerationResponse = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover", meta=(ClampMin="0.1"))
	float HoverExitAccelerationResponse = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost", meta=(ClampMin="1.0"))
	float BoostSpeedMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost", meta=(ClampMin="0.1"))
	float BoostResponseSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost", meta=(ClampMin="0"))
	float MaxBoostEnergy = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost", meta=(ClampMin="0"))
	float BoostEnergyDrainPerSecond = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost", meta=(ClampMin="0"))
	float BoostEnergyRegenPerSecond = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost", meta=(ClampMin="0"))
	float BoostRegenDelay = 1.0f;
};

USTRUCT(BlueprintType)
struct FJetCombatStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin="1"))
	float MaxHealth = 10000.0f;
};

USTRUCT(BlueprintType)
struct FRifleGunStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle", meta=(ClampMin="0"))
	float Damage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle", meta=(ClampMin="0.1"))
	float ShotsPerSecond = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle", meta=(ClampMin="0"))
	float MaxRange = 200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle", meta=(ClampMin="0", ClampMax="45"))
	float SpreadAngleDegrees = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle|Visual", meta=(ClampMin="1"))
	float TracerSpeed = 200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle|Visual", meta=(ClampMin="1"))
	float TracerLength = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rifle|Visual", meta=(ClampMin="0.1"))
	float TracerWidth = 5.0f;
};

/** Shared sensor ranges. Weapons still apply their own range and angle limits. */
USTRUCT(BlueprintType)
struct FRadarStats
{
	GENERATED_BODY()

	/** Maximum range at which a contact is displayed on the radar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radar", meta=(ClampMin="0", Units="cm"))
	float DetectionRange = 50000.0f;

	/** Reserved for optional gun targeting assistance added by equipment later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radar", meta=(ClampMin="0", Units="cm"))
	float GunTargetingRange = 80000.0f;

	/** Maximum sensor range available to guided missile weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radar", meta=(ClampMin="0", Units="cm"))
	float MissileTargetingRange = 50000.0f;
};

USTRUCT(BlueprintType)
struct FShotgunStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun", meta=(ClampMin="1"))
	int32 PelletCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun", meta=(ClampMin="0"))
	float DamagePerPellet = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun", meta=(ClampMin="0.1"))
	float ShotsPerSecond = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun", meta=(ClampMin="0"))
	float MaxRange = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun", meta=(ClampMin="0", ClampMax="45"))
	float SpreadAngleDegrees = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun|Visual", meta=(ClampMin="1"))
	float TracerSpeed = 180000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun|Visual", meta=(ClampMin="1"))
	float TracerLength = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shotgun|Visual", meta=(ClampMin="0.1"))
	float TracerWidth = 60.0f;
};

USTRUCT(BlueprintType)
struct FRocketBarrageStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barrage", meta=(ClampMin="1"))
	int32 SalvoCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barrage", meta=(ClampMin="1"))
	int32 RocketsPerSalvo = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barrage", meta=(ClampMin="0"))
	float SalvoInterval = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barrage", meta=(ClampMin="0"))
	float Cooldown = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0"))
	float RocketSpeed = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0"))
	float RocketDamage = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0"))
	float MaxTravelDistance = 200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0"))
	float ProximityRadius = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0", ClampMax="45"))
	float SpreadAngleDegrees = 1.0f;

	/** Forward length of the distance-driven launch separation curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Separation", meta=(ClampMin="0"))
	float SeparationForwardDistance = 800.0f;

	/** Maximum randomized side/up offset reached at the end of separation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Separation", meta=(ClampMin="0"))
	float SeparationRadius = 125.0f;

	/** Length of Bezier control handles relative to forward separation distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Separation", meta=(ClampMin="0.05", ClampMax="0.75"))
	float SeparationCurveStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0.016"))
	float ProximityCheckInterval = 0.1f;
};

USTRUCT(BlueprintType)
struct FHomingMissileStats
{
	GENERATED_BODY()

	/** Number of salvos fired by one complete trigger sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Salvo", meta=(ClampMin="1"))
	int32 SalvoCount = 1;

	/** Missiles spawned simultaneously in each salvo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Salvo", meta=(ClampMin="1"))
	int32 MissilesPerSalvo = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Salvo", meta=(ClampMin="0", Units="s"))
	float SalvoInterval = 0.4f;

	/** Full cooldown starts after the final salvo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Salvo", meta=(ClampMin="0", Units="s"))
	float Cooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile", meta=(ClampMin="1", Units="cm/s"))
	float MissileSpeed = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile", meta=(ClampMin="0"))
	float MissileDamage = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile", meta=(ClampMin="1", Units="cm"))
	float MaxTravelDistance = 200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile", meta=(ClampMin="0", Units="cm"))
	float ProximityRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile", meta=(ClampMin="0.016", Units="s"))
	float ProximityCheckInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting", meta=(ClampMin="1", Units="cm"))
	float LockRange = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float LockAngleDegrees = 15.0f;

	/** A manually rejected target cannot be selected again during this time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting", meta=(ClampMin="0", Units="s"))
	float TargetRejectDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guidance", meta=(ClampMin="0", Units="deg/s"))
	float MaxTurnRateDegreesPerSecond = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float SpreadAngleDegrees = 1.0f;

	/** Forward length of the shared distance-driven launch separation curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile|Separation", meta=(ClampMin="0", Units="cm"))
	float SeparationForwardDistance = 800.0f;

	/** Maximum randomized side/up offset reached at the end of separation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile|Separation", meta=(ClampMin="0", Units="cm"))
	float SeparationRadius = 125.0f;

	/** Length of Bezier control handles relative to forward separation distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Missile|Separation",
		meta=(ClampMin="0.05", ClampMax="0.75"))
	float SeparationCurveStrength = 0.35f;
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UJetStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJetStatsComponent();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FJetFlightStats& GetFlightStats() const { return EffectiveFlightStats; }

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FJetCombatStats& GetCombatStats() const { return EffectiveCombatStats; }

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FRadarStats& GetRadarStats() const { return EffectiveRadarStats; }

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FRocketBarrageStats& GetRocketBarrageStats() const { return EffectiveRocketBarrageStats; }

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FHomingMissileStats& GetHomingMissileStats() const { return EffectiveHomingMissileStats; }

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FRifleGunStats& GetRifleGunStats() const { return EffectiveRifleGunStats; }

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FShotgunStats& GetShotgunStats() const { return EffectiveShotgunStats; }

	UFUNCTION(BlueprintCallable, Category="Plane|Stats")
	void RecalculateStats();

	void SetBaseFlightStats(const FJetFlightStats& InStats) { BaseFlightStats = InStats; }
	void SetBaseCombatStats(const FJetCombatStats& InStats) { BaseCombatStats = InStats; }
	void SetBaseRadarStats(const FRadarStats& InStats) { BaseRadarStats = InStats; }
	void SetBaseRifleGunStats(const FRifleGunStats& InStats) { BaseRifleGunStats = InStats; }
	void SetBaseShotgunStats(const FShotgunStats& InStats) { BaseShotgunStats = InStats; }
	void SetBaseHomingMissileStats(const FHomingMissileStats& InStats) { BaseHomingMissileStats = InStats; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FJetFlightStats BaseFlightStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FJetFlightStats EffectiveFlightStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FJetCombatStats BaseCombatStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FJetCombatStats EffectiveCombatStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRadarStats BaseRadarStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRadarStats EffectiveRadarStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRocketBarrageStats BaseRocketBarrageStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRocketBarrageStats EffectiveRocketBarrageStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FHomingMissileStats BaseHomingMissileStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FHomingMissileStats EffectiveHomingMissileStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRifleGunStats BaseRifleGunStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRifleGunStats EffectiveRifleGunStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FShotgunStats BaseShotgunStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FShotgunStats EffectiveShotgunStats;
};
