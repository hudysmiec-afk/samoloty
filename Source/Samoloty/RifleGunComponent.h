#pragma once

#include "CoreMinimal.h"
#include "PlaneWeaponComponent.h"
#include "RifleGunComponent.generated.h"

class ARifleTracerVisual;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API URifleGunComponent : public UPlaneWeaponComponent
{
	GENERATED_BODY()

public:
	URifleGunComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetMuzzlePoints(USceneComponent* InLeftMuzzle, USceneComponent* InRightMuzzle);
	void SetDamagePlayersOnly(bool bEnabled) { bDamagePlayersOnly = bEnabled; }
	void SetCameraAimEnabled(bool bEnabled);
	void SetCameraAim(const FVector& CameraOrigin, const FVector& CameraDirection);
	virtual void SetAimContext(const FVector& AimOrigin, const FVector& AimDirection) override;
	virtual void SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint) override;

	virtual void SetFireHeld(bool bHeld) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Debug")
	bool bDrawShotDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Debug")
	bool bShowRifleDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Aim", meta=(ClampMin="100"))
	float MaxCameraOriginDistance = 2500.0f;

	/** Camera hits closer than this in front of the aircraft are rejected so fixed guns never fire backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Aim", meta=(ClampMin="0"))
	float MinimumCameraHitForwardDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Aim", meta=(ClampMin="1", ClampMax="120"))
	float AimUpdatesPerSecond = 30.0f;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Rifle|Visual")
	TSubclassOf<ARifleTracerVisual> TracerVisualClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Rifle|Visual")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Rifle|Visual")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Rifle|Audio")
	TObjectPtr<USoundBase> RifleFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Rifle|Audio")
	TObjectPtr<USoundBase> RifleImpactSound;

private:
	virtual void OnWeaponEquippedChanged() override;

	UFUNCTION(Server, Reliable)
	void ServerSetFireHeld(bool bHeld);

	UFUNCTION(Server, Unreliable)
	void ServerSetCameraAim(FVector_NetQuantize100 CameraOrigin,
		FVector_NetQuantizeNormal CameraDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayShot(FVector_NetQuantize Start, FVector_NetQuantize End,
		FVector_NetQuantizeNormal Direction, uint8 MuzzleIndex, bool bHit);

	void FireAuthoritativeShot();
	void FirePredictedShot();
	void PlayShotVisual(const FVector& Start, const FVector& End, const FVector& Direction,
		uint8 MuzzleIndex, bool bHit) const;
	bool BuildShot(uint8 MuzzleIndex, bool bApplyDamage, FVector& OutStart, FVector& OutEnd,
		FVector& OutDirection, bool& bOutHit) const;
	FVector FindCameraAimPoint(const FVector& CameraOrigin, const FVector& CameraDirection,
		float MaxRange, FHitResult& OutHit) const;
	USceneComponent* GetMuzzle(uint8 MuzzleIndex) const;
	void DrawRifleDebug() const;

	TWeakObjectPtr<USceneComponent> LeftMuzzle;
	TWeakObjectPtr<USceneComponent> RightMuzzle;
	bool bLocalFireHeld = false;
	bool bServerFireHeld = false;
	bool bDamagePlayersOnly = false;
	bool bLocalCameraAimEnabled = false;
	bool bServerCameraAimEnabled = false;
	FVector LocalCameraOrigin = FVector::ZeroVector;
	FVector LocalCameraDirection = FVector::ForwardVector;
	FVector ServerCameraOrigin = FVector::ZeroVector;
	FVector ServerCameraDirection = FVector::ForwardVector;
	double NextAimUpdateTime = 0.0;
	uint8 LocalMuzzleIndex = 0;
	uint8 ServerMuzzleIndex = 0;
	double NextLocalShotTime = 0.0;
	double NextServerShotTime = 0.0;
	UPROPERTY(Replicated)
	int32 ServerShotsFired = 0;

	UPROPERTY(Replicated)
	int32 ServerHits = 0;
};
