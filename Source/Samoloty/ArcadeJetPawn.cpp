#include "ArcadeJetPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

AArcadeJetPawn::AArcadeJetPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	Collision->InitCapsuleSize(110.0f, 240.0f);
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Collision;

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(Collision);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Collision);
	CameraBoom->TargetArmLength = 1150.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 180.0f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
	CameraBoom->CameraRotationLagSpeed = 7.0f;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->FieldOfView = 90.0f;
}

void AArcadeJetPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PlayerController->SetInputMode(InputMode);

		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		PlayerController->SetMouseLocation(ViewportWidth / 2, ViewportHeight / 2);
	}
}

void AArcadeJetPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("PlaneStrafe"), this, &AArcadeJetPawn::SetStrafe);
	PlayerInputComponent->BindAction(TEXT("PlaneBoost"), IE_Pressed, this, &AArcadeJetPawn::StartBoost);
	PlayerInputComponent->BindAction(TEXT("PlaneBoost"), IE_Released, this, &AArcadeJetPawn::StopBoost);
}

void AArcadeJetPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SmoothedStrafeInput = FMath::FInterpTo(
		SmoothedStrafeInput, StrafeInput, DeltaSeconds, StrafeResponseSpeed);
	BoostAlpha = FMath::FInterpTo(
		BoostAlpha, bBoostRequested ? 1.0f : 0.0f, DeltaSeconds, BoostResponseSpeed);
	RotateAircraft(DeltaSeconds);
	MoveAircraft(DeltaSeconds);

	// Keep the camera aligned behind the aircraft, but shift the framing slightly.
	// This gives screen-space movement without ever showing the aircraft side-on.
	const FVector DesiredSocketOffset(
		0.0f,
		FMath::Clamp(SmoothedMouseX, -1.0f, 1.0f) * MaxCameraSideShift - SmoothedStrafeInput * StrafeCameraSideShift,
		180.0f + FMath::Clamp(SmoothedMouseY, -1.0f, 1.0f) * MaxCameraVerticalShift);
	CurrentCameraSocketOffset = FMath::VInterpTo(
		CurrentCameraSocketOffset, DesiredSocketOffset, DeltaSeconds, CameraFramingSpeed);
	CameraBoom->SocketOffset = CurrentCameraSocketOffset;
	CameraBoom->TargetArmLength = FMath::Lerp(NormalCameraDistance, BoostCameraDistance, BoostAlpha);
	FollowCamera->SetFieldOfView(FMath::Lerp(NormalFieldOfView, BoostFieldOfView, BoostAlpha));
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
}

void AArcadeJetPawn::SetStrafe(const float Value) { StrafeInput = Value; }

void AArcadeJetPawn::StartBoost()
{
	if (!bBoostRequested)
	{
		bBoostRequested = true;
		OnBoostStateChanged(true);
	}
}

void AArcadeJetPawn::StopBoost()
{
	if (bBoostRequested)
	{
		bBoostRequested = false;
		OnBoostStateChanged(false);
	}
}

void AArcadeJetPawn::UpdateCursorSteering(const float DeltaSeconds)
{
	float DesiredSteeringX = 0.0f;
	float DesiredSteeringY = 0.0f;

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);

		if (ViewportWidth > 0 && ViewportHeight > 0 && PlayerController->GetMousePosition(MouseX, MouseY))
		{
			DesiredSteeringX = FMath::Clamp((MouseX / static_cast<float>(ViewportWidth) - 0.5f) * 2.0f, -1.0f, 1.0f);
			// Screen Y grows downwards, so cursor above centre must produce positive pitch.
			DesiredSteeringY = FMath::Clamp((0.5f - MouseY / static_cast<float>(ViewportHeight)) * 2.0f, -1.0f, 1.0f);
		}
	}

	auto ApplyResponseCurve = [this](const float Value)
	{
		const float AbsoluteValue = FMath::Abs(Value);
		if (AbsoluteValue <= CursorDeadZone)
		{
			return 0.0f;
		}
		const float Remapped = (AbsoluteValue - CursorDeadZone) / (1.0f - CursorDeadZone);
		return FMath::Sign(Value) * FMath::Pow(Remapped, CursorResponseExponent);
	};

	DesiredSteeringX = ApplyResponseCurve(DesiredSteeringX);
	DesiredSteeringY = ApplyResponseCurve(DesiredSteeringY);
	SmoothedMouseX = FMath::FInterpTo(SmoothedMouseX, DesiredSteeringX, DeltaSeconds, InputSmoothingSpeed);
	SmoothedMouseY = FMath::FInterpTo(SmoothedMouseY, DesiredSteeringY, DeltaSeconds, InputSmoothingSpeed);
}

void AArcadeJetPawn::RotateAircraft(const float DeltaSeconds)
{
	UpdateCursorSteering(DeltaSeconds);
	const FRotator CurrentRotation = GetActorRotation();
	TargetYaw = CurrentRotation.Yaw + SmoothedMouseX * MaxYawTurnRate * DeltaSeconds;
	const float PitchDirection = bInvertMouseY ? 1.0f : -1.0f;
	TargetPitch = FMath::Clamp(
		CurrentRotation.Pitch + SmoothedMouseY * PitchDirection * MaxPitchTurnRate * DeltaSeconds,
		-MaxPitch, MaxPitch);

	const float VisualBank = -SmoothedMouseX * MaxVisualBank;
	const float StrafeBank = -SmoothedStrafeInput * MaxStrafeBank;
	// A/D progressively overrides mouse banking. Opposing inputs can no longer
	// cancel the strafe lean; at full strafe MaxStrafeBank is always reached.
	const float StrafePriority = FMath::Clamp(FMath::Abs(SmoothedStrafeInput), 0.0f, 1.0f);
	const float TargetRoll = FMath::Lerp(VisualBank, StrafeBank, StrafePriority);
	const float SmoothedRoll = FMath::FInterpTo(CurrentRotation.Roll, TargetRoll, DeltaSeconds, RotationResponsiveness);
	SetActorRotation(FRotator(TargetPitch, TargetYaw, SmoothedRoll));
}

void AArcadeJetPawn::MoveAircraft(const float DeltaSeconds)
{
	const float CurrentForwardSpeed = ForwardSpeed * FMath::Lerp(1.0f, BoostSpeedMultiplier, BoostAlpha);
	const FVector Velocity = GetActorForwardVector() * CurrentForwardSpeed + GetActorRightVector() * SmoothedStrafeInput * StrafeSpeed;
	FHitResult Hit;
	AddActorWorldOffset(Velocity * DeltaSeconds, true, &Hit);

	if (Hit.IsValidBlockingHit())
	{
		const FVector RemainingMove = Velocity * DeltaSeconds * (1.0f - Hit.Time);
		AddActorWorldOffset(FVector::VectorPlaneProject(RemainingMove, Hit.Normal), true);
	}
}
