#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JetBoostComponent.generated.h"

class UJetStatsComponent;
class UBackwardDashComponent;
class UQuickReversalComponent;
class UForwardDashComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBoostStateChangedSignature, bool, bIsBoosting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBoostEnergyChangedSignature, float, CurrentEnergy, float, MaxEnergy);

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UJetBoostComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJetBoostComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Plane|Boost")
	void SetBoostRequested(bool bRequested);

	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	bool IsBoosting() const;

	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	float GetBoostAlpha() const;

	/** One during normal operation, fading to zero while a mobility skill cuts the engine. */
	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	float GetEnginePowerAlpha() const;

	UFUNCTION(BlueprintPure, Category="Plane|Boost")
	float GetCurrentEnergy() const { return CurrentEnergy; }

	UPROPERTY(BlueprintAssignable, Category="Plane|Boost")
	FBoostStateChangedSignature OnBoostStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Plane|Boost")
	FBoostEnergyChangedSignature OnBoostEnergyChanged;

	/** Temporary local diagnostics. Disable after the boost issue is resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost|Debug")
	bool bShowBoostDebug = false;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetBoostRequested(bool bRequested);

	UFUNCTION()
	void OnRep_IsBoosting();

	void SetAuthoritativeBoostState(bool bNewBoosting);
	void UpdatePresentationBoostState();
	const UJetStatsComponent* GetStatsComponent() const;
	void DrawBoostDebug(float MaxEnergy) const;

	UPROPERTY(ReplicatedUsing=OnRep_IsBoosting)
	bool bIsBoosting = false;

	UPROPERTY(Replicated)
	float CurrentEnergy = 0.0f;

	bool bLocalBoostRequested = false;
	float BoostAlpha = 0.0f;
	float TimeSinceBoostStopped = 0.0f;
	bool bLastPresentationBoosting = false;

	UPROPERTY(Transient)
	TObjectPtr<UQuickReversalComponent> CachedQuickReversalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBackwardDashComponent> CachedBackwardDashComponent;

	UPROPERTY(Transient)
	TObjectPtr<UForwardDashComponent> CachedForwardDashComponent;
};
