#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EvasiveRollComponent.generated.h"

class UArcadeFlightComponent;
class UHealthComponent;
class UPlaneAbilityQueueComponent;

UENUM(BlueprintType)
enum class EEvasiveRollDirection : uint8
{
	Left,
	Right
};

USTRUCT()
struct FEvasiveRollNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	EEvasiveRollDirection Direction = EEvasiveRollDirection::Left;

	UPROPERTY()
	double ServerStartTime = 0.0;

	UPROPERTY()
	uint16 Sequence = 0;
};

/**
 * Detects double-tap strafe input and presents a networked evasive barrel roll.
 * The owning client predicts only the animation; missile protection is always
 * decided by the authoritative server.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UEvasiveRollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEvasiveRollComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called once for each new A/D press; holding the key does not count as another tap. */
	void RegisterStrafeTap(EEvasiveRollDirection Direction);
	void PredictAbilityQueueRoll(EEvasiveRollDirection Direction);
	bool TryActivateFromAbilityQueue(EEvasiveRollDirection Direction);

	UFUNCTION(BlueprintPure, Category="Plane|Evasive Roll")
	bool IsRolling() const;

	/** Exact roll timeline used by the server ability queue. */
	bool IsRollManeuverInProgress() const;

	/** Authoritative gameplay check used by missile projectiles and damage handling. */
	UFUNCTION(BlueprintPure, Category="Plane|Evasive Roll")
	bool IsEvadingMissiles() const;

	UFUNCTION(BlueprintPure, Category="Plane|Evasive Roll")
	float GetCooldownRemaining() const;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestRoll(EEvasiveRollDirection Direction);

	UFUNCTION(Client, Reliable)
	void ClientRejectRoll();

	UFUNCTION()
	void OnRep_RollState();

	bool CanStartRoll() const;
	void StartAuthoritativeRoll(EEvasiveRollDirection Direction);
	void StartLocalPrediction(EEvasiveRollDirection Direction);
	void UpdateVisualRoll();
	double GetSynchronizedTime() const;
	double GetLocalTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_RollState)
	FEvasiveRollNetworkState RollState;

	UPROPERTY(Transient)
	TObjectPtr<UArcadeFlightComponent> CachedFlightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> CachedHealthComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPlaneAbilityQueueComponent> CachedAbilityQueueComponent;

	double LastLeftTapTime = -1.0;
	double LastRightTapTime = -1.0;
	double NextAllowedServerTime = 0.0;
	double LocalNextAllowedTime = 0.0;
	double LocalPredictionStartTime = 0.0;
	EEvasiveRollDirection LocalPredictionDirection = EEvasiveRollDirection::Left;
	bool bLocalPredictionActive = false;
	bool bLocalPresentationFinished = false;
};
