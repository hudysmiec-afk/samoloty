#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GunAimAssistComponent.generated.h"

/** Optional shared targeting assistance used by fixed forward-firing guns. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UGunAimAssistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunAimAssistComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons|Aim Assist")
	void SetAimAssistEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Plane|Weapons|Aim Assist")
	bool IsAimAssistEnabled() const { return bAimAssistEnabled; }

	/** Returns the selected target point when it passes the camera cone and gun range checks. */
	bool FindAssistedAimPoint(const FVector& AimOrigin, const FVector& AimDirection,
		float WeaponRange, FVector& OutAimPoint) const;

	/** Master switch. Equipment can toggle this later without modifying each weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated,
		Category="Plane|Weapons|Aim Assist")
	bool bAimAssistEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Aim Assist",
		meta=(ClampMin="0.0", ClampMax="30.0", Units="deg"))
	float AimAssistAngleDegrees = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Weapons|Aim Assist|Debug")
	bool bDrawAimAssistDebug = false;
};
