#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RifleTracerVisual.generated.h"

class UStaticMeshComponent;

/** Lightweight, local-only visual for a hitscan shot. It never owns collision or damage. */
UCLASS(Blueprintable)
class SAMOLOTY_API ARifleTracerVisual : public AActor
{
	GENERATED_BODY()

public:
	ARifleTracerVisual();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Plane|Weapons|Rifle")
	void InitializeTracer(const FVector& Start, const FVector& End, float Speed, float Length, float Width);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tracer")
	TObjectPtr<UStaticMeshComponent> TracerMesh;

private:
	void UpdateVisual();

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	float TotalDistance = 0.0f;
	float HeadDistance = 0.0f;
	float VisualSpeed = 1.0f;
	float VisualLength = 1.0f;
	float VisualWidth = 1.0f;
	bool bInitialized = false;
};
