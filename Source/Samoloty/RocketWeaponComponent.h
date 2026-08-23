#pragma once

#include "CoreMinimal.h"
#include "PlaneWeaponComponent.h"
#include "RocketWeaponComponent.generated.h"

class ARocketProjectile;
class USceneComponent;
class USoundBase;
struct FRocketBarrageStats;
struct FRocketLaunchData;
struct FRocketSeparationPath;

UENUM(BlueprintType)
enum class ERocketWeaponState : uint8
{
	Ready,
	FiringSalvos,
	Cooldown
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API URocketWeaponComponent : public UPlaneWeaponComponent
{
	GENERATED_BODY()

public:
	URocketWeaponComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetSpawnPoints(USceneComponent* InLeftSpawnPoint, USceneComponent* InRightSpawnPoint);
	virtual void SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint) override;

	virtual void SetFireHeld(bool bHeld) override;
	virtual void StartEquipCooldown() override;
	virtual bool GetCooldownStatus(float& OutRemainingSeconds,
		float& OutDurationSeconds) const override;

	UFUNCTION(BlueprintPure, Category="Plane|Weapons")
	ERocketWeaponState GetWeaponState() const { return WeaponState; }
	void DrawWeaponDebug(bool bForceDisplay = false) const;

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

	/** Called once on the server before every salvo. Derived barrage variants can cache a shared aim point. */
	virtual void PrepareSalvo(const FRocketBarrageStats& Stats);

	/** Allows a barrage variant to alter only the final flight direction while retaining shared salvo logic. */
	virtual void ConfigureRocketLaunchData(FRocketLaunchData& LaunchData,
		const FRocketSeparationPath& SeparationPath, FRandomStream& RandomStream) const;
	virtual bool UsesDataOnlyRocketSimulation() const { return true; }

	/** Copies shared projectile/effect defaults only where this component still uses native defaults. */
	void InheritMissingPresentationFrom(const URocketWeaponComponent& Source);

private:
	virtual void OnWeaponEquippedChanged() override;

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
