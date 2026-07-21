#include "ArcadeJetPawn.h"

#include "ArcadeFlightComponent.h"
#include "JetBoostComponent.h"
#include "JetStatsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
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

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	Collision->InitCapsuleSize(110.0f, 240.0f);
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Collision;

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(Collision);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	JetStats = CreateDefaultSubobject<UJetStatsComponent>(TEXT("JetStats"));
	JetBoost = CreateDefaultSubobject<UJetBoostComponent>(TEXT("JetBoost"));
	FlightMovement = CreateDefaultSubobject<UArcadeFlightComponent>(TEXT("FlightMovement"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Collision);
	CameraBoom->TargetArmLength = NormalCameraDistance;
	CameraBoom->SocketOffset = CurrentCameraSocketOffset;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
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
	if (IsLocallyControlled())
	{
		UpdateCursorInput();
		FlightMovement->SetLocalFlightInput(CursorSteering, StrafeInput);
		UpdateLocalCamera(DeltaSeconds);
	}
}

void AArcadeJetPawn::SetStrafe(const float Value)
{
	StrafeInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AArcadeJetPawn::StartBoost()
{
	JetBoost->SetBoostRequested(true);
}

void AArcadeJetPawn::StopBoost()
{
	JetBoost->SetBoostRequested(false);
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
	const FVector DesiredOffset(
		0.0f,
		Steering.X * MaxCameraSideShift - Strafe * StrafeCameraSideShift,
		180.0f + Steering.Y * MaxCameraVerticalShift);
	CurrentCameraSocketOffset = FMath::VInterpTo(
		CurrentCameraSocketOffset, DesiredOffset, DeltaSeconds, CameraFramingSpeed);
	CameraBoom->SocketOffset = CurrentCameraSocketOffset;
	CameraBoom->TargetArmLength = FMath::Lerp(NormalCameraDistance, BoostCameraDistance, BoostAlpha);
	FollowCamera->SetFieldOfView(FMath::Lerp(NormalFieldOfView, BoostFieldOfView, BoostAlpha));
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
}
