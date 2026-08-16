#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MissileWarningComponent.generated.h"

class ARocketProjectile;
class UAudioComponent;
class USoundBase;

/** Local cockpit warning driven by homing projectiles currently tracking this aircraft. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UMissileWarningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMissileWarningComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void AddIncomingMissile(ARocketProjectile* Missile);
	void RemoveIncomingMissile(ARocketProjectile* Missile);

	UFUNCTION(BlueprintPure, Category="Plane|Audio|Missile Warning")
	int32 GetIncomingMissileCount() const { return IncomingMissiles.Num(); }

protected:
	/** Use a looping Sound Cue/MetaSound for a continuous warning siren. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Missile Warning")
	TObjectPtr<USoundBase> IncomingMissileWarningSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Missile Warning",
		meta=(ClampMin="0"))
	float WarningVolume = 1.0f;

private:
	bool IsLocalPlayerOwner() const;
	void UpdateWarningAudio();

	TSet<TWeakObjectPtr<ARocketProjectile>> IncomingMissiles;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WarningAudioComponent;
};
