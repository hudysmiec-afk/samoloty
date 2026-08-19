#include "TargetIntentComponent.h"

#include "Net/UnrealNetwork.h"

UTargetIntentComponent::UTargetIntentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTargetIntentComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTargetIntentComponent, CurrentTarget);
}

void UTargetIntentComponent::SetCurrentTarget(AActor* NewTarget)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}
	if (!IsValid(NewTarget) || NewTarget->IsActorBeingDestroyed())
	{
		NewTarget = nullptr;
	}
	if (CurrentTarget == NewTarget)
	{
		return;
	}
	CurrentTarget = NewTarget;
	OwnerActor->ForceNetUpdate();
}
