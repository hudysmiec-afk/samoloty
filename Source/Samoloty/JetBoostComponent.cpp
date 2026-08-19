#include "JetBoostComponent.h"

#include "ArcadeFlightComponent.h"
#include "BackwardDashComponent.h"
#include "ForwardDashComponent.h"
#include "JetStatsComponent.h"
#include "QuickReversalComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UJetBoostComponent::UJetBoostComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	// Gameplay state lives in the rewindable flight simulation. This component
	// is now only an input/presentation facade and does not need its own channel.
	SetIsReplicatedByDefault(false);
}

void UJetBoostComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const UJetStatsComponent* Stats = GetStatsComponent())
	{
		CurrentEnergy = Stats->GetFlightStats().MaxBoostEnergy;
	}
	CachedQuickReversalComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UQuickReversalComponent>() : nullptr;
	CachedBackwardDashComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UBackwardDashComponent>() : nullptr;
	CachedForwardDashComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UForwardDashComponent>() : nullptr;
	CachedFlightComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UArcadeFlightComponent>() : nullptr;
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
	if (CachedFlightComponent)
	{
		bIsBoosting = CachedFlightComponent->IsPredictedBoosting();
		CurrentEnergy = CachedFlightComponent->GetPredictedBoostEnergy();
		BoostAlpha = CachedFlightComponent->GetPredictedBoostAlpha();
	}
	UpdatePresentationBoostState();
	DrawBoostDebug(Values.MaxBoostEnergy);
}

bool UJetBoostComponent::IsBoosting() const
{
	if (CachedForwardDashComponent && CachedForwardDashComponent->IsDashActive())
	{
		return true;
	}
	if (CachedBackwardDashComponent && CachedBackwardDashComponent->IsDashActive())
	{
		return false;
	}
	if (CachedQuickReversalComponent
		&& CachedQuickReversalComponent->IsReversalActive())
	{
		return CachedQuickReversalComponent->GetSkillBoostAlpha() > 0.01f;
	}
	return bIsBoosting;
}

float UJetBoostComponent::GetBoostAlpha() const
{
	if (CachedForwardDashComponent && CachedForwardDashComponent->IsDashActive())
	{
		return 1.0f;
	}
	if (CachedBackwardDashComponent && CachedBackwardDashComponent->IsDashActive())
	{
		return 0.0f;
	}
	if (CachedQuickReversalComponent
		&& CachedQuickReversalComponent->IsReversalActive())
	{
		return CachedQuickReversalComponent->GetSkillBoostAlpha();
	}
	return BoostAlpha;
}

float UJetBoostComponent::GetEnginePowerAlpha() const
{
	if (CachedBackwardDashComponent && CachedBackwardDashComponent->IsDashActive())
	{
		return 0.0f;
	}
	return CachedQuickReversalComponent
		? 1.0f - CachedQuickReversalComponent->GetEngineCutAlpha() : 1.0f;
}

void UJetBoostComponent::UpdatePresentationBoostState()
{
	const bool bPresentationBoosting = IsBoosting();
	if (bLastPresentationBoosting != bPresentationBoosting)
	{
		bLastPresentationBoosting = bPresentationBoosting;
		OnBoostStateChanged.Broadcast(bPresentationBoosting);
	}
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
	const EArcadeHoverState HoverState = Flight ? Flight->GetHoverState() : EArcadeHoverState::Flying;
	const TCHAR* HoverStateText = HoverState == EArcadeHoverState::Flying ? TEXT("FLYING")
		: HoverState == EArcadeHoverState::Entering ? TEXT("ENTERING")
		: HoverState == EArcadeHoverState::Hovering ? TEXT("HOVERING") : TEXT("LEAVING");
	const FString Message = FString::Printf(
		TEXT("BOOST [%s]\nEnergy: %.1f / %.1f\nSpeed: %.1f m/s\nRotation: Yaw %.1f deg | Pitch %.1f deg (limit +/-%.1f)\nMax turn rate: Yaw %.1f deg/s | Pitch %.1f deg/s (x%.2f)\nHover: %s | Alpha: %.2f\nRequested: %s | Server state: %s | Alpha: %.2f"),
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
		HoverStateText,
		Flight ? Flight->GetHoverAlpha() : 0.0f,
		bLocalBoostRequested ? TEXT("YES") : TEXT("NO"),
		bIsBoosting ? TEXT("ON") : TEXT("OFF"),
		BoostAlpha);
	const FColor Color = bIsBoosting ? FColor::Cyan : (CurrentEnergy <= KINDA_SMALL_NUMBER ? FColor::Red : FColor::White);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, Color, Message);
}

void UJetBoostComponent::SetBoostRequested(const bool bRequested)
{
	bLocalBoostRequested = bRequested;
	if (!CachedFlightComponent && GetOwner())
	{
		CachedFlightComponent = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	}
	if (CachedFlightComponent)
	{
		CachedFlightComponent->SetLocalBoostRequested(bRequested);
	}
}

const UJetStatsComponent* UJetBoostComponent::GetStatsComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
}
