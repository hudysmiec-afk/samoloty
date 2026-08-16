#include "WorldNameComponent.h"

#include "Net/UnrealNetwork.h"

UWorldNameComponent::UWorldNameComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWorldNameComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWorldNameComponent, DisplayName);
	DOREPLIFETIME(UWorldNameComponent, NameColor);
	DOREPLIFETIME(UWorldNameComponent, bShowNameplate);
}
