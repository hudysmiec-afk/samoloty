#include "PlaneAbilityQueueComponent.h"

#include "ArcadeFlightComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UPlaneAbilityQueueComponent::UPlaneAbilityQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UPlaneAbilityQueueComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
	}
}

void UPlaneAbilityQueueComponent::RequestAbility(
	const EPlaneAbilityType Type, const float Direction, const FVector2D MovementInput)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()
		|| Type == EPlaneAbilityType::None || !CachedFlightComponent)
	{
		return;
	}

	CachedFlightComponent->QueuePredictedAbility(Type, Direction, MovementInput);
}

EPlaneAbilityType UPlaneAbilityQueueComponent::GetQueuedAbility() const
{
	return CachedFlightComponent
		? CachedFlightComponent->GetPredictedQueuedAbility()
		: EPlaneAbilityType::None;
}
