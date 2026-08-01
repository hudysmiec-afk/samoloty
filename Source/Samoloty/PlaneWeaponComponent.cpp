#include "PlaneWeaponComponent.h"

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
