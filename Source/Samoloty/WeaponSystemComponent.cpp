#include "WeaponSystemComponent.h"

#include "ArcadeFlightComponent.h"
#include "HomingMissileWeaponComponent.h"
#include "RocketWeaponComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
	bool IsValidWeaponSlot(const EWeaponSlot Slot)
	{
		return Slot == EWeaponSlot::Gun || Slot == EWeaponSlot::Missile;
	}
}

UWeaponSystemComponent::UWeaponSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UWeaponSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);
	DiscoverWeapons();
	ApplySelection(EWeaponSlot::Gun);
	ApplySelection(EWeaponSlot::Missile);
}

void UWeaponSystemComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UPlaneWeaponComponent* ActiveMissile = GetActiveWeapon(EWeaponSlot::Missile);
	if (const URocketWeaponComponent* Rocket = Cast<URocketWeaponComponent>(ActiveMissile))
	{
		Rocket->DrawWeaponDebug();
	}
	else if (const UHomingMissileWeaponComponent* Homing =
		Cast<UHomingMissileWeaponComponent>(ActiveMissile))
	{
		Homing->DrawWeaponDebug();
	}
}

void UWeaponSystemComponent::DiscoverWeapons()
{
	GunWeapons.Reset();
	MissileWeapons.Reset();
	bWeaponsDiscovered = true;
	if (!GetOwner())
	{
		return;
	}

	TArray<UPlaneWeaponComponent*> FoundWeapons;
	GetOwner()->GetComponents<UPlaneWeaponComponent>(FoundWeapons);
	FoundWeapons.Sort([](const UPlaneWeaponComponent& Left, const UPlaneWeaponComponent& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});
	for (UPlaneWeaponComponent* Weapon : FoundWeapons)
	{
		if (!Weapon)
		{
			continue;
		}
		GetWeaponsMutable(Weapon->GetWeaponSlot()).Add(Weapon);
	}

	for (UPlaneWeaponComponent* Weapon : GunWeapons)
	{
		Weapon->SetFirePoints(GunLeftPoint.Get(), GunRightPoint.Get());
	}
	for (UPlaneWeaponComponent* Weapon : MissileWeapons)
	{
		Weapon->SetFirePoints(MissileLeftPoint.Get(), MissileRightPoint.Get());
	}
}

void UWeaponSystemComponent::SetFireHeld(const EWeaponSlot Slot, const bool bHeld)
{
	if (!IsValidWeaponSlot(Slot))
	{
		return;
	}
	const UArcadeFlightComponent* Flight = GetOwner()
		? GetOwner()->FindComponentByClass<UArcadeFlightComponent>() : nullptr;
	if (bHeld && Flight && !Flight->IsCombatFlightEnabled())
	{
		return;
	}
	GetFireHeldMutable(Slot) = bHeld;
	if (UPlaneWeaponComponent* Weapon = GetActiveWeapon(Slot))
	{
		Weapon->SetFireHeld(bHeld);
	}
}

void UWeaponSystemComponent::CycleWeapon(const EWeaponSlot Slot)
{
	if (!IsValidWeaponSlot(Slot) || !GetOwner())
	{
		return;
	}
	const bool bContinueFiring = GetFireHeld(Slot);
	// Release the old implementation without clearing the physical trigger state.
	if (UPlaneWeaponComponent* CurrentWeapon = GetActiveWeapon(Slot))
	{
		CurrentWeapon->SetFireHeld(false);
	}
	if (GetOwner()->HasAuthority())
	{
		CycleWeaponAuthoritative(Slot, bContinueFiring);
	}
	else
	{
		ServerCycleWeapon(Slot, bContinueFiring);
	}
}

void UWeaponSystemComponent::RejectCurrentTarget(const EWeaponSlot Slot)
{
	if (!IsValidWeaponSlot(Slot))
	{
		return;
	}
	if (UPlaneWeaponComponent* Weapon = GetActiveWeapon(Slot))
	{
		Weapon->RejectCurrentTarget();
	}
}

void UWeaponSystemComponent::ServerCycleWeapon_Implementation(
	const EWeaponSlot Slot, const bool bContinueFiring)
{
	if (!IsValidWeaponSlot(Slot))
	{
		return;
	}
	CycleWeaponAuthoritative(Slot, bContinueFiring);
}

void UWeaponSystemComponent::CycleWeaponAuthoritative(
	const EWeaponSlot Slot, const bool bContinueFiring)
{
	TArray<TObjectPtr<UPlaneWeaponComponent>>& Weapons = GetWeaponsMutable(Slot);
	if (Weapons.Num() == 0)
	{
		return;
	}

	if (UPlaneWeaponComponent* CurrentWeapon = GetActiveWeapon(Slot))
	{
		CurrentWeapon->SetFireHeld(false);
	}
	int32& ActiveIndex = GetActiveIndexMutable(Slot);
	ActiveIndex = (FMath::Clamp(ActiveIndex, 0, Weapons.Num() - 1) + 1) % Weapons.Num();
	ApplySelection(Slot);
	if (UPlaneWeaponComponent* NewWeapon = GetActiveWeapon(Slot))
	{
		NewWeapon->StartEquipCooldown();
		NewWeapon->SetFireHeld(bContinueFiring);
	}
	GetOwner()->ForceNetUpdate();
}

