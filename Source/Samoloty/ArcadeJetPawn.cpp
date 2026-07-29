#include "ArcadeJetPawn.h"

#include "ArcadeFlightComponent.h"
#include "HealthComponent.h"
#include "JetBoostComponent.h"
#include "JetEngineAudioComponent.h"
#include "JetStatsComponent.h"
#include "RocketWeaponComponent.h"
#include "RifleGunComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

AArcadeJetPawn::AArcadeJetPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(true);
	// FlightComponent replicates one authoritative server transform to every client.
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(15.0f);
	SetNetCullDistanceSquared(FMath::Square(500000.0f)); // 5 km in Unreal units.

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(240.0f, 110.0f, 60.0f));
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Collision;

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(Collision);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	JetStats = CreateDefaultSubobject<UJetStatsComponent>(TEXT("JetStats"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	JetBoost = CreateDefaultSubobject<UJetBoostComponent>(TEXT("JetBoost"));
	JetEngineAudio = CreateDefaultSubobject<UJetEngineAudioComponent>(TEXT("JetEngineAudio"));
	FlightMovement = CreateDefaultSubobject<UArcadeFlightComponent>(TEXT("FlightMovement"));
	RocketWeapon = CreateDefaultSubobject<URocketWeaponComponent>(TEXT("RocketWeapon"));
	RifleGun = CreateDefaultSubobject<URifleGunComponent>(TEXT("RifleGun"));

	RocketSpawnLeft = CreateDefaultSubobject<USceneComponent>(TEXT("RocketSpawnLeft"));
	RocketSpawnLeft->SetupAttachment(Collision);
	RocketSpawnLeft->SetRelativeLocation(FVector(100.0f, -250.0f, 0.0f));
	RocketSpawnRight = CreateDefaultSubobject<USceneComponent>(TEXT("RocketSpawnRight"));
	RocketSpawnRight->SetupAttachment(Collision);
	RocketSpawnRight->SetRelativeLocation(FVector(100.0f, 250.0f, 0.0f));

	GunMuzzleLeft = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleLeft"));
	GunMuzzleLeft->SetupAttachment(Collision);
	GunMuzzleLeft->SetRelativeLocation(FVector(200.0f, -150.0f, 0.0f));
	GunMuzzleRight = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleRight"));
	GunMuzzleRight->SetupAttachment(Collision);
	GunMuzzleRight->SetRelativeLocation(FVector(200.0f, 150.0f, 0.0f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Collision);
	CameraBoom->TargetArmLength = NormalCameraDistance;
	CameraBoom->SocketOffset = CurrentCameraSocketOffset;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->FieldOfView = NormalFieldOfView;
}

void AArcadeJetPawn::BeginPlay()
{
	Super::BeginPlay();
	JetStats->RecalculateStats();
	RocketWeapon->SetSpawnPoints(RocketSpawnLeft, RocketSpawnRight);
	RifleGun->SetMuzzlePoints(GunMuzzleLeft, GunMuzzleRight);
	RifleGun->SetCameraAimEnabled(true);
	CurrentHoverCameraDistance = FMath::Clamp(
		HoverCameraDistance, MinHoverCameraDistance, MaxHoverCameraDistance);
	CurrentCameraWorldRotation = GetActorRotation();

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
	PlayerInputComponent->BindAxis(TEXT("PlaneBrake"), this, &AArcadeJetPawn::SetBrake);
	PlayerInputComponent->BindAxis(TEXT("PlaneHoverZoom"), this, &AArcadeJetPawn::SetHoverCameraZoom);
	PlayerInputComponent->BindAxis(TEXT("PlaneMouseX"), this, &AArcadeJetPawn::AddHoverCameraYawInput);
	PlayerInputComponent->BindAxis(TEXT("PlaneMouseY"), this, &AArcadeJetPawn::AddHoverCameraPitchInput);
	PlayerInputComponent->BindAction(TEXT("PlaneBoost"), IE_Pressed, this, &AArcadeJetPawn::StartBoost);
	PlayerInputComponent->BindAction(TEXT("PlaneBoost"), IE_Released, this, &AArcadeJetPawn::StopBoost);
	PlayerInputComponent->BindAction(TEXT("PlaneHover"), IE_Pressed, this, &AArcadeJetPawn::ToggleHover);
	PlayerInputComponent->BindAction(TEXT("PlaneRocketFire"), IE_Pressed, this, &AArcadeJetPawn::StartRocketFire);
	PlayerInputComponent->BindAction(TEXT("PlaneRocketFire"), IE_Released, this, &AArcadeJetPawn::StopRocketFire);
	PlayerInputComponent->BindAction(TEXT("PlaneRifleFire"), IE_Pressed, this, &AArcadeJetPawn::StartRifleFire);
	PlayerInputComponent->BindAction(TEXT("PlaneRifleFire"), IE_Released, this, &AArcadeJetPawn::StopRifleFire);
}

void AArcadeJetPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsLocallyControlled())
	{
		if (bHoverCameraOrbitHeld && FlightMovement->GetHoverState() != EArcadeHoverState::Hovering)
		{
			EndHoverCameraOrbit();
		}
		UpdateCursorInput();
		FlightMovement->SetLocalFlightInput(CursorSteering, StrafeInput, BrakeInput);
		UpdateLocalCamera(DeltaSeconds);
		RifleGun->SetCameraAim(FollowCamera->GetComponentLocation(), FollowCamera->GetForwardVector());
	}
}

