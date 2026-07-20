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

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UJetStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJetStatsComponent();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Plane|Stats")
	const FJetFlightStats& GetFlightStats() const { return EffectiveFlightStats; }

	UFUNCTION(BlueprintCallable, Category="Plane|Stats")
	void RecalculateStats();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FJetFlightStats BaseFlightStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Stats")
	FJetFlightStats EffectiveFlightStats;
};