void UWeaponSystemComponent::ConfigureFirePoints(const EWeaponSlot Slot,
	USceneComponent* LeftPoint, USceneComponent* RightPoint)
{
	if (!IsValidWeaponSlot(Slot))
	{
		return;
	}
	if (Slot == EWeaponSlot::Gun)
	{
		GunLeftPoint = LeftPoint;
		GunRightPoint = RightPoint;
	}
	else
	{
		MissileLeftPoint = LeftPoint;
		MissileRightPoint = RightPoint;
	}

	for (UPlaneWeaponComponent* Weapon : GetWeapons(Slot))
	{
		if (Weapon)
		{
			Weapon->SetFirePoints(LeftPoint, RightPoint);
		}
	}
}

void UWeaponSystemComponent::SetAimContext(const FVector& AimOrigin, const FVector& AimDirection)
{
	if (UPlaneWeaponComponent* Gun = GetActiveWeapon(EWeaponSlot::Gun))
	{
		Gun->SetAimContext(AimOrigin, AimDirection);
	}
	if (UPlaneWeaponComponent* Missile = GetActiveWeapon(EWeaponSlot::Missile))
	{
		Missile->SetAimContext(AimOrigin, AimDirection);
	}
}

UPlaneWeaponComponent* UWeaponSystemComponent::GetActiveWeapon(const EWeaponSlot Slot) const
{
	if (!IsValidWeaponSlot(Slot))
	{
		return nullptr;
	}
	const TArray<TObjectPtr<UPlaneWeaponComponent>>& Weapons = GetWeapons(Slot);
	const int32 ActiveIndex = GetActiveIndex(Slot);
	return Weapons.IsValidIndex(ActiveIndex) ? Weapons[ActiveIndex].Get() : nullptr;
}

void UWeaponSystemComponent::ApplySelection(const EWeaponSlot Slot)
{
	TArray<TObjectPtr<UPlaneWeaponComponent>>& Weapons = GetWeaponsMutable(Slot);
	int32& ActiveIndex = GetActiveIndexMutable(Slot);
	ActiveIndex = Weapons.Num() > 0 ? FMath::Clamp(ActiveIndex, 0, Weapons.Num() - 1) : INDEX_NONE;
	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		if (UPlaneWeaponComponent* Weapon = Weapons[Index])
		{
			Weapon->SetWeaponEquipped(Index == ActiveIndex);
		}
	}
}

void UWeaponSystemComponent::OnRep_ActiveGunIndex()
{
	if (!bWeaponsDiscovered)
	{
		DiscoverWeapons();
	}
	ApplySelection(EWeaponSlot::Gun);
	if (UPlaneWeaponComponent* NewWeapon = GetActiveWeapon(EWeaponSlot::Gun))
	{
		NewWeapon->StartEquipCooldown();
	}
	ResumeHeldFireAfterSelection(EWeaponSlot::Gun);
}

void UWeaponSystemComponent::OnRep_ActiveMissileIndex()
{
	if (!bWeaponsDiscovered)
	{
		DiscoverWeapons();
	}
	ApplySelection(EWeaponSlot::Missile);
	if (UPlaneWeaponComponent* NewWeapon = GetActiveWeapon(EWeaponSlot::Missile))
	{
		NewWeapon->StartEquipCooldown();
	}
	ResumeHeldFireAfterSelection(EWeaponSlot::Missile);
}

bool& UWeaponSystemComponent::GetFireHeldMutable(const EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Gun ? bGunFireHeld : bMissileFireHeld;
}

bool UWeaponSystemComponent::GetFireHeld(const EWeaponSlot Slot) const
{
	return Slot == EWeaponSlot::Gun ? bGunFireHeld : bMissileFireHeld;
}

void UWeaponSystemComponent::ResumeHeldFireAfterSelection(const EWeaponSlot Slot)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GetFireHeld(Slot))
	{
		return;
	}
	if (UPlaneWeaponComponent* Weapon = GetActiveWeapon(Slot))
	{
		Weapon->SetFireHeld(true);
	}
}

TArray<TObjectPtr<UPlaneWeaponComponent>>& UWeaponSystemComponent::GetWeaponsMutable(const EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Gun ? GunWeapons : MissileWeapons;
}

const TArray<TObjectPtr<UPlaneWeaponComponent>>& UWeaponSystemComponent::GetWeapons(const EWeaponSlot Slot) const
{
	return Slot == EWeaponSlot::Gun ? GunWeapons : MissileWeapons;
}

int32& UWeaponSystemComponent::GetActiveIndexMutable(const EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Gun ? ActiveGunIndex : ActiveMissileIndex;
}

int32 UWeaponSystemComponent::GetActiveIndex(const EWeaponSlot Slot) const
{
	return Slot == EWeaponSlot::Gun ? ActiveGunIndex : ActiveMissileIndex;
}

void UWeaponSystemComponent::DrawWeaponSystemDebug() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bShowWeaponSystemDebug || !Pawn || !Pawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}

	const UPlaneWeaponComponent* Gun = GetActiveWeapon(EWeaponSlot::Gun);
	const UPlaneWeaponComponent* Missile = GetActiveWeapon(EWeaponSlot::Missile);
	const FString Message = FString::Printf(
		TEXT("WEAPONS | [1] GUN: %s (%d) | [2] MISSILE: %s (%d)"),
		Gun ? *Gun->GetWeaponDisplayName().ToString() : TEXT("NONE"), GunWeapons.Num(),
		Missile ? *Missile->GetWeaponDisplayName().ToString() : TEXT("NONE"), MissileWeapons.Num());
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Orange, Message);
}

void UWeaponSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWeaponSystemComponent, ActiveGunIndex);
	DOREPLIFETIME(UWeaponSystemComponent, ActiveMissileIndex);
}
