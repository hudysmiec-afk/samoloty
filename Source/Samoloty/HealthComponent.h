#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UDamageType;
class UJetStatsComponent;
class UNiagaraSystem;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHealthChangedSignature, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHealthDepletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamageReceivedSignature, AActor*, AttackerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDamageDealtSignature,
	AActor*, DamagedActor, float, Damage, FVector, WorldLocation);

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Plane|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Plane|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category="Plane|Health")
	bool IsDead() const { return bIsDead; }

	UPROPERTY(BlueprintAssignable, Category="Plane|Health")
	FHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Plane|Health")
	FHealthDepletedSignature OnHealthDepleted;

	/** Delivered to the owning client so local HUD can show the attacker direction. */
	UPROPERTY(BlueprintAssignable, Category="Plane|Health")
	FDamageReceivedSignature OnDamageReceived;

	/** Server-confirmed outgoing damage delivered only to the attacker's owning client. */
	UPROPERTY(BlueprintAssignable, Category="Plane|Health")
	FDamageDealtSignature OnDamageDealt;

	/** One-shot cosmetic spawned for every client immediately before this actor is destroyed. */
	UPROPERTY(EditAnywhere, Category="Plane|Health|Death")
	TObjectPtr<UNiagaraSystem> DeathEffect;

	UPROPERTY(EditAnywhere, Category="Plane|Health|Death", meta=(ClampMin="0.01"))
	float DeathEffectScale = 1.0f;

	/** One-shot spatial sound played for every client at the destruction location. */
	UPROPERTY(EditAnywhere, Category="Plane|Health|Death")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditAnywhere, Category="Plane|Health|Death", meta=(ClampMin="0.0"))
	float DeathSoundVolume = 1.0f;

private:
	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION(Client, Unreliable)
	void ClientNotifyDamageReceived(AActor* AttackerActor);

	UFUNCTION(Client, Reliable)
	void ClientNotifyDamageDealt(
		AActor* DamagedActor, float Damage, FVector_NetQuantize WorldLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDeathEffect(FVector_NetQuantize Location, FRotator Rotation);

	const UJetStatsComponent* GetStatsComponent() const;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentHealth)
	float CurrentHealth = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_IsDead)
	bool bIsDead = false;
};
