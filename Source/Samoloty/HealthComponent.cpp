#include "HealthComponent.h"

#include "JetStatsComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		CurrentHealth = GetMaxHealth();
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleAnyDamage);
	}
}

float UHealthComponent::GetMaxHealth() const
{
	const UJetStatsComponent* Stats = GetStatsComponent();
	return Stats ? Stats->GetCombatStats().MaxHealth : 0.0f;
}

void UHealthComponent::HandleAnyDamage(AActor* DamagedActor, const float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (!GetOwner()->HasAuthority() || bIsDead || Damage <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
	if (CurrentHealth <= KINDA_SMALL_NUMBER)
	{
		bIsDead = true;
		OnHealthDepleted.Broadcast();
		GetOwner()->Destroy();
	}
}

void UHealthComponent::OnRep_CurrentHealth()
{
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
}

void UHealthComponent::OnRep_IsDead()
{
	if (bIsDead)
	{
		OnHealthDepleted.Broadcast();
	}
}

const UJetStatsComponent* UHealthComponent::GetStatsComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, CurrentHealth);
	DOREPLIFETIME(UHealthComponent, bIsDead);
}
