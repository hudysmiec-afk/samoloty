#pragma once

#include "CoreMinimal.h"
#include "CombatImpactTypes.h"
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
	virtual void StartEquipCooldown() override;
	virtual bool GetCooldownStatus(float& OutRemainingSeconds,
		float& OutDurationSeconds) const override;
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
	void MulticastPlayBlast(const TArray<FCombatImpactEvent>& PelletImpacts,
		uint8 MuzzleIndex, uint16 Sequence);

	UFUNCTION(Server, Unreliable)
	void ServerFireBlast(uint16 Sequence, uint8 MuzzleIndex,
		FVector_NetQuantize100 CameraOrigin,
		FVector_NetQuantizeNormal CameraDirection, bool bUseCameraAim,
		double ClientFireServerTime);

	void FirePredictedBlast();
	void HandleServerFireBlast(uint16 Sequence, uint8 MuzzleIndex,
		const FVector& CameraOrigin, const FVector& CameraDirection, bool bUseCameraAim,
		double ClientFireServerTime);
	void FireAuthoritativeBlast(uint16 Sequence, uint8 MuzzleIndex,
		double RewindServerTime = 0.0);
	int32 BuildPelletPaths(bool bApplyDamage, int32 ShotSeed,
		uint16 Sequence, TArray<FCombatImpactEvent>& OutPelletImpacts) const;
	void PlayBlastVisual(const TArray<FCombatImpactEvent>& PelletImpacts,
		uint8 MuzzleIndex) const;
	void PlayConfirmedBlastImpacts(const TArray<FCombatImpactEvent>& PelletImpacts,
		const TArray<FCombatImpactEvent>* PredictedImpacts) const;
	USceneComponent* GetMuzzle(uint8 MuzzleIndex) const;
	double GetEstimatedServerTime() const;
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
	uint16 LocalShotSequence = 0;
	uint16 ServerAiShotSequence = 0;
	TMap<uint16, TArray<FCombatImpactEvent>> PredictedBlastImpacts;

	UPROPERTY(Replicated)
	int32 ServerBlastsFired = 0;

	UPROPERTY(Replicated)
	int32 ServerPelletHits = 0;
};
