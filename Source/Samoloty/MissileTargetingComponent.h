#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MissileTargetingComponent.generated.h"

class AActor;
class UTargetSelectionComponent;
struct FHomingMissileStats;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMissileTargetChangedSignature,
	AActor*, PreviousTarget, AActor*, CurrentTarget);

/**
 * Local missile-lock evaluation for the currently selected radar contact. The
 * server does not trust the result; every requested target is validated again.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UMissileTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMissileTargetingComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void SetTargetingEnabled(bool bEnabled);
	void RejectCurrentTarget();

	UFUNCTION(BlueprintPure, Category="Plane|Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Plane|Targeting")
	bool HasLockedTarget() const { return CurrentTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Plane|Targeting")
	bool IsTargetingEnabled() const { return bTargetingEnabled; }

	UPROPERTY(BlueprintAssignable, Category="Plane|Targeting")
	FMissileTargetChangedSignature OnTargetChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Targeting|Debug")
	bool bShowTargetingDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Targeting",
		meta=(ClampMin="0.02", Units="s"))
	float TargetScanInterval = 0.1f;

private:
	void ScanTargets();
	void SetCurrentTarget(AActor* NewTarget);
	bool IsEligibleTarget(const AActor* Candidate, const FHomingMissileStats& Stats) const;
	const FHomingMissileStats* GetStats() const;
	void DrawTargetingDebug() const;

	TWeakObjectPtr<AActor> CurrentTarget;
	float ScanAccumulator = 0.0f;
	bool bTargetingEnabled = false;
};
