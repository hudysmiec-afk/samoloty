#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetIntentComponent.generated.h"

/**
 * Replicated, presentation-safe information about the actor's current combat target.
 * AI and player targeting systems write it on authority; HUDs only read it.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UTargetIntentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetIntentComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Combat|Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category="Combat|Targeting")
	bool IsTargeting(const AActor* Actor) const { return CurrentTarget == Actor; }

	/** Authority only. Invalid and destroyed actors are treated as no target. */
	void SetCurrentTarget(AActor* NewTarget);

private:
	UPROPERTY(Replicated)
	TObjectPtr<AActor> CurrentTarget;
};
