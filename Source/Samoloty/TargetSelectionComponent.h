#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetSelectionComponent.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTargetSelectionChangedSignature,
	AActor*, PreviousTarget, AActor*, SelectedTarget);

/** Generic local target selection shared by missiles and future gun assistance. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UTargetSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetSelectionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Plane|Targeting")
	AActor* GetSelectedTarget() const { return SelectedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Plane|Targeting")
	bool HasSelectedTarget() const { return SelectedTarget.IsValid(); }

	/** Rejects the current target briefly; a new one still has to pass under the crosshair. */
	UFUNCTION(BlueprintCallable, Category="Plane|Targeting")
	void CycleSelectedTarget();

	UPROPERTY(BlueprintAssignable, Category="Plane|Targeting")
	FTargetSelectionChangedSignature OnSelectedTargetChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Targeting",
		meta=(ClampMin="0.02", Units="s"))
	float SelectionRefreshInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Targeting",
		meta=(ClampMin="0", Units="s"))
	float PreviousTargetRejectDuration = 1.0f;

	/** Extra angular margin around the target bounds used when acquiring a selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Targeting",
		meta=(ClampMin="0", ClampMax="20", Units="deg"))
	float CrosshairToleranceDegrees = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Targeting|Debug")
	bool bShowSelectionDebug = false;

private:
	void RefreshSelection();
	void CleanupRejectedTargets(double Now);
	void SetSelectedTarget(AActor* NewTarget);
	bool IsSelectable(const AActor* Candidate, double Now) const;
	bool IsUnderCrosshair(const AActor* Candidate, const FVector& ViewLocation,
		const FVector& ViewDirection, float& OutAimError) const;
	void GetSelectionViewPoint(FVector& OutLocation, FVector& OutDirection) const;
	void DrawSelectionDebug() const;

	TWeakObjectPtr<AActor> SelectedTarget;
	TMap<TWeakObjectPtr<AActor>, double> RejectedTargetsUntil;
	float RefreshAccumulator = 0.0f;
};
