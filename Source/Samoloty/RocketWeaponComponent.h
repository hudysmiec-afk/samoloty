#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RocketWeaponComponent.generated.h"

class ARocketProjectile;
class USceneComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ERocketWeaponState : uint8
{
	Ready,
	FiringSalvos,
	Cooldown
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API URocketWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URocketWeaponComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetSpawnPoints(USceneComponent* InLeftSpawnPoint, USceneComponent* InRightSpawnPoint);

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons")
	void SetFireHeld(bool bHeld);

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	ERocketWeaponState GetWeaponState() const { return WeaponState; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Debug")
	bool bShowWeaponDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Debug")
	bool bDrawRocketDebug = false;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	TSubclassOf<ARocketProjectile> RocketClass;

	/** Local 3D sound played once for each complete salvo, not once per rocket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Effects")
	TObjectPtr<USoundBase> RocketLaunchSound;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetFireHeld(bool bHeld);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySalvoLaunchSound(FVector_NetQuantize Location);

	void SetAuthoritativeFireHeld(bool bHeld);
	void StartBarrage();
	void FireSalvo();
	void EnterCooldown();
	void SpawnRocket(USceneComponent* SpawnPoint, bool bLeftSide, int32 SideIndex, int32 SideCount,
		FRandomStream& RandomStream);
	void HandleRocketFinished(ARocketProjectile* Rocket);
	void UpdateDebugCounts(float DeltaTime);
	void DrawWeaponDebug() const;
	double GetServerTimeSeconds() const;

	UPROPERTY(Replicated)
	ERocketWeaponState WeaponState = ERocketWeaponState::Ready;

	UPROPERTY(Replicated)
	int32 SalvosFired = 0;

	UPROPERTY(Replicated)
	double NextActionServerTime = 0.0;

	UPROPERTY(Replicated)
	int32 ActiveOwnedRocketCount = 0;

	UPROPERTY(Replicated)
	int32 ServerActiveRocketCountForDebug = 0;

	TWeakObjectPtr<USceneComponent> LeftSpawnPoint;
	TWeakObjectPtr<USceneComponent> RightSpawnPoint;
	TSet<TWeakObjectPtr<ARocketProjectile>> ActiveOwnedRockets;
	bool bLocalFireHeld = false;
	bool bServerFireHeld = false;
	float DebugCountAccumulator = 0.0f;
};
