#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RifleGunComponent.generated.h"

class ARifleTracerVisual;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API URifleGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URifleGunComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetMuzzlePoints(USceneComponent* InLeftMuzzle, USceneComponent* InRightMuzzle);

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons|Rifle")
	void SetFireHeld(bool bHeld);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Debug")
	bool bDrawShotDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Rifle|Debug")
	bool bShowRifleDebug = true;

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
	UFUNCTION(Server, Reliable)
	void ServerSetFireHeld(bool bHeld);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayShot(FVector_NetQuantize Start, FVector_NetQuantize End,
		FVector_NetQuantizeNormal Direction, uint8 MuzzleIndex, bool bHit);

	void FireAuthoritativeShot();
	void FirePredictedShot();
	void PlayShotVisual(const FVector& Start, const FVector& End, const FVector& Direction,
		uint8 MuzzleIndex, bool bHit) const;
	bool BuildShot(uint8 MuzzleIndex, bool bApplyDamage, FVector& OutStart, FVector& OutEnd,
		FVector& OutDirection, bool& bOutHit) const;
	USceneComponent* GetMuzzle(uint8 MuzzleIndex) const;
	void DrawRifleDebug() const;

	TWeakObjectPtr<USceneComponent> LeftMuzzle;
	TWeakObjectPtr<USceneComponent> RightMuzzle;
	bool bLocalFireHeld = false;
	bool bServerFireHeld = false;
	uint8 LocalMuzzleIndex = 0;
	uint8 ServerMuzzleIndex = 0;
	double NextLocalShotTime = 0.0;
	double NextServerShotTime = 0.0;
	UPROPERTY(Replicated)
	int32 ServerShotsFired = 0;

	UPROPERTY(Replicated)
	int32 ServerHits = 0;
};
