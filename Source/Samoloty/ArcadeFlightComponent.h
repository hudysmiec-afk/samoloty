#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcadeFlightComponent.generated.h"

class UJetBoostComponent;
class UJetStatsComponent;

USTRUCT()
struct FArcadeFlightNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Location;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize100 Velocity;

	UPROPERTY()
	double ServerTimeSeconds = 0.0;

	UPROPERTY()
	uint16 Sequence = 0;

	UPROPERTY()
	bool bTeleport = false;
};

struct FBufferedFlightState
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Velocity = FVector::ZeroVector;
	double ServerTimeSeconds = 0.0;
	uint16 Sequence = 0;
};

UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UArcadeFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcadeFlightComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called by the owning Pawn. Values are normalized to -1..1. */
	void SetLocalFlightInput(const FVector2D& Steering, float Strafe, float Brake);

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	FVector2D GetSmoothedSteering() const { return SmoothedSteering; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetSmoothedStrafe() const { return SmoothedStrafe; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentForwardSpeed() const { return CurrentForwardSpeed; }

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetCurrentTurnRateMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Plane|Flight")
	float GetSmoothedBrake() const { return SmoothedBrake; }

private:
	UFUNCTION(Server, Unreliable)
	void ServerSetFlightInput(FVector2D Steering, float Strafe, float Brake, uint16 InputSequence);

	void SimulateFlight(float DeltaTime);
	void RotateAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void MoveAircraft(float DeltaTime, const struct FJetFlightStats& Stats);
	void UpdateReplicatedState();
	void InterpolateBufferedState();
	void UpdateLocalPresentationInput(float DeltaTime);
	float CalculateTurnRateMultiplier(const struct FJetFlightStats& Stats) const;

	UFUNCTION()
	void OnRep_ServerState();

	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0.016", ClampMax="0.25"))
	float InputReplicationInterval = 0.033f;

	/** How far behind server time clients render, leaving two snapshots to blend between. */
	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0.05", ClampMax="0.3"))
	float SnapshotInterpolationDelay = 0.1f;

	/** Prevents a long freeze if several packets are lost. */
	UPROPERTY(EditAnywhere, Category="Plane|Networking", meta=(ClampMin="0", ClampMax="0.25"))
	float MaxExtrapolationTime = 0.1f;

	UPROPERTY(ReplicatedUsing=OnRep_ServerState)
	FArcadeFlightNetworkState ServerState;

	FVector2D RawSteering = FVector2D::ZeroVector;
	FVector2D SmoothedSteering = FVector2D::ZeroVector;
	float RawStrafe = 0.0f;
	float SmoothedStrafe = 0.0f;
	float RawBrake = 0.0f;
	float SmoothedBrake = 0.0f;
	float CurrentForwardSpeed = 0.0f;
	FVector CurrentVelocity = FVector::ZeroVector;
	float TimeSinceInputSent = 0.0f;
	uint16 LocalInputSequence = 0;
	TArray<FBufferedFlightState> SnapshotBuffer;
};
