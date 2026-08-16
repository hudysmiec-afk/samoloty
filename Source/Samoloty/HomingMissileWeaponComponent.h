#pragma once

#include "CoreMinimal.h"
#include "PlaneWeaponComponent.h"
#include "HomingMissileWeaponComponent.generated.h"

class AActor;
class ARocketProjectile;
class UMissileTargetingComponent;
class USceneComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EHomingMissileWeaponState : uint8
{
	Ready,
	FiringSalvos,
	Cooldown
};

/** A server-authoritative guided missile weapon with barrage-style salvo timing. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UHomingMissileWeaponComponent : public UPlaneWeaponComponent
{
	GENERATED_BODY()

public:
	UHomingMissileWeaponComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void SetFireHeld(bool bHeld) override;
	virtual void SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint) override;
	virtual void RejectCurrentTarget() override;
	virtual bool GetCooldownStatus(float& OutRemainingSeconds,
		float& OutDurationSeconds) const override;

	/** Server-only firing entry used by AI that owns its target selection. */
	void SetAITargetAndFire(AActor* TargetActor, bool bHeld);

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	EHomingMissileWeaponState GetWeaponState() const { return WeaponState; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Debug")
	bool bShowWeaponDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Debug")
	bool bDrawMissileDebug = false;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	TSubclassOf<ARocketProjectile> MissileClass;

	/** Local 3D sound played once per complete salvo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Effects")
	TObjectPtr<USoundBase> MissileLaunchSound;

private:
	virtual void OnWeaponEquippedChanged() override;

	UFUNCTION(Server, Reliable)
	void ServerSetFireHeld(bool bHeld, AActor* RequestedTargetActor);

	UFUNCTION(Server, Reliable)
	void ServerUpdateRequestedTarget(AActor* RequestedTargetActor);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySalvoLaunchSound(FVector_NetQuantize Location);

	void SetAuthoritativeFireHeld(bool bHeld, AActor* RequestedTargetActor);
	void UpdateRequestedTargetFromLocal();
	void StartSequence();
	void FireSalvo();
	void EnterCooldown();
	void SpawnMissile(USceneComponent* SpawnPoint, bool bLeftSide, int32 SideIndex,
		int32 SideCount, FRandomStream& RandomStream, AActor* TargetActor);
	AActor* ValidateRequestedTarget(AActor* TargetActor) const;
	AActor* GetLiveSequenceTarget() const;
	UMissileTargetingComponent* GetTargetingComponent() const;
	double GetServerTimeSeconds() const;
	void DrawWeaponDebug() const;

	UPROPERTY(Replicated)
	EHomingMissileWeaponState WeaponState = EHomingMissileWeaponState::Ready;

	UPROPERTY(Replicated)
	int32 SalvosFired = 0;

	UPROPERTY(Replicated)
	double NextActionServerTime = 0.0;

	TWeakObjectPtr<USceneComponent> LeftSpawnPoint;
	TWeakObjectPtr<USceneComponent> RightSpawnPoint;
	TWeakObjectPtr<AActor> RequestedTarget;
	TWeakObjectPtr<AActor> SequenceTarget;
	TWeakObjectPtr<AActor> LastTargetSentToServer;
	bool bLocalFireHeld = false;
	bool bServerFireHeld = false;
};
