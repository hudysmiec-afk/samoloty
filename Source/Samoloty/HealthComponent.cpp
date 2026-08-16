#include "HealthComponent.h"

#include "ArcadeFlightGameMode.h"
#include "EvasiveRollComponent.h"
#include "JetStatsComponent.h"
#include "RocketProjectile.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
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
	if (Cast<ARocketProjectile>(DamageCauser))
	{
		const UEvasiveRollComponent* Roll =
			GetOwner()->FindComponentByClass<UEvasiveRollComponent>();
		if (Roll && Roll->IsEvadingMissiles())
		{
			return;
		}
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	OnHealthChanged.Broadcast(CurrentHealth, GetMaxHealth());
	AActor* AttackerActor = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
	if (!AttackerActor && DamageCauser)
	{
		AttackerActor = DamageCauser->GetInstigator();
		if (!AttackerActor)
		{
			AttackerActor = DamageCauser->GetOwner();
		}
	}
	if (AttackerActor && AttackerActor != GetOwner())
	{
		ClientNotifyDamageReceived(AttackerActor);
		APawn* AttackerPawn = Cast<APawn>(AttackerActor);
		if (AttackerPawn && AttackerPawn->IsPlayerControlled())
		{
			if (UHealthComponent* AttackerHealth =
				AttackerPawn->FindComponentByClass<UHealthComponent>())
			{
				FVector DamageCenter = GetOwner()->GetActorLocation();
				FVector DamageExtent = FVector::ZeroVector;
				GetOwner()->GetActorBounds(true, DamageCenter, DamageExtent);
				AttackerHealth->ClientNotifyDamageDealt(
					GetOwner(), AppliedDamage, DamageCenter);
			}
		}
	}
	if (CurrentHealth <= KINDA_SMALL_NUMBER)
	{
		bIsDead = true;
		OnHealthDepleted.Broadcast();
		if (APawn* DeadPawn = Cast<APawn>(GetOwner()); DeadPawn && DeadPawn->IsPlayerControlled())
		{
			if (AArcadeFlightGameMode* FlightGameMode =
				Cast<AArcadeFlightGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				FlightGameMode->SchedulePlayerRespawn(DeadPawn->GetController());
			}
		}
		GetOwner()->Destroy();
	}
}

void UHealthComponent::ClientNotifyDamageReceived_Implementation(AActor* AttackerActor)
{
	if (IsValid(AttackerActor))
	{
		OnDamageReceived.Broadcast(AttackerActor);
	}
}

void UHealthComponent::ClientNotifyDamageDealt_Implementation(
	AActor* DamagedActor, const float Damage, const FVector_NetQuantize WorldLocation)
{
	if (Damage > 0.0f)
	{
		OnDamageDealt.Broadcast(DamagedActor, Damage, WorldLocation);
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
