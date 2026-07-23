#include "JetBoostComponent.h"

#include "ArcadeFlightComponent.h"
#include "JetStatsComponent.h"
#include "Engine/Engine.h"
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

	// The server is the only source of boost truth. Clients smooth the replicated
	// state but never keep boosting only because Space is still held locally.
	BoostAlpha = FMath::FInterpTo(BoostAlpha, bIsBoosting ? 1.0f : 0.0f,
		DeltaTime, Values.BoostResponseSpeed);
	DrawBoostDebug(Values.MaxBoostEnergy);
}

void UJetBoostComponent::DrawBoostDebug(const float MaxEnergy) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bShowBoostDebug || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}

	const FString AuthorityText = GetOwner()->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	const UJetStatsComponent* StatsComponent = GetStatsComponent();
	const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	const float SpeedCmPerSecond = Flight ? Flight->GetCurrentForwardSpeed() : 0.0f;
	const float TurnMultiplier = Flight ? Flight->GetCurrentTurnRateMultiplier() : 1.0f;
	const float MaxYawRate = StatsComponent
		? StatsComponent->GetFlightStats().MaxYawTurnRate * TurnMultiplier : 0.0f;
	const float MaxPitchRate = StatsComponent
		? StatsComponent->GetFlightStats().MaxPitchTurnRate * TurnMultiplier : 0.0f;
	const FRotator AircraftRotation = GetOwner()->GetActorRotation();
	const float MaxPitchAngle = StatsComponent ? StatsComponent->GetFlightStats().MaxPitch : 0.0f;
	const FString Message = FString::Printf(
		TEXT("BOOST [%s]\nEnergy: %.1f / %.1f\nSpeed: %.1f m/s\nRotation: Yaw %.1f deg | Pitch %.1f deg (limit +/-%.1f)\nMax turn rate: Yaw %.1f deg/s | Pitch %.1f deg/s (x%.2f)\nRequested: %s | Server state: %s | Alpha: %.2f"),
		*AuthorityText,
		CurrentEnergy,
		MaxEnergy,
		SpeedCmPerSecond / 100.0f,
		AircraftRotation.Yaw,
		AircraftRotation.Pitch,
		MaxPitchAngle,
		MaxYawRate,
		MaxPitchRate,
		TurnMultiplier,
		bLocalBoostRequested ? TEXT("YES") : TEXT("NO"),
		bIsBoosting ? TEXT("ON") : TEXT("OFF"),
		BoostAlpha);
	const FColor Color = bIsBoosting ? FColor::Cyan : (CurrentEnergy <= KINDA_SMALL_NUMBER ? FColor::Red : FColor::White);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, Color, Message);
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
