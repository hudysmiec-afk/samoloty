#include "EvasiveRollComponent.h"

#include "ArcadeFlightComponent.h"
#include "ArcadeJetPawn.h"
#include "HealthComponent.h"
#include "PlaneAbilityQueueComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace EvasiveRollTuning
{
	constexpr double DoubleTapWindow = 0.24;
	constexpr double RollDuration = 1.0;
	constexpr double RollCooldown = 3.0;
}

UEvasiveRollComponent::UEvasiveRollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

void UEvasiveRollComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
		CachedHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
		CachedAbilityQueueComponent = Owner->FindComponentByClass<UPlaneAbilityQueueComponent>();
	}
}

void UEvasiveRollComponent::RegisterStrafeTap(const EEvasiveRollDirection Direction)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	const double Now = GetLocalTime();
	double& PreviousTapTime = Direction == EEvasiveRollDirection::Left
		? LastLeftTapTime : LastRightTapTime;
	const bool bIsDoubleTap = PreviousTapTime >= 0.0
		&& Now - PreviousTapTime <= EvasiveRollTuning::DoubleTapWindow;
	PreviousTapTime = bIsDoubleTap ? -1.0 : Now;
	if (!bIsDoubleTap || Now < LocalNextAllowedTime)
	{
		return;
	}
	if (!CachedAbilityQueueComponent)
	{
		return;
	}
	CachedAbilityQueueComponent->RequestAbility(
		Direction == EEvasiveRollDirection::Left
			? EPlaneAbilityType::EvasiveRollLeft
			: EPlaneAbilityType::EvasiveRollRight);
}

void UEvasiveRollComponent::PredictAbilityQueueRoll(
	const EEvasiveRollDirection Direction)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || IsRolling()
		|| GetLocalTime() < LocalNextAllowedTime
		|| !CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead()))
	{
		return;
	}
	StartLocalPrediction(Direction);
	LocalNextAllowedTime = GetLocalTime() + EvasiveRollTuning::RollCooldown;
}

bool UEvasiveRollComponent::TryActivateFromAbilityQueue(
	const EEvasiveRollDirection Direction)
{
	if (!CanStartRoll())
	{
		return false;
	}
	StartAuthoritativeRoll(Direction);
	return true;
}

void UEvasiveRollComponent::ServerRequestRoll_Implementation(const EEvasiveRollDirection Direction)
{
	if (!CanStartRoll())
	{
		ClientRejectRoll();
		return;
	}
	StartAuthoritativeRoll(Direction);
}

void UEvasiveRollComponent::ClientRejectRoll_Implementation()
{
	bLocalPredictionActive = false;
	bLocalPresentationFinished = false;
	LocalNextAllowedTime = GetLocalTime();
	UpdateVisualRoll();
}

bool UEvasiveRollComponent::CanStartRoll() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || RollState.bActive
		|| GetSynchronizedTime() < NextAllowedServerTime)
	{
		return false;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled())
	{
		return false;
	}
	return !CachedHealthComponent || !CachedHealthComponent->IsDead();
}

void UEvasiveRollComponent::StartAuthoritativeRoll(const EEvasiveRollDirection Direction)
{
	const double Now = GetSynchronizedTime();
	RollState.bActive = true;
	RollState.Direction = Direction;
	RollState.ServerStartTime = Now;
	++RollState.Sequence;
	NextAllowedServerTime = Now + EvasiveRollTuning::RollCooldown;
	GetOwner()->ForceNetUpdate();
}

void UEvasiveRollComponent::StartLocalPrediction(const EEvasiveRollDirection Direction)
{
	bLocalPredictionActive = true;
	bLocalPresentationFinished = false;
	LocalPredictionDirection = Direction;
	LocalPredictionStartTime = GetLocalTime();
}

void UEvasiveRollComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateVisualRoll();
}

void UEvasiveRollComponent::UpdateVisualRoll()
{
	AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner());
	if (!Plane)
	{
		return;
	}

	const bool bHasVisualRoll = CachedFlightComponent
		&& CachedFlightComponent->IsEvasiveRollActive();
	const float Progress = bHasVisualRoll
		? CachedFlightComponent->GetEvasiveRollProgress() : 0.0f;
	// Smooth acceleration at the beginning and deceleration at the end remove the
	// one-frame lateral kick caused by instantly starting a full-speed orbit.
	const float VisualProgress = Progress * Progress * (3.0f - 2.0f * Progress);
	const float DirectionSign = bHasVisualRoll
		? CachedFlightComponent->GetEvasiveRollDirection() : 1.0f;
	const float RollDegrees = bHasVisualRoll ? DirectionSign * 360.0f * VisualProgress : 0.0f;
	Plane->SetEvasiveRollPresentation(bHasVisualRoll, RollDegrees);
}

void UEvasiveRollComponent::OnRep_RollState()
{
	if (RollState.bActive)
	{
		// Keep a matching owner prediction on its uninterrupted local timeline.
		// Replacing it with the later server start time would visibly rewind the
		// first part of the maneuver by roughly half the round-trip latency.
		if (bLocalPredictionActive && LocalPredictionDirection != RollState.Direction)
		{
			bLocalPredictionActive = false;
			bLocalPresentationFinished = false;
		}
		LocalNextAllowedTime = FMath::Max(
			LocalNextAllowedTime, GetLocalTime() + EvasiveRollTuning::RollCooldown
			- FMath::Max(0.0, GetSynchronizedTime() - RollState.ServerStartTime));
	}
	else
	{
		bLocalPredictionActive = false;
		bLocalPresentationFinished = false;
	}
	UpdateVisualRoll();
}

bool UEvasiveRollComponent::IsRolling() const
{
	return CachedFlightComponent && CachedFlightComponent->IsEvasiveRollActive();
}

bool UEvasiveRollComponent::IsRollManeuverInProgress() const
{
	return IsRolling();
}

bool UEvasiveRollComponent::IsEvadingMissiles() const
{
	// Gameplay protection deliberately excludes unconfirmed client prediction.
	return GetOwner() && GetOwner()->HasAuthority() && IsRolling();
}

float UEvasiveRollComponent::GetCooldownRemaining() const
{
	return CachedFlightComponent
		? CachedFlightComponent->GetEvasiveRollCooldownRemaining() : 0.0f;
}

double UEvasiveRollComponent::GetSynchronizedTime() const
{
	if (!GetWorld())
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return GetWorld()->GetTimeSeconds();
}

double UEvasiveRollComponent::GetLocalTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UEvasiveRollComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEvasiveRollComponent, RollState);
}
