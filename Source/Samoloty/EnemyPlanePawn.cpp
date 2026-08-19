#include "EnemyPlanePawn.h"

#include "AircraftCollisionComponent.h"
#include "EnemyPlaneAIComponent.h"
#include "HealthComponent.h"
#include "HitRewindComponent.h"
#include "JetStatsComponent.h"
#include "RifleGunComponent.h"
#include "TargetIntentComponent.h"
#include "WorldNameComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AEnemyPlanePawn::AEnemyPlanePawn()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);
	// EnemyPlaneAIComponent sends a compact authoritative state and smooths it on clients.
	SetReplicateMovement(false);
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(10.0f);
	SetNetCullDistanceSquared(FMath::Square(500000.0f));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MovementCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("MovementCollision"));
	MovementCollision->InitCapsuleSize(55.0f, 220.0f);
	MovementCollision->SetupAttachment(SceneRoot);
	MovementCollision->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(SceneRoot);

	BodyHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitbox"));
	BodyHitbox->InitBoxExtent(FVector(240.0f, 48.0f, 45.0f));
	BodyHitbox->SetupAttachment(VisualRoot);
	LeftWingHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftWingHitbox"));
	LeftWingHitbox->InitBoxExtent(FVector(115.0f, 75.0f, 18.0f));
	LeftWingHitbox->SetupAttachment(VisualRoot);
	LeftWingHitbox->SetRelativeLocation(FVector(0.0f, -115.0f, 0.0f));
	RightWingHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("RightWingHitbox"));
	RightWingHitbox->InitBoxExtent(FVector(115.0f, 75.0f, 18.0f));
	RightWingHitbox->SetupAttachment(VisualRoot);
	RightWingHitbox->SetRelativeLocation(FVector(0.0f, 115.0f, 0.0f));
	AircraftCollision = CreateDefaultSubobject<UAircraftCollisionComponent>(TEXT("AircraftCollision"));
	HitRewind = CreateDefaultSubobject<UHitRewindComponent>(TEXT("HitRewind"));
	UAircraftCollisionComponent::ConfigureDamageHitbox(BodyHitbox);
	UAircraftCollisionComponent::ConfigureDamageHitbox(LeftWingHitbox);
	UAircraftCollisionComponent::ConfigureDamageHitbox(RightWingHitbox);

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(VisualRoot);
	PlaneMesh->SetAbsolute(false, false, false);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	JetStats = CreateDefaultSubobject<UJetStatsComponent>(TEXT("JetStats"));
	FJetFlightStats FlightStats;
	FlightStats.ForwardSpeed = 2250.0f;
	FlightStats.MaxYawTurnRate = 55.0f;
	FlightStats.MaxPitchTurnRate = 45.0f;
	JetStats->SetBaseFlightStats(FlightStats);
	FJetCombatStats CombatStats;
	CombatStats.MaxHealth = 500.0f;
	JetStats->SetBaseCombatStats(CombatStats);
	FRifleGunStats RifleStats;
	RifleStats.Damage = 20.0f;
	RifleStats.ShotsPerSecond = 10.0f;
	RifleStats.SpreadAngleDegrees = 4.0f;
	JetStats->SetBaseRifleGunStats(RifleStats);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	RifleGun = CreateDefaultSubobject<URifleGunComponent>(TEXT("RifleGun"));
	EnemyAI = CreateDefaultSubobject<UEnemyPlaneAIComponent>(TEXT("EnemyAI"));
	TargetIntent = CreateDefaultSubobject<UTargetIntentComponent>(TEXT("TargetIntent"));
	WorldName = CreateDefaultSubobject<UWorldNameComponent>(TEXT("WorldName"));
	WorldName->SetDefaultDisplayName(NSLOCTEXT("WorldNames", "EnemyPlane", "Enemy Plane"));

	GunMuzzleLeft = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleLeft"));
	GunMuzzleLeft->SetupAttachment(VisualRoot);
	GunMuzzleLeft->SetRelativeLocation(FVector(200.0f, -100.0f, 0.0f));
	GunMuzzleRight = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleRight"));
	GunMuzzleRight->SetupAttachment(VisualRoot);
	GunMuzzleRight->SetRelativeLocation(FVector(200.0f, 100.0f, 0.0f));
}

void AEnemyPlanePawn::BeginPlay()
{
	Super::BeginPlay();
	if (PlaneMesh && VisualRoot)
	{
		PlaneMesh->AttachToComponent(VisualRoot,
			FAttachmentTransformRules::KeepRelativeTransform);
		PlaneMesh->SetAbsolute(false, false, false);
	}
	AircraftCollision->SetMovementCapsule(MovementCollision);
	RifleGun->SetMuzzlePoints(GunMuzzleLeft, GunMuzzleRight);
	RifleGun->SetDamagePlayersOnly(true);
}
