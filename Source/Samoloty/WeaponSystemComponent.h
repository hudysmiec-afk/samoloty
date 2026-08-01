#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlaneWeaponComponent.h"
#include "WeaponSystemComponent.generated.h"

class UPlaneWeaponComponent;
class USceneComponent;

/** Selects equipped Gun/Missile weapons and routes input without knowing weapon types. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UWeaponSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponSystemComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons")
	void SetFireHeld(EWeaponSlot Slot, bool bHeld);

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons|Debug")
	void CycleWeapon(EWeaponSlot Slot);

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	UPlaneWeaponComponent* GetActiveWeapon(EWeaponSlot Slot) const;

	void ConfigureFirePoints(EWeaponSlot Slot, USceneComponent* LeftPoint, USceneComponent* RightPoint);
	void SetAimContext(const FVector& AimOrigin, const FVector& AimDirection);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Debug")
	bool bShowWeaponSystemDebug = true;

private:
	UFUNCTION(Server, Reliable)
	void ServerCycleWeapon(EWeaponSlot Slot);

	UFUNCTION()
	void OnRep_ActiveGunIndex();

	UFUNCTION()
	void OnRep_ActiveMissileIndex();

	void DiscoverWeapons();
	void ApplySelection(EWeaponSlot Slot);
	void CycleWeaponAuthoritative(EWeaponSlot Slot);
	void DrawWeaponSystemDebug() const;
	TArray<TObjectPtr<UPlaneWeaponComponent>>& GetWeaponsMutable(EWeaponSlot Slot);
	const TArray<TObjectPtr<UPlaneWeaponComponent>>& GetWeapons(EWeaponSlot Slot) const;
	int32& GetActiveIndexMutable(EWeaponSlot Slot);
	int32 GetActiveIndex(EWeaponSlot Slot) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPlaneWeaponComponent>> GunWeapons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPlaneWeaponComponent>> MissileWeapons;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveGunIndex)
	int32 ActiveGunIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveMissileIndex)
	int32 ActiveMissileIndex = 0;

	TWeakObjectPtr<USceneComponent> GunLeftPoint;
	TWeakObjectPtr<USceneComponent> GunRightPoint;
	TWeakObjectPtr<USceneComponent> MissileLeftPoint;
	TWeakObjectPtr<USceneComponent> MissileRightPoint;
	bool bWeaponsDiscovered = false;
};
