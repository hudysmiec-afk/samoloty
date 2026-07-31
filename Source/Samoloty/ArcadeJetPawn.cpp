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
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

namespace ArcadeJetCameraTuning
{
	// The prototype flight camera deliberately stays code-only so old Blueprint values
	// cannot silently override the behavior while the flight model is established.
	constexpr float CursorDeadZone = 0.01f;
	constexpr float FlightDistance = 1500.0f;
	constexpr float FlightHeight = 80.0f;
	constexpr float LookAheadDistance = 12000.0f;
	constexpr float NormalFollowResponse = 3.0f;
	constexpr float BoostFollowResponse = 4.0f;
	constexpr float HoverFollowResponse = 8.0f;
	constexpr float TeleportSnapDistance = 12000.0f;
}

AArcadeJetPawn::AArcadeJetPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(true);
	// FlightComponent replicates one authoritative server transform to every client.
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(15.0f);
	SetNetCullDistanceSquared(FMath::Square(500000.0f)); // 5 km in Unreal units.

	VirtualFlightRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VirtualFlightRoot"));
	RootComponent = VirtualFlightRoot;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(VirtualFlightRoot);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(240.0f, 110.0f, 60.0f));
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->SetupAttachment(VisualRoot);

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(VisualRoot);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	JetStats = CreateDefaultSubobject<UJetStatsComponent>(TEXT("JetStats"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	JetBoost = CreateDefaultSubobject<UJetBoostComponent>(TEXT("JetBoost"));
	JetEngineAudio = CreateDefaultSubobject<UJetEngineAudioComponent>(TEXT("JetEngineAudio"));
	FlightMovement = CreateDefaultSubobject<UArcadeFlightComponent>(TEXT("FlightMovement"));
	RocketWeapon = CreateDefaultSubobject<URocketWeaponComponent>(TEXT("RocketWeapon"));
	RifleGun = CreateDefaultSubobject<URifleGunComponent>(TEXT("RifleGun"));

	RocketSpawnLeft = CreateDefaultSubobject<USceneComponent>(TEXT("RocketSpawnLeft"));
	RocketSpawnLeft->SetupAttachment(VisualRoot);
	RocketSpawnLeft->SetRelativeLocation(FVector(100.0f, -250.0f, 0.0f));
	RocketSpawnRight = CreateDefaultSubobject<USceneComponent>(TEXT("RocketSpawnRight"));
	RocketSpawnRight->SetupAttachment(VisualRoot);
	RocketSpawnRight->SetRelativeLocation(FVector(100.0f, 250.0f, 0.0f));

	GunMuzzleLeft = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleLeft"));
	GunMuzzleLeft->SetupAttachment(VisualRoot);
	GunMuzzleLeft->SetRelativeLocation(FVector(200.0f, -150.0f, 0.0f));
	GunMuzzleRight = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleRight"));
	GunMuzzleRight->SetupAttachment(VisualRoot);
	GunMuzzleRight->SetRelativeLocation(FVector(200.0f, 150.0f, 0.0f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VirtualFlightRoot);
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->SetAbsolute(true, true, false);
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bDoCollisionTest = false;

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
	VisualRoot->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	CameraBoom->SetAbsolute(true, true, false);
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bDoCollisionTest = false;
	CurrentHoverCameraDistance = FMath::Clamp(
		HoverCameraDistance, MinHoverCameraDistance, MaxHoverCameraDistance);
	CurrentCameraWorldPosition = GetActorLocation()
		- GetActorForwardVector() * ArcadeJetCameraTuning::FlightDistance
		+ GetActorUpVector() * ArcadeJetCameraTuning::FlightHeight;
	CurrentCameraWorldRotation = GetActorRotation();
	bFlightCameraInitialized = true;

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

void AArcadeJetPawn::SetVisualBank(const float BankDegrees)
{
	if (!VisualRoot)
	{
		return;
	}
	VisualRoot->SetRelativeLocation(FVector::ZeroVector);
	VisualRoot->SetRelativeRotation(FRotator(0.0f, 0.0f, BankDegrees));
}

FVector AArcadeJetPawn::MovePlaneWithCollision(const FVector& RequestedMove)
{
	if (RequestedMove.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	if (!Collision || !GetWorld())
	{
		AddActorWorldOffset(RequestedMove, false, nullptr, ETeleportType::None);
		return RequestedMove;
	}

	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeBox(Collision->GetScaledBoxExtent());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlaneMovementCollision), false, this);
	QueryParams.AddIgnoredActor(this);
	const FVector SweepStart = Collision->GetComponentLocation();
	const bool bHit = GetWorld()->SweepSingleByProfile(
		Hit,
		SweepStart,
		SweepStart + RequestedMove,
		Collision->GetComponentQuat(),
		Collision->GetCollisionProfileName(),
		Shape,
		QueryParams);

	const float MoveAlpha = bHit
		? FMath::Clamp(Hit.Time - 0.001f, 0.0f, 1.0f)
		: 1.0f;
	const FVector AppliedMove = RequestedMove * MoveAlpha;
	AddActorWorldOffset(AppliedMove, false, nullptr, ETeleportType::None);
	Collision->UpdateOverlaps();
	return AppliedMove;
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

	auto ApplyDeadZone = [](const float Value)
	{
		return FMath::Abs(Value) <= ArcadeJetCameraTuning::CursorDeadZone ? 0.0f : Value;
	};

	// Cursor displacement is mapped linearly against each viewport half-axis. Using
	// width and height independently gives equal normalized authority on 16:9 screens.
	CursorSteering.X = ApplyDeadZone(DesiredX);
	CursorSteering.Y = ApplyDeadZone(DesiredY) * (bInvertMouseY ? 1.0f : -1.0f);
}

void AArcadeJetPawn::UpdateLocalCamera(const float DeltaSeconds)
{
	const float BoostAlpha = JetBoost->GetBoostAlpha();
	const float HoverAlpha = FlightMovement->GetHoverAlpha();
	const FVector PlaneLocation = GetActorLocation();
	const FVector FlightForward = GetActorForwardVector();
	const FVector FlightUp = GetActorUpVector();
	const FVector DesiredFlightEye = PlaneLocation
		- FlightForward * ArcadeJetCameraTuning::FlightDistance
		+ FlightUp * ArcadeJetCameraTuning::FlightHeight;
	const FVector FlightLookTarget = PlaneLocation
		+ FlightForward * ArcadeJetCameraTuning::LookAheadDistance;

	const FRotator HoverOrbitRotation(
		HoverCameraOrbitPitch,
		GetActorRotation().Yaw + HoverCameraOrbitYaw,
		0.0f);
	const FVector DesiredHoverEye = PlaneLocation
		- HoverOrbitRotation.Vector() * CurrentHoverCameraDistance
		+ FVector::UpVector * HoverCameraHeight;
	const FVector DesiredEye = FMath::Lerp(DesiredFlightEye, DesiredHoverEye, HoverAlpha);

	const float FlightFollowResponse = FMath::Lerp(
		ArcadeJetCameraTuning::NormalFollowResponse,
		ArcadeJetCameraTuning::BoostFollowResponse,
		BoostAlpha);
	const float FollowResponse = FMath::Lerp(
		FlightFollowResponse,
		ArcadeJetCameraTuning::HoverFollowResponse,
		HoverAlpha);
	if (!bFlightCameraInitialized
		|| FVector::DistSquared(CurrentCameraWorldPosition, DesiredEye)
			> FMath::Square(ArcadeJetCameraTuning::TeleportSnapDistance))
	{
		CurrentCameraWorldPosition = DesiredEye;
		bFlightCameraInitialized = true;
	}
	else
	{
		CurrentCameraWorldPosition = FMath::VInterpTo(
			CurrentCameraWorldPosition, DesiredEye, DeltaSeconds, FollowResponse);
	}

	// The eye position catches up smoothly, while the target follows the logical
	// flight direction immediately. This creates the intended chase-camera lag.
	const FVector LookTarget = FMath::Lerp(FlightLookTarget, PlaneLocation, HoverAlpha);
	const FVector ViewDirection = LookTarget - CurrentCameraWorldPosition;
	const FVector CameraUp = FMath::Lerp(FlightUp, FVector::UpVector, HoverAlpha).GetSafeNormal();
	if (!ViewDirection.IsNearlyZero())
	{
		CurrentCameraWorldRotation = FRotationMatrix::MakeFromXZ(
			ViewDirection.GetSafeNormal(), CameraUp).Rotator();
	}
	CameraBoom->SetWorldLocationAndRotation(
		CurrentCameraWorldPosition, CurrentCameraWorldRotation);
	FollowCamera->SetFieldOfView(FMath::Lerp(NormalFieldOfView, BoostFieldOfView, BoostAlpha));
}
