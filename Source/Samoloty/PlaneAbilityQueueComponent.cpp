#include "PlaneAbilityQueueComponent.h"

#include "ArcadeFlightComponent.h"
#include "BackwardDashComponent.h"
#include "BlinkComponent.h"
#include "EvasiveRollComponent.h"
#include "ForwardDashComponent.h"
#include "HealthComponent.h"
#include "QuickReversalComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

namespace PlaneAbilityQueueTuning
{
	constexpr double ExecutionRetryWindow = 0.50;
}

UPlaneAbilityQueueComponent::UPlaneAbilityQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void UPlaneAbilityQueueComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
		CachedBackwardDashComponent = Owner->FindComponentByClass<UBackwardDashComponent>();
		CachedBlinkComponent = Owner->FindComponentByClass<UBlinkComponent>();
		CachedForwardDashComponent = Owner->FindComponentByClass<UForwardDashComponent>();
		CachedEvasiveRollComponent = Owner->FindComponentByClass<UEvasiveRollComponent>();
		CachedQuickReversalComponent = Owner->FindComponentByClass<UQuickReversalComponent>();
		CachedHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
	}
}

void UPlaneAbilityQueueComponent::RequestAbility(
	const EPlaneAbilityType Type, const float Direction, const FVector2D MovementInput)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || Type == EPlaneAbilityType::None)
	{
		return;
	}

	FPlaneAbilityRequest Request;
	Request.Type = Type;
	Request.Direction = Direction;
	Request.MovementInput = MovementInput;
	Request.Sequence = ++LocalRequestSequence;

	// Roll and backward movement occupy compatible presentation/movement channels.
	// Preserve immediate roll prediction whenever the requested roll is not blocked.
	if (!GetOwner()->HasAuthority()
		&& !HasBlockingAbilityFor(Type) && CachedEvasiveRollComponent)
	{
		if (Type == EPlaneAbilityType::EvasiveRollLeft)
		{
			CachedEvasiveRollComponent->PredictAbilityQueueRoll(EEvasiveRollDirection::Left);
		}
		else if (Type == EPlaneAbilityType::EvasiveRollRight)
		{
			CachedEvasiveRollComponent->PredictAbilityQueueRoll(EEvasiveRollDirection::Right);
		}
	}

	if (GetOwner()->HasAuthority())
	{
		HandleAuthoritativeRequest(Request);
	}
	else
	{
		ServerRequestAbility(Type, Direction, MovementInput, Request.Sequence);
	}
}

void UPlaneAbilityQueueComponent::ServerRequestAbility_Implementation(
	const EPlaneAbilityType Type, const float Direction,
	const FVector2D MovementInput, const uint16 Sequence)
{
	FPlaneAbilityRequest Request;
	Request.Type = Type;
	Request.Direction = FMath::Clamp(Direction, -1.0f, 1.0f);
	Request.MovementInput = FVector2D(
		FMath::Clamp(MovementInput.X, -1.0f, 1.0f),
		FMath::Clamp(MovementInput.Y, -1.0f, 1.0f));
	Request.Sequence = Sequence;
	HandleAuthoritativeRequest(Request);
}

void UPlaneAbilityQueueComponent::HandleAuthoritativeRequest(
	const FPlaneAbilityRequest& Request)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| Request.Type == EPlaneAbilityType::None)
	{
		return;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead()))
	{
		ClearQueue();
		return;
	}

	if (HasBlockingAbilityFor(Request.Type))
	{
		QueuedRequest = Request;
		QueueReadySince = 0.0;
		return;
	}
	TryExecuteAbility(Request);
}

void UPlaneAbilityQueueComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| QueuedRequest.Type == EPlaneAbilityType::None)
	{
		return;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead()))
	{
		ClearQueue();
		return;
	}
	if (HasBlockingAbilityFor(QueuedRequest.Type))
	{
		QueueReadySince = 0.0;
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (QueueReadySince <= 0.0)
	{
		QueueReadySince = Now;
	}
	if (TryExecuteAbility(QueuedRequest)
		|| Now - QueueReadySince >= PlaneAbilityQueueTuning::ExecutionRetryWindow)
	{
		ClearQueue();
	}
}

bool UPlaneAbilityQueueComponent::HasBlockingAbilityFor(
	const EPlaneAbilityType RequestedType) const
{
	const bool bRollActive = CachedEvasiveRollComponent
		&& CachedEvasiveRollComponent->IsRollManeuverInProgress();
	const bool bReversalActive = CachedQuickReversalComponent
		&& CachedQuickReversalComponent->IsReversalActive();
	const bool bBackwardDashActive = CachedBackwardDashComponent
		&& CachedBackwardDashComponent->IsDashActive();
	const bool bForwardDashActive = CachedForwardDashComponent
		&& CachedForwardDashComponent->IsDashActive();

	if (bReversalActive)
	{
		return true;
	}
	switch (RequestedType)
	{
	case EPlaneAbilityType::EvasiveRollLeft:
	case EPlaneAbilityType::EvasiveRollRight:
		return bRollActive;
	case EPlaneAbilityType::BackwardDash:
		return bBackwardDashActive || bForwardDashActive;
	case EPlaneAbilityType::ForwardDash:
		return bBackwardDashActive || bForwardDashActive;
	case EPlaneAbilityType::Blink:
		return bBackwardDashActive || bForwardDashActive;
	case EPlaneAbilityType::QuickReversal:
		return bRollActive || bBackwardDashActive || bForwardDashActive;
	default:
		return false;
	}
}

bool UPlaneAbilityQueueComponent::TryExecuteAbility(
	const FPlaneAbilityRequest& Request)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	switch (Request.Type)
	{
	case EPlaneAbilityType::EvasiveRollLeft:
		return CachedEvasiveRollComponent
			&& CachedEvasiveRollComponent->TryActivateFromAbilityQueue(
				EEvasiveRollDirection::Left);
	case EPlaneAbilityType::EvasiveRollRight:
		return CachedEvasiveRollComponent
			&& CachedEvasiveRollComponent->TryActivateFromAbilityQueue(
				EEvasiveRollDirection::Right);
	case EPlaneAbilityType::QuickReversal:
		return CachedQuickReversalComponent
			&& CachedQuickReversalComponent->TryActivateFromAbilityQueue(Request.Direction);
	case EPlaneAbilityType::BackwardDash:
		return CachedBackwardDashComponent
			&& CachedBackwardDashComponent->TryActivateFromAbilityQueue();
	case EPlaneAbilityType::ForwardDash:
		return CachedForwardDashComponent
			&& CachedForwardDashComponent->TryActivateFromAbilityQueue();
	case EPlaneAbilityType::Blink:
		return CachedBlinkComponent
			&& CachedBlinkComponent->TryActivateFromAbilityQueue(Request.MovementInput);
	default:
		return false;
	}
}

void UPlaneAbilityQueueComponent::ClearQueue()
{
	QueuedRequest = FPlaneAbilityRequest();
	QueueReadySince = 0.0;
}
