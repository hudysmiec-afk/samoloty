#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcadeFlightComponent.generated.h"

class UJetBoostComponent;
class UJetStatsComponent;

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UArcadeFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcadeFlightComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called by the owning Pawn. Values are normalized to -1..1. */
	void SetLocalFlightInput(const FVector2D& Steering, float Strafe);

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	FVector2D GetSmoothedSteering() const { return SmoothedSteering; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetSmoothedStrafe() const { return SmoothedStrafe; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentForwardSpeed() const { return CurrentForwardSpeed; }

private:
	UFUNCTION(Server, Unreliable)
	void ServerSetFlightInput(FVector2D Steering, float Strafe, uint16 InputSequence);

	void SimulateFlight(float DeltaTime);
	void RotateAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void MoveAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	bool ShouldSimulate() const;

	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0.016", ClampMax="0.25"))
	float InputReplicationInterval = 0.05f;

	FVector2D RawSteering = FVector2D::ZeroVector;
	FVector2D SmoothedSteering = FVector2D::ZeroVector;
	float RawStrafe = 0.0f;
	float SmoothedStrafe = 0.0f;
	float CurrentForwardSpeed = 0.0f;
	float TimeSinceInputSent = 0.0f;
	uint16 LocalInputSequence = 0;
};
