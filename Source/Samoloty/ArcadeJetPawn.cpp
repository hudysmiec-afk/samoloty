#include "ArcadeJetPawn.h"

#include "ArcadeFlightComponent.h"
#include "EvasiveRollComponent.h"
#include "HealthComponent.h"
#include "HomingMissileWeaponComponent.h"
#include "JetBoostComponent.h"
#include "JetEngineAudioComponent.h"
#include "JetStatsComponent.h"
#include "MissileTargetingComponent.h"
#include "QuickReversalComponent.h"
#include "RadarComponent.h"
#include "RocketWeaponComponent.h"
#include "RifleGunComponent.h"
#include "ShotgunGunComponent.h"
#include "TargetSelectionComponent.h"
#include "WeaponSystemComponent.h"
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
	constexpr float FlightHeight = 360.0f;
	constexpr float LookAheadDistance = 10000.0f;
	constexpr float FieldOfView = 105.0f;
	constexpr float NormalFollowResponse = 3.0f;
	constexpr float BoostFollowResponse = 4.0f;
	constexpr float HoverFollowResponse = 8.0f;
	constexpr float TeleportSnapDistance = 12000.0f;
	constexpr float ReversalCameraDelayFraction = 0.55f;
	constexpr float ReversalCameraRecoveryStart = 0.82f;
	constexpr float ReversalCameraLookAcquireEnd = 0.28f;
	constexpr float ReversalCameraPullBack = 450.0f;
	constexpr float ReversalCameraLift = 170.0f;
}

namespace ArcadeJetPresentationTuning
{
	// The roll axis sits slightly above the aircraft rather than passing through
	// the mesh center. A small radius keeps the maneuver readable without throwing
	// the plane several meters across the screen.
	constexpr float EvasiveRollPivotHeight = 200.0f;
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

