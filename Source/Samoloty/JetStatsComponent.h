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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float StrafeSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.1"))
	float StrafeResponseSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MaxYawTurnRate = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MaxPitchTurnRate = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.1"))
	float InputSmoothingSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.1"))
	float RotationResponsiveness = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0", ClampMax="89"))
	float MaxPitch = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MaxVisualBank = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0", ClampMax="85"))
	float MaxStrafeBank = 30.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0.016"))
	float ProximityCheckInterval = 0.1f;
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
	const FRocketBarrageStats& GetRocketBarrageStats() const { return EffectiveRocketBarrageStats; }

	UFUNCTION(BlueprintCallable, Category="Plane|Stats")
	void RecalculateStats();

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
	FRocketBarrageStats BaseRocketBarrageStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FRocketBarrageStats EffectiveRocketBarrageStats;
};