void AArcadeJetPawn::SetStrafe(const float Value)
{
	StrafeInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AArcadeJetPawn::SetBrake(const float Value)
{
	BrakeInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void AArcadeJetPawn::StartBoost()
{
	EndHoverCameraOrbit();
	FlightMovement->RequestExitHover();
	JetBoost->SetBoostRequested(true);
}

void AArcadeJetPawn::StopBoost()
{
	JetBoost->SetBoostRequested(false);
}

void AArcadeJetPawn::ToggleHover()
{
	EndHoverCameraOrbit();
	HoverCameraOrbitYaw = 0.0f;
	HoverCameraOrbitPitch = 0.0f;
	FlightMovement->ToggleHover();
}

void AArcadeJetPawn::AddHoverCameraYawInput(const float Value)
{
	if (bHoverCameraOrbitHeld)
	{
		HoverCameraOrbitYaw = FMath::UnwindDegrees(
			HoverCameraOrbitYaw + Value * HoverCameraMouseSensitivity);
	}
}

void AArcadeJetPawn::AddHoverCameraPitchInput(const float Value)
{
	if (bHoverCameraOrbitHeld)
	{
		HoverCameraOrbitPitch = FMath::Clamp(
			HoverCameraOrbitPitch + Value * HoverCameraMouseSensitivity,
			-MaxHoverCameraPitch, MaxHoverCameraPitch);
	}
}

void AArcadeJetPawn::BeginHoverCameraOrbit()
{
	if (bHoverCameraOrbitHeld)
	{
		return;
	}
	bHoverCameraOrbitHeld = true;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		bHasSavedCursorPosition = PlayerController->GetMousePosition(MouseX, MouseY);
		SavedCursorPosition = FVector2D(MouseX, MouseY);
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void AArcadeJetPawn::EndHoverCameraOrbit()
{
	if (!bHoverCameraOrbitHeld)
	{
		return;
	}
	bHoverCameraOrbitHeld = false;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PlayerController->SetInputMode(InputMode);
		if (bHasSavedCursorPosition)
		{
			PlayerController->SetMouseLocation(
				FMath::RoundToInt(SavedCursorPosition.X), FMath::RoundToInt(SavedCursorPosition.Y));
		}
	}
	bHasSavedCursorPosition = false;
}

void AArcadeJetPawn::SetHoverCameraZoom(const float Value)
{
	if (FMath::Abs(Value) <= KINDA_SMALL_NUMBER
		|| FlightMovement->GetHoverState() == EArcadeHoverState::Flying)
	{
		return;
	}
	CurrentHoverCameraDistance = FMath::Clamp(
		CurrentHoverCameraDistance - Value * HoverCameraZoomStep,
		MinHoverCameraDistance, MaxHoverCameraDistance);
}

void AArcadeJetPawn::StartRocketFire()
{
	if (FlightMovement->GetHoverState() == EArcadeHoverState::Hovering)
	{
		BeginHoverCameraOrbit();
		return;
	}
	RocketWeapon->SetFireHeld(true);
}

void AArcadeJetPawn::StopRocketFire()
{
	EndHoverCameraOrbit();
	RocketWeapon->SetFireHeld(false);
}

void AArcadeJetPawn::StartRifleFire()
{
	RifleGun->SetFireHeld(true);
}

void AArcadeJetPawn::StopRifleFire()
{
	RifleGun->SetFireHeld(false);
}

void AArcadeJetPawn::UpdateCursorInput()
{
	float DesiredX = 0.0f;
	float DesiredY = 0.0f;
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		int32 Width = 0;
		int32 Height = 0;
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		PlayerController->GetViewportSize(Width, Height);
		if (Width > 0 && Height > 0 && PlayerController->GetMousePosition(MouseX, MouseY))
		{
			DesiredX = FMath::Clamp((MouseX / static_cast<float>(Width) - 0.5f) * 2.0f, -1.0f, 1.0f);
			DesiredY = FMath::Clamp((0.5f - MouseY / static_cast<float>(Height)) * 2.0f, -1.0f, 1.0f);
		}
	}

	auto ApplyCurve = [this](const float Value)
	{
		const float Absolute = FMath::Abs(Value);
		if (Absolute <= CursorDeadZone)
		{
			return 0.0f;
		}
		const float Remapped = (Absolute - CursorDeadZone) / (1.0f - CursorDeadZone);
		return FMath::Sign(Value) * FMath::Pow(Remapped, CursorResponseExponent);
	};

	CursorSteering.X = ApplyCurve(DesiredX);
	CursorSteering.Y = ApplyCurve(DesiredY) * (bInvertMouseY ? 1.0f : -1.0f);
}

void AArcadeJetPawn::UpdateLocalCamera(const float DeltaSeconds)
{
	const FVector2D Steering = FlightMovement->GetSmoothedSteering();
	const float Strafe = FlightMovement->GetSmoothedStrafe();
	const float BoostAlpha = JetBoost->GetBoostAlpha();
	const float HoverAlpha = FlightMovement->GetHoverAlpha();
	const FVector DesiredOffset(
		0.0f,
		(Steering.X * MaxCameraSideShift - Strafe * StrafeCameraSideShift) * (1.0f - HoverAlpha),
		FMath::Lerp(180.0f + Steering.Y * MaxCameraVerticalShift, HoverCameraHeight, HoverAlpha));
	CurrentCameraSocketOffset = FMath::VInterpTo(
		CurrentCameraSocketOffset, DesiredOffset, DeltaSeconds, CameraFramingSpeed);
	CameraBoom->SocketOffset = CurrentCameraSocketOffset;
	const float FlightCameraDistance = FMath::Lerp(NormalCameraDistance, BoostCameraDistance, BoostAlpha);
	CameraBoom->TargetArmLength = FMath::Lerp(FlightCameraDistance, CurrentHoverCameraDistance, HoverAlpha);
	const FRotator ActorViewRotation(GetActorRotation().Pitch, GetActorRotation().Yaw, 0.0f);
	const FRotator HoverViewRotation(HoverCameraOrbitPitch,
		GetActorRotation().Yaw + HoverCameraOrbitYaw, 0.0f);
	const FQuat DesiredCameraRotation = FQuat::Slerp(
		ActorViewRotation.Quaternion(), HoverViewRotation.Quaternion(), HoverAlpha).GetNormalized();
	CurrentCameraWorldRotation = FMath::RInterpTo(CurrentCameraWorldRotation,
		DesiredCameraRotation.Rotator(), DeltaSeconds, CameraRotationFollowSpeed);
	CameraBoom->SetWorldRotation(CurrentCameraWorldRotation);
	FollowCamera->SetFieldOfView(FMath::Lerp(NormalFieldOfView, BoostFieldOfView, BoostAlpha));
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
}
