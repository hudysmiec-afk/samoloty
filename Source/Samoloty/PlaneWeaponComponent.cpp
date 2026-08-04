#include "PlaneWeaponComponent.h"

#include "QuickReversalComponent.h"
#include "GameFramework/Actor.h"

UPlaneWeaponComponent::UPlaneWeaponComponent()
{
	SetIsReplicatedByDefault(true);
}

void UPlaneWeaponComponent::SetFireHeld(const bool bHeld)
{
}

void UPlaneWeaponComponent::SetAimContext(const FVector& AimOrigin, const FVector& AimDirection)
{
}

void UPlaneWeaponComponent::SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint)
{
}

void UPlaneWeaponComponent::RejectCurrentTarget()
{
}

void UPlaneWeaponComponent::SetWeaponEquipped(const bool bEquipped)
{
	if (bWeaponEquipped == bEquipped)
	{
		return;
	}
	bWeaponEquipped = bEquipped;
	OnWeaponEquippedChanged();
}

void UPlaneWeaponComponent::OnWeaponEquippedChanged()
{
}

bool UPlaneWeaponComponent::IsOwnerFireBlocked() const
{
	const UQuickReversalComponent* Reversal = GetOwner()
		? GetOwner()->FindComponentByClass<UQuickReversalComponent>() : nullptr;
	return Reversal && Reversal->IsWeaponFireBlocked();
}
