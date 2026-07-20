#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ArcadeJetPawn.generated.h"

class UArcadeFlightComponent;
class UCameraComponent;
class UCapsuleComponent;
class UJetBoostComponent;
class UJetStatsComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** Input, camera and presentation shell for the network-ready flight components. */
UCLASS(Blueprintable)
class SAMOLOTY_API AArcadeJetPawn : public APawn
{
	GENERATED_BODY()

public:
	AArcadeJetPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnRep_ReplicatedMovement() override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UCapsuleComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UStaticMeshComponent> PlaneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UJetStatsComponent> JetStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UJetBoostComponent> JetBoost;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UArcadeFlightComponent> FlightMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Input", meta=(ClampMin="0", ClampMax="0.5"))
	float CursorDeadZone = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Input", meta=(ClampMin="0.1", ClampMax="4"))
	float CursorResponseExponent = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Input")
	bool bInvertMouseY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0"))
	float CameraLagSpeed = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0.1"))
	float MaxCameraSideShift = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0"))
	float StrafeCameraSideShift = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0.1"))
	float MaxCameraVerticalShift = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0.1"))
	float CameraFramingSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="5", ClampMax="170"))
	float NormalFieldOfView = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="5", ClampMax="170"))
	float BoostFieldOfView = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0"))
	float NormalCameraDistance = 1150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0"))
	float BoostCameraDistance = 1350.0f;

	/** Visual error correction speed used only for aircraft observed over the network. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Networking", meta=(ClampMin="0.1"))
	float NetworkVisualSmoothingSpeed = 12.0f;

	/** Larger corrections snap immediately to avoid a visual aircraft far away from its collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Networking", meta=(ClampMin="0"))
	float MaxNetworkVisualError = 1200.0f;

private:
	void SetStrafe(float Value);
	void StartBoost();
	void StopBoost();
	void UpdateCursorInput();
	void UpdateLocalCamera(float DeltaSeconds);
	void CacheNetworkSmoothedComponents();
	void UpdateNetworkVisualSmoothing(float DeltaSeconds);
	bool IsNetworkSimulatedProxy() const;

	FVector2D CursorSteering = FVector2D::ZeroVector;
	float StrafeInput = 0.0f;
	FVector CurrentCameraSocketOffset = FVector(0.0f, 0.0f, 180.0f);
	TMap<TWeakObjectPtr<USceneComponent>, FTransform> VisualBaseRelativeTransforms;
};
