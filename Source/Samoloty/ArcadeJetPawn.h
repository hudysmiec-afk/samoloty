#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ArcadeJetPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** Cursor-driven arcade aircraft controls. */
UCLASS(Blueprintable)
class SAMOLOTY_API AArcadeJetPawn : public APawn
{
	GENERATED_BODY()

public:
	AArcadeJetPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UCapsuleComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UStaticMeshComponent> PlaneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0"))
	float ForwardSpeed = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost", meta=(ClampMin="1.0"))
	float BoostSpeedMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost", meta=(ClampMin="0.1"))
	float BoostResponseSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost", meta=(ClampMin="5", ClampMax="170"))
	float NormalFieldOfView = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost", meta=(ClampMin="5", ClampMax="170"))
	float BoostFieldOfView = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost", meta=(ClampMin="0"))
	float NormalCameraDistance = 1150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Boost", meta=(ClampMin="0"))
	float BoostCameraDistance = 1350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0"))
	float StrafeSpeed = 700.0f;

	/** How quickly A/D reaches full strafe. Lower values make direction changes softer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0.1"))
	float StrafeResponseSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0"))
	float MaxYawTurnRate = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0"))
	float MaxPitchTurnRate = 42.0f;

	/** Radius around screen centre where the cursor does not steer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0", ClampMax="0.5"))
	float CursorDeadZone = 0.06f;

	/** Higher values give finer control near screen centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0.1", ClampMax="4"))
	float CursorResponseExponent = 1.35f;

	/** Higher values make mouse steering react faster; lower values remove twitching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0.1"))
	float InputSmoothingSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0"))
	float RotationResponsiveness = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0", ClampMax="89"))
	float MaxPitch = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0"))
	float MaxVisualBank = 35.0f;

	/** Independent bank angle produced by A/D strafe (does not affect StrafeSpeed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight", meta=(ClampMin="0", ClampMax="85"))
	float MaxStrafeBank = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Flight")
	bool bInvertMouseY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0"))
	float CameraLagSpeed = 6.0f;

	/** Maximum sideways framing shift while turning. Camera still points along the aircraft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0.1"))
	float MaxCameraSideShift = 160.0f;

	/** Extra screen-space offset while holding A/D. Does not change flight speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0"))
	float StrafeCameraSideShift = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0.1"))
	float MaxCameraVerticalShift = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera", meta=(ClampMin="0.1"))
	float CameraFramingSpeed = 5.0f;

	/** Use this in BP_PlayerPlane to enable engine trails, sound and UI effects. */
	UFUNCTION(BlueprintImplementableEvent, Category="Plane|Boost")
	void OnBoostStateChanged(bool bBoosting);

private:
	void SetStrafe(float Value);
	void StartBoost();
	void StopBoost();
	void UpdateCursorSteering(float DeltaSeconds);
	void MoveAircraft(float DeltaSeconds);
	void RotateAircraft(float DeltaSeconds);

	float StrafeInput = 0.0f;
	float SmoothedStrafeInput = 0.0f;
	float SmoothedMouseX = 0.0f;
	float SmoothedMouseY = 0.0f;
	float TargetPitch = 0.0f;
	float TargetYaw = 0.0f;
	float BoostAlpha = 0.0f;
	bool bBoostRequested = false;
	FVector CurrentCameraSocketOffset = FVector(0.0f, 0.0f, 180.0f);
};
