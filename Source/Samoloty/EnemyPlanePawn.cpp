#include "EnemyPlanePawn.h"

#include "EnemyPlaneAIComponent.h"
#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RifleGunComponent.h"
#include "Components/BoxComponent.h"
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

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(240.0f, 110.0f, 60.0f));
	Collision->SetCollisionProfileName(FName(TEXT("PlanePawn")));
	RootComponent = Collision;

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(Collision);
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

	GunMuzzleLeft = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleLeft"));
	GunMuzzleLeft->SetupAttachment(Collision);
	GunMuzzleLeft->SetRelativeLocation(FVector(200.0f, -100.0f, 0.0f));
	GunMuzzleRight = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleRight"));
	GunMuzzleRight->SetupAttachment(Collision);
	GunMuzzleRight->SetRelativeLocation(FVector(200.0f, 100.0f, 0.0f));
}

void AEnemyPlanePawn::BeginPlay()
{
	Super::BeginPlay();
	RifleGun->SetMuzzlePoints(GunMuzzleLeft, GunMuzzleRight);
	RifleGun->SetDamagePlayersOnly(true);
}
