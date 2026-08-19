#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlaneWeaponComponent.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Gun,
	Missile
};

/** Common contract used by the loadout system for every aircraft weapon. */
UCLASS(Abstract, Blueprintable, ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UPlaneWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlaneWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons")
	virtual void SetFireHeld(bool bHeld);

	virtual void SetAimContext(const FVector& AimOrigin, const FVector& AimDirection);
	virtual void SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint);
	virtual void RejectCurrentTarget();
	virtual void SetWeaponEquipped(bool bEquipped);
	/** Starts this weapon's normal cooldown when it becomes active after a swap. */
	virtual void StartEquipCooldown();

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	EWeaponSlot GetWeaponSlot() const { return WeaponSlot; }

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	FName GetWeaponDisplayName() const { return WeaponDisplayName; }

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	bool IsWeaponEquipped() const { return bWeaponEquipped; }

	/** Common HUD contract. Remaining zero means the weapon is ready. */
	virtual bool GetCooldownStatus(float& OutRemainingSeconds,
		float& OutDurationSeconds) const;

protected:
	virtual void OnWeaponEquippedChanged();
	/** Shared local/server gate for mobility states in which weapon fire is invalid. */
	bool IsOwnerFireBlocked() const;
	void SetWeaponSlot(EWeaponSlot InSlot) { WeaponSlot = InSlot; }
	void SetWeaponDisplayName(FName InName) { WeaponDisplayName = InName; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	EWeaponSlot WeaponSlot = EWeaponSlot::Gun;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	FName WeaponDisplayName = TEXT("Weapon");

private:
	bool bWeaponEquipped = true;
};
