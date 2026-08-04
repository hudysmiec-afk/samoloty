#pragma once

#include "CoreMinimal.h"
#include "PlaneWeaponComponent.h"
#include "ShotgunGunComponent.generated.h"

class ARifleTracerVisual;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;

/** Close-range hitscan weapon that fires a server-authoritative pellet spread. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UShotgunGunComponent : public UPlaneWeaponComponent
{
	GENERATED_BODY()

public:
	UShotgunGunComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void SetFireHeld(bool bHeld) override;
	virtual void SetAimContext(const FVector& AimOrigin, const FVector& AimDirection) override;
	virtual void SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Shotgun|Debug")
	bool bDrawPelletDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Shotgun|Debug")
	bool bShowShotgunDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Shotgun|Aim", meta=(ClampMin="100"))
	float MaxCameraOriginDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Shotgun|Aim", meta=(ClampMin="0"))
	float MinimumCameraHitForwardDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Shotgun|Aim", meta=(ClampMin="1", ClampMax="120"))
	float AimUpdatesPerSecond = 30.0f;

protected:
	/** Cosmetic projectile/tracer spawned separately for every pellet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Shotgun|Visual",
		meta=(DisplayName="Visual Projectile Class"))
	TSubclassOf<ARifleTracerVisual> TracerVisualClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Shotgun|Visual")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Shotgun|Visual")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Shotgun|Audio")
	TObjectPtr<USoundBase> ShotgunFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Shotgun|Audio")
	TObjectPtr<USoundBase> ShotgunImpactSound;

private:
	virtual void OnWeaponEquippedChanged() override;

	UFUNCTION(Server, Reliable)
	void ServerSetFireHeld(bool bHeld);

	UFUNCTION(Server, Unreliable)
	void ServerSetCameraAim(FVector_NetQuantize100 CameraOrigin,
		FVector_NetQuantizeNormal CameraDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayBlast(const TArray<FVector_NetQuantize>& PelletEnds,
		const TArray<uint8>& HitFlags, uint8 MuzzleIndex);

	void FirePredictedBlast();
	void FireAuthoritativeBlast();
	int32 BuildPelletPaths(bool bApplyDamage, int32 ShotSeed,
		TArray<FVector_NetQuantize>& OutPelletEnds, TArray<uint8>& OutHitFlags) const;
	void PlayBlastVisual(const TArray<FVector_NetQuantize>& PelletEnds,
		const TArray<uint8>& HitFlags, uint8 MuzzleIndex) const;
	USceneComponent* GetMuzzle(uint8 MuzzleIndex) const;
	void DrawShotgunDebug() const;

	TWeakObjectPtr<USceneComponent> LeftMuzzle;
	TWeakObjectPtr<USceneComponent> RightMuzzle;
	bool bLocalFireHeld = false;
	bool bServerFireHeld = false;
	bool bLocalCameraAimEnabled = false;
	bool bServerCameraAimEnabled = false;
	FVector LocalCameraOrigin = FVector::ZeroVector;
	FVector LocalCameraDirection = FVector::ForwardVector;
	FVector ServerCameraOrigin = FVector::ZeroVector;
	FVector ServerCameraDirection = FVector::ForwardVector;
	double NextAimUpdateTime = 0.0;
	double NextLocalShotTime = 0.0;
	double NextServerShotTime = 0.0;
	int32 LocalShotSequence = 0;
	int32 ServerShotSequence = 0;

	UPROPERTY(Replicated)
	int32 ServerBlastsFired = 0;

	UPROPERTY(Replicated)
	int32 ServerPelletHits = 0;
};
