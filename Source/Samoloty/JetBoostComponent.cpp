#include "JetBoostComponent.h"

#include "JetStatsComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UJetBoostComponent::UJetBoostComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UJetBoostComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const UJetStatsComponent* Stats = GetStatsComponent())
	{
		CurrentEnergy = Stats->GetFlightStats().MaxBoostEnergy;
	}
}

void UJetBoostComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UJetStatsComponent* Stats = GetStatsComponent();
	if (!Stats)
	{
		return;
	}

	const FJetFlightStats& Values = Stats->GetFlightStats();
	if (GetOwner()->HasAuthority())
	{
		const bool bCanBoost = bLocalBoostRequested && CurrentEnergy > KINDA_SMALL_NUMBER;
		SetAuthoritativeBoostState(bCanBoost);

		if (bIsBoosting)
		{
			CurrentEnergy = FMath::Max(0.0f, CurrentEnergy - Values.BoostEnergyDrainPerSecond * DeltaTime);
			TimeSinceBoostStopped = 0.0f;
			if (CurrentEnergy <= KINDA_SMALL_NUMBER)
			{
				SetAuthoritativeBoostState(false);
			}
		}
		else
		{
			TimeSinceBoostStopped += DeltaTime;
			if (TimeSinceBoostStopped >= Values.BoostRegenDelay)
			{
				CurrentEnergy = FMath::Min(Values.MaxBoostEnergy,
					CurrentEnergy + Values.BoostEnergyRegenPerSecond * DeltaTime);
			}
		}
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bUsePredictedState = OwnerPawn && OwnerPawn->IsLocallyControlled() && !GetOwner()->HasAuthority();
	const bool bVisualBoosting = bUsePredictedState ? bLocalBoostRequested : bIsBoosting;
	BoostAlpha = FMath::FInterpTo(BoostAlpha, bVisualBoosting ? 1.0f : 0.0f,
		DeltaTime, Values.BoostResponseSpeed);
}

void UJetBoostComponent::SetBoostRequested(const bool bRequested)
{
	bLocalBoostRequested = bRequested;
	if (GetOwner()->HasAuthority())
	{
		SetAuthoritativeBoostState(bRequested && CurrentEnergy > KINDA_SMALL_NUMBER);
	}
	else
	{
		OnBoostStateChanged.Broadcast(bRequested);
		ServerSetBoostRequested(bRequested);
	}
}

void UJetBoostComponent::ServerSetBoostRequested_Implementation(const bool bRequested)
{
	bLocalBoostRequested = bRequested;
}

void UJetBoostComponent::SetAuthoritativeBoostState(const bool bNewBoosting)
{
	if (bIsBoosting != bNewBoosting)
	{
		bIsBoosting = bNewBoosting;
		OnBoostStateChanged.Broadcast(bIsBoosting);
	}
}

void UJetBoostComponent::OnRep_IsBoosting()
{
	OnBoostStateChanged.Broadcast(bIsBoosting);
}

const UJetStatsComponent* UJetBoostComponent::GetStatsComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
}

void UJetBoostComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJetBoostComponent, bIsBoosting);
	DOREPLIFETIME(UJetBoostComponent, CurrentEnergy);
}
