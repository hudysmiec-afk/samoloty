#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ArcadeJetPawn.generated.h"

class UArcadeFlightComponent;
class UCameraComponent;
class UBoxComponent;
class UEvasiveRollComponent;
class UHealthComponent;
class UJetBoostComponent;
class UJetEngineAudioComponent;
class UJetStatsComponent;
class UHomingMissileWeaponComponent;
class UMissileTargetingComponent;
class URadarComponent;
class UQuickReversalComponent;
class URocketWeaponComponent;
class URifleGunComponent;
class UShotgunGunComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UTargetSelectionComponent;
class UWeaponSystemComponent;

/** Input, camera and presentation shell for the network-ready flight components. */
UCLASS(Blueprintable)
class SAMOLOTY_API AArcadeJetPawn : public APawn
{
	GENERATED_BODY()

public:
	AArcadeJetPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Applies render-only roll to the complete visible aircraft assembly. */
	void SetVisualBank(float BankDegrees);

	/** Temporarily replaces normal visual bank with the evasive roll orientation. */
	void SetEvasiveRollPresentation(bool bActive, float RollDegrees);

	/** Moves the logical plane while sweeping its box hitbox and returns the applied delta. */
	FVector MovePlaneWithCollision(const FVector& RequestedMove);

	/** Called by the flight component after movement/interpolation for deterministic camera order. */
	void UpdateCameraAfterFlight(float DeltaSeconds);

protected:
	virtual void BeginPlay() override;

	/** Invisible transform representing the actual flight path and camera direction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<USceneComponent> VirtualFlightRoot;

	/** Axis parallel to the flight direction but positioned above the aircraft. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<USceneComponent> EvasiveRollPivot;

	/** Visible aircraft assembly. Mesh, damage hitbox and weapon origins move together. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UStaticMeshComponent> PlaneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UJetStatsComponent> JetStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UJetBoostComponent> JetBoost;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UJetEngineAudioComponent> JetEngineAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UEvasiveRollComponent> EvasiveRoll;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UQuickReversalComponent> QuickReversal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<URocketWeaponComponent> RocketWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UMissileTargetingComponent> MissileTargeting;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<URadarComponent> Radar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UTargetSelectionComponent> TargetSelection;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UHomingMissileWeaponComponent> HomingMissileWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<URifleGunComponent> RifleGun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UShotgunGunComponent> ShotgunGun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UWeaponSystemComponent> WeaponSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	TObjectPtr<USceneComponent> RocketSpawnLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	TObjectPtr<USceneComponent> RocketSpawnRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	TObjectPtr<USceneComponent> GunMuzzleLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Weapons")
	TObjectPtr<USceneComponent> GunMuzzleRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UArcadeFlightComponent> FlightMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plane|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Input")
	bool bInvertMouseY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover", meta=(ClampMin="0"))
	float HoverCameraDistance = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover")
	float HoverCameraHeight = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover", meta=(ClampMin="0", ClampMax="89"))
	float MaxHoverCameraPitch = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover", meta=(ClampMin="0"))
	float MinHoverCameraDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover", meta=(ClampMin="0"))
	float MaxHoverCameraDistance = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover", meta=(ClampMin="1"))
	float HoverCameraZoomStep = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Camera|Hover", meta=(ClampMin="0.01"))
	float HoverCameraMouseSensitivity = 3.0f;

private:
	void SetStrafe(float Value);
	void RegisterLeftRollTap();
	void RegisterRightRollTap();
	void ActivateQuickReversal();
	void SetBrake(float Value);
	void StartBoost();
	void StopBoost();
	void ToggleHover();
	void SetHoverCameraZoom(float Value);
	void AddHoverCameraYawInput(float Value);
	void AddHoverCameraPitchInput(float Value);
	void BeginHoverCameraOrbit();
	void EndHoverCameraOrbit();
	void StartMissileFire();
	void StopMissileFire();
	void StartGunFire();
	void StopGunFire();
	void CycleGunWeapon();
	void CycleMissileWeapon();
	void RejectMissileTarget();
	void UpdateCursorInput();
	void ApplyVisualRotation();

	FVector2D CursorSteering = FVector2D::ZeroVector;
	float StrafeInput = 0.0f;
	float BrakeInput = 0.0f;
	float CurrentVisualBankDegrees = 0.0f;
	bool bHoverCameraOrbitHeld = false;
	float HoverCameraOrbitYaw = 0.0f;
	float HoverCameraOrbitPitch = 0.0f;
	float CurrentHoverCameraDistance = 1400.0f;
	FVector CurrentCameraWorldPosition = FVector::ZeroVector;
	FRotator CurrentCameraWorldRotation = FRotator::ZeroRotator;
	FVector QuickReversalCameraStartOffset = FVector::ZeroVector;
	FVector QuickReversalCameraStartPlaneLocation = FVector::ZeroVector;
	FVector QuickReversalCameraStartViewDirection = FVector::ForwardVector;
	FVector QuickReversalCameraStartUp = FVector::UpVector;
	bool bQuickReversalCameraPathActive = false;
	FVector2D SavedCursorPosition = FVector2D::ZeroVector;
	bool bHasSavedCursorPosition = false;
};