	EvasiveRollPivot = CreateDefaultSubobject<USceneComponent>(TEXT("EvasiveRollPivot"));
	EvasiveRollPivot->SetupAttachment(VirtualFlightRoot);
	EvasiveRollPivot->SetRelativeLocation(
		FVector(0.0f, 0.0f, ArcadeJetPresentationTuning::EvasiveRollPivotHeight));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(EvasiveRollPivot);
	VisualRoot->SetRelativeLocation(
		FVector(0.0f, 0.0f, -ArcadeJetPresentationTuning::EvasiveRollPivotHeight));

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(240.0f, 110.0f, 60.0f));
	Collision->SetCollisionProfileName(FName(TEXT("PlanePawn")));
	Collision->SetupAttachment(VisualRoot);

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(VisualRoot);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	JetStats = CreateDefaultSubobject<UJetStatsComponent>(TEXT("JetStats"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	JetBoost = CreateDefaultSubobject<UJetBoostComponent>(TEXT("JetBoost"));
	JetEngineAudio = CreateDefaultSubobject<UJetEngineAudioComponent>(TEXT("JetEngineAudio"));
	EvasiveRoll = CreateDefaultSubobject<UEvasiveRollComponent>(TEXT("EvasiveRoll"));
	QuickReversal = CreateDefaultSubobject<UQuickReversalComponent>(TEXT("QuickReversal"));
	FlightMovement = CreateDefaultSubobject<UArcadeFlightComponent>(TEXT("FlightMovement"));
	RocketWeapon = CreateDefaultSubobject<URocketWeaponComponent>(TEXT("RocketWeapon"));
	Radar = CreateDefaultSubobject<URadarComponent>(TEXT("Radar"));
	TargetSelection = CreateDefaultSubobject<UTargetSelectionComponent>(TEXT("TargetSelection"));
	MissileTargeting = CreateDefaultSubobject<UMissileTargetingComponent>(TEXT("MissileTargeting"));
	HomingMissileWeapon = CreateDefaultSubobject<UHomingMissileWeaponComponent>(TEXT("RocketWeaponHoming"));
	RifleGun = CreateDefaultSubobject<URifleGunComponent>(TEXT("RifleGun"));
	ShotgunGun = CreateDefaultSubobject<UShotgunGunComponent>(TEXT("ShotgunGun"));
	WeaponSystem = CreateDefaultSubobject<UWeaponSystemComponent>(TEXT("WeaponSystem"));

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
	FollowCamera->FieldOfView = ArcadeJetCameraTuning::FieldOfView;
}

void AArcadeJetPawn::BeginPlay()
{
	Super::BeginPlay();
	WeaponSystem->ConfigureFirePoints(EWeaponSlot::Gun, GunMuzzleLeft, GunMuzzleRight);
	WeaponSystem->ConfigureFirePoints(EWeaponSlot::Missile, RocketSpawnLeft, RocketSpawnRight);
	EvasiveRollPivot->SetRelativeLocationAndRotation(
		FVector(0.0f, 0.0f, ArcadeJetPresentationTuning::EvasiveRollPivotHeight),
		FRotator::ZeroRotator);
	VisualRoot->SetRelativeLocationAndRotation(
		FVector(0.0f, 0.0f, -ArcadeJetPresentationTuning::EvasiveRollPivotHeight),
		FRotator::ZeroRotator);
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
	FollowCamera->SetFieldOfView(ArcadeJetCameraTuning::FieldOfView);
	CurrentHoverCameraDistance = FMath::Clamp(
		HoverCameraDistance, MinHoverCameraDistance, MaxHoverCameraDistance);
	CurrentCameraWorldPosition = GetActorLocation()
		- GetActorForwardVector() * ArcadeJetCameraTuning::FlightDistance
		+ GetActorUpVector() * ArcadeJetCameraTuning::FlightHeight;
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

void AArcadeJetPawn::SetVisualBank(const float BankDegrees)
{
	CurrentVisualBankDegrees = BankDegrees;
	ApplyVisualRotation();
}

void AArcadeJetPawn::SetEvasiveRollPresentation(const bool bActive, const float RollDegrees)
{
	if (EvasiveRollPivot)
	{
		EvasiveRollPivot->SetRelativeRotation(
			FRotator(0.0f, 0.0f, bActive ? RollDegrees : 0.0f));
	}
}

void AArcadeJetPawn::ApplyVisualRotation()
{
	if (VisualRoot)
	{
		const bool bMobilityPresentation = QuickReversal
			&& QuickReversal->IsReversalActive();
		const FQuat MobilityRotation = bMobilityPresentation
			? QuickReversal->GetPresentationRelativeRotation(GetActorQuat())
			: FQuat::Identity;
		const float FlightBankBlend = bMobilityPresentation
			? QuickReversal->GetFlightBankBlendAlpha() : 1.0f;
		const float FlightBankDegrees = CurrentVisualBankDegrees * FlightBankBlend;
		const FQuat FlightBankRotation(
			FVector::ForwardVector, FMath::DegreesToRadians(FlightBankDegrees));
		VisualRoot->SetRelativeLocation(
			FVector(0.0f, 0.0f, -ArcadeJetPresentationTuning::EvasiveRollPivotHeight));
		VisualRoot->SetRelativeRotation(
			(MobilityRotation * FlightBankRotation).GetNormalized());
	}
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
	PlayerInputComponent->BindAction(TEXT("PlaneEvasiveRollLeftTap"), IE_Pressed,
		this, &AArcadeJetPawn::RegisterLeftRollTap);
	PlayerInputComponent->BindAction(TEXT("PlaneEvasiveRollRightTap"), IE_Pressed,
		this, &AArcadeJetPawn::RegisterRightRollTap);
	PlayerInputComponent->BindAction(TEXT("PlaneQuickReversal"), IE_Pressed,
		this, &AArcadeJetPawn::ActivateQuickReversal);
	PlayerInputComponent->BindAxis(TEXT("PlaneBrake"), this, &AArcadeJetPawn::SetBrake);
	PlayerInputComponent->BindAxis(TEXT("PlaneHoverZoom"), this, &AArcadeJetPawn::SetHoverCameraZoom);
	PlayerInputComponent->BindAxis(TEXT("PlaneMouseX"), this, &AArcadeJetPawn::AddHoverCameraYawInput);
	PlayerInputComponent->BindAxis(TEXT("PlaneMouseY"), this, &AArcadeJetPawn::AddHoverCameraPitchInput);
	PlayerInputComponent->BindAction(TEXT("PlaneBoost"), IE_Pressed, this, &AArcadeJetPawn::StartBoost);
	PlayerInputComponent->BindAction(TEXT("PlaneBoost"), IE_Released, this, &AArcadeJetPawn::StopBoost);
	PlayerInputComponent->BindAction(TEXT("PlaneHover"), IE_Pressed, this, &AArcadeJetPawn::ToggleHover);
	PlayerInputComponent->BindAction(TEXT("PlaneMissileFire"), IE_Pressed, this, &AArcadeJetPawn::StartMissileFire);
	PlayerInputComponent->BindAction(TEXT("PlaneMissileFire"), IE_Released, this, &AArcadeJetPawn::StopMissileFire);
	PlayerInputComponent->BindAction(TEXT("PlaneGunFire"), IE_Pressed, this, &AArcadeJetPawn::StartGunFire);
	PlayerInputComponent->BindAction(TEXT("PlaneGunFire"), IE_Released, this, &AArcadeJetPawn::StopGunFire);
	PlayerInputComponent->BindAction(TEXT("PlaneCycleGunWeapon"), IE_Pressed,
		this, &AArcadeJetPawn::CycleGunWeapon);
	PlayerInputComponent->BindAction(TEXT("PlaneCycleMissileWeapon"), IE_Pressed,
		this, &AArcadeJetPawn::CycleMissileWeapon);
	PlayerInputComponent->BindAction(TEXT("PlaneRejectMissileTarget"), IE_Pressed,
		this, &AArcadeJetPawn::RejectMissileTarget);
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
		// Steering and lateral movement remain continuous through the roll. Double
		// taps are detected by separate key actions, so they do not require cutting
		// the existing strafe velocity.
		FlightMovement->SetLocalFlightInput(CursorSteering, StrafeInput, BrakeInput);
	}
}

void AArcadeJetPawn::SetStrafe(const float Value)
{
	StrafeInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AArcadeJetPawn::RegisterLeftRollTap()
{
	EvasiveRoll->RegisterStrafeTap(EEvasiveRollDirection::Left);
}

void AArcadeJetPawn::RegisterRightRollTap()
{
	EvasiveRoll->RegisterStrafeTap(EEvasiveRollDirection::Right);
}

void AArcadeJetPawn::ActivateQuickReversal()
{
	const float PreferredDirection = FMath::Abs(CursorSteering.X) > 0.05f
		? CursorSteering.X : StrafeInput;
	QuickReversal->RequestActivation(PreferredDirection);
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
	// Stop held triggers before requesting the transition. Server-side weapon
	// checks provide the authoritative equivalent.
	WeaponSystem->SetFireHeld(EWeaponSlot::Gun, false);
	WeaponSystem->SetFireHeld(EWeaponSlot::Missile, false);
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

void AArcadeJetPawn::StartMissileFire()
{
	if (FlightMovement->GetHoverState() == EArcadeHoverState::Hovering)
	{
		BeginHoverCameraOrbit();
		return;
	}
	if (!FlightMovement->IsCombatFlightEnabled())
	{
		return;
	}
	WeaponSystem->SetFireHeld(EWeaponSlot::Missile, true);
}

void AArcadeJetPawn::StopMissileFire()
{
	EndHoverCameraOrbit();
	WeaponSystem->SetFireHeld(EWeaponSlot::Missile, false);
}

void AArcadeJetPawn::StartGunFire()
{
	if (!FlightMovement->IsCombatFlightEnabled())
	{
		return;
	}
	WeaponSystem->SetFireHeld(EWeaponSlot::Gun, true);
}

void AArcadeJetPawn::StopGunFire()
{
	WeaponSystem->SetFireHeld(EWeaponSlot::Gun, false);
}

void AArcadeJetPawn::CycleGunWeapon()
{
	WeaponSystem->CycleWeapon(EWeaponSlot::Gun);
}

void AArcadeJetPawn::CycleMissileWeapon()
{
	WeaponSystem->CycleWeapon(EWeaponSlot::Missile);
}

void AArcadeJetPawn::RejectMissileTarget()
{
	TargetSelection->CycleSelectedTarget();
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

void AArcadeJetPawn::UpdateCameraAfterFlight(const float DeltaSeconds)
{
	const float BoostAlpha = JetBoost->GetBoostAlpha();
	const float HoverAlpha = FlightMovement->GetHoverAlpha();
	const EArcadeHoverState HoverState = FlightMovement->GetHoverState();
	const bool bUseFlightCamera = HoverState == EArcadeHoverState::Flying;
	const FVector PlaneLocation = GetActorLocation();
	const FVector FlightForward = GetActorForwardVector();
	const FVector FlightUp = GetActorUpVector();
	const FVector DesiredFlightEye = PlaneLocation
		- FlightForward * ArcadeJetCameraTuning::FlightDistance
		+ FlightUp * ArcadeJetCameraTuning::FlightHeight;
	const FVector FlightLookTarget = PlaneLocation
		+ FlightForward * ArcadeJetCameraTuning::LookAheadDistance;
	const FVector LevelFlightForward = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector();
	const FVector StableTransitionEye = PlaneLocation
		- LevelFlightForward * ArcadeJetCameraTuning::FlightDistance
		+ FVector::UpVector * ArcadeJetCameraTuning::FlightHeight;
	const FVector StableTransitionLookTarget = PlaneLocation
		+ LevelFlightForward * ArcadeJetCameraTuning::LookAheadDistance;

	const FRotator HoverOrbitRotation(
		HoverCameraOrbitPitch,
		GetActorRotation().Yaw + HoverCameraOrbitYaw,
		0.0f);
	const FVector DesiredHoverEye = PlaneLocation
		- HoverOrbitRotation.Vector() * CurrentHoverCameraDistance
		+ FVector::UpVector * HoverCameraHeight;
	// During entering/leaving, use a level flight basis so aircraft pitch cannot drag
	// the camera around an arc. HoverAlpha still blends both endpoints, preventing a
	// hard camera switch when flight control is returned.
	const FVector FlightEyeForBlend = bUseFlightCamera ? DesiredFlightEye : StableTransitionEye;
	const FVector DesiredEye = FMath::Lerp(FlightEyeForBlend, DesiredHoverEye, HoverAlpha);

	const float FlightFollowResponse = FMath::Lerp(
		ArcadeJetCameraTuning::NormalFollowResponse,
		ArcadeJetCameraTuning::BoostFollowResponse,
		BoostAlpha);
	const float FollowResponse = FMath::Lerp(
		FlightFollowResponse,
		ArcadeJetCameraTuning::HoverFollowResponse,
		HoverAlpha);
	const bool bQuickReversalActive = QuickReversal
		&& QuickReversal->IsReversalActive();
	if (bQuickReversalActive)
	{
		// Mobility maneuvers use their own camera path. The path turns later than the
		// aircraft and looks back at it, instead of attaching the camera to its forward
		// vector. This keeps the fast reversal readable and avoids a rigid 180-degree
		// camera whip.
		if (!bQuickReversalCameraPathActive)
		{
			QuickReversalCameraStartOffset = CurrentCameraWorldPosition - PlaneLocation;
			QuickReversalCameraStartPlaneLocation = PlaneLocation;
			const FQuat StartCameraRotation = CurrentCameraWorldRotation.Quaternion();
			QuickReversalCameraStartViewDirection =
				StartCameraRotation.GetForwardVector().GetSafeNormal();
			QuickReversalCameraStartUp =
				StartCameraRotation.GetUpVector().GetSafeNormal();
			if (QuickReversalCameraStartOffset.IsNearlyZero())
			{
				QuickReversalCameraStartOffset = DesiredFlightEye - PlaneLocation;
			}
			bQuickReversalCameraPathActive = true;
		}

		const float ManeuverProgress = QuickReversal->GetManeuverProgress();
		const float RawCameraProgress = static_cast<float>(
			FMath::GetMappedRangeValueClamped(
				FVector2D(ArcadeJetCameraTuning::ReversalCameraDelayFraction, 1.0),
				FVector2D(0.0, 1.0), static_cast<double>(ManeuverProgress)));
		const float CameraProgress = FMath::SmoothStep(0.0f, 1.0f, RawCameraProgress);
		const float RawRecoveryAlpha = static_cast<float>(
			FMath::GetMappedRangeValueClamped(
				FVector2D(ArcadeJetCameraTuning::ReversalCameraRecoveryStart, 1.0),
				FVector2D(0.0, 1.0), static_cast<double>(ManeuverProgress)));
		const float RecoveryAlpha = FMath::SmoothStep(0.0f, 1.0f, RawRecoveryAlpha);
		const float ArcAlpha = FMath::Sin(UE_PI * ManeuverProgress);
		const float OrbitDegrees = QuickReversal->GetDirectionSign()
			* 180.0f * CameraProgress;

		const FVector ManeuverUp = QuickReversal->GetManeuverUpAxis();
		const float AxialOffset = FVector::DotProduct(
			QuickReversalCameraStartOffset, ManeuverUp);
		FVector RadialOffset = QuickReversalCameraStartOffset
			- ManeuverUp * AxialOffset;
		const float StartRadius = RadialOffset.Size();
		if (RadialOffset.IsNearlyZero())
		{
			RadialOffset = -FlightForward;
		}
		RadialOffset = RadialOffset.GetSafeNormal()
			.RotateAngleAxis(OrbitDegrees, ManeuverUp)
			* (StartRadius + ArcadeJetCameraTuning::ReversalCameraPullBack * ArcAlpha);
		// Keep the path centered at the activation point. During the first phase the
		// plane moves away while facing the camera, then flies back toward it before
		// the delayed camera orbit catches up.
		const FVector PatternEye = QuickReversalCameraStartPlaneLocation + RadialOffset
			+ ManeuverUp * (
				AxialOffset
				+ ArcadeJetCameraTuning::ReversalCameraLift * ArcAlpha);
		CurrentCameraWorldPosition = FMath::Lerp(
			PatternEye, DesiredFlightEye, RecoveryAlpha);

		const float LookAcquireAlpha = FMath::SmoothStep(
			0.0f, ArcadeJetCameraTuning::ReversalCameraLookAcquireEnd,
			ManeuverProgress);
		const FVector PlaneViewDirection =
			(PlaneLocation - CurrentCameraWorldPosition).GetSafeNormal();
		const FVector TrackingViewDirection = FMath::Lerp(
			QuickReversalCameraStartViewDirection,
			PlaneViewDirection,
			LookAcquireAlpha).GetSafeNormal();
		const FVector FlightViewDirection =
			(FlightLookTarget - CurrentCameraWorldPosition).GetSafeNormal();
		const FVector PatternViewDirection = FMath::Lerp(
			TrackingViewDirection,
			FlightViewDirection,
			RecoveryAlpha).GetSafeNormal();
		const FVector TrackingUp = FMath::Lerp(
			QuickReversalCameraStartUp,
			ManeuverUp,
			LookAcquireAlpha).GetSafeNormal();
		const FVector PatternUp = FMath::Lerp(
			TrackingUp, FlightUp, RecoveryAlpha).GetSafeNormal();
		if (!PatternViewDirection.IsNearlyZero())
		{
			CurrentCameraWorldRotation = FRotationMatrix::MakeFromXZ(
				PatternViewDirection, PatternUp).Rotator();
		}
	}
	else if (FVector::DistSquared(CurrentCameraWorldPosition, DesiredEye)
			> FMath::Square(ArcadeJetCameraTuning::TeleportSnapDistance))
	{
		CurrentCameraWorldPosition = DesiredEye;
	}
	else
	{
		CurrentCameraWorldPosition = FMath::VInterpTo(
			CurrentCameraWorldPosition, DesiredEye, DeltaSeconds, FollowResponse);
	}
	if (!bQuickReversalActive)
	{
		bQuickReversalCameraPathActive = false;
	}

	// Blend normalized view directions rather than distant target positions. This
	// gives a smooth return from looking at the hovering aircraft to looking ahead.
	const FVector FlightLookForBlend = bUseFlightCamera
		? FlightLookTarget : StableTransitionLookTarget;
	const FVector FlightViewDirection =
		(FlightLookForBlend - CurrentCameraWorldPosition).GetSafeNormal();
	const FVector HoverViewDirection =
		(PlaneLocation - CurrentCameraWorldPosition).GetSafeNormal();
	const FVector ViewDirection = FMath::Lerp(
		FlightViewDirection, HoverViewDirection, HoverAlpha).GetSafeNormal();
	const FVector FlightUpForBlend = bUseFlightCamera ? FlightUp : FVector::UpVector;
	const FVector CameraUp = FMath::Lerp(
		FlightUpForBlend, FVector::UpVector, HoverAlpha).GetSafeNormal();
	if (!bQuickReversalActive && !ViewDirection.IsNearlyZero())
	{
		CurrentCameraWorldRotation = FRotationMatrix::MakeFromXZ(
			ViewDirection.GetSafeNormal(), CameraUp).Rotator();
	}
	CameraBoom->SetWorldLocationAndRotation(
		CurrentCameraWorldPosition, CurrentCameraWorldRotation);
	WeaponSystem->SetAimContext(
		FollowCamera->GetComponentLocation(), FollowCamera->GetForwardVector());
}
