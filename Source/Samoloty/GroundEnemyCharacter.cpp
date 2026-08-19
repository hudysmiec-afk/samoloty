#include "GroundEnemyCharacter.h"

#include "AIController.h"
#include "AircraftCollisionComponent.h"
#include "GroundEnemyAIComponent.h"
#include "HealthComponent.h"
#include "HitRewindComponent.h"
#include "HomingMissileWeaponComponent.h"
#include "JetStatsComponent.h"
#include "PlaneWeaponComponent.h"
#include "RifleGunComponent.h"
#include "TargetIntentComponent.h"
#include "WeaponSystemComponent.h"
#include "WorldNameComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGroundEnemyCharacter::AGroundEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(10.0f);
	SetNetCullDistanceSquared(FMath::Square(300000.0f));

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	GetCapsuleComponent()->InitCapsuleSize(55.0f, 100.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	// The capsule belongs only to CharacterMovement/NavMesh, not weapon targeting.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BodyHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitbox"));
	BodyHitbox->InitBoxExtent(FVector(85.0f, 60.0f, 45.0f));
	BodyHitbox->SetupAttachment(GetMesh());
	UpperBodyHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("UpperBodyHitbox"));
	UpperBodyHitbox->InitBoxExtent(FVector(55.0f, 50.0f, 35.0f));
	UpperBodyHitbox->SetupAttachment(GetMesh());
	UpperBodyHitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
	UAircraftCollisionComponent::ConfigureDamageHitbox(BodyHitbox);
	UAircraftCollisionComponent::ConfigureDamageHitbox(UpperBodyHitbox);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 600.0f;
	Movement->bOrientRotationToMovement = false;
	Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);

	JetStats = CreateDefaultSubobject<UJetStatsComponent>(TEXT("JetStats"));
	FJetCombatStats CombatStats;
	CombatStats.MaxHealth = 500.0f;
	JetStats->SetBaseCombatStats(CombatStats);
	FRifleGunStats RifleStats;
	RifleStats.Damage = 20.0f;
	RifleStats.ShotsPerSecond = 8.0f;
	RifleStats.MaxRange = 30000.0f;
	RifleStats.SpreadAngleDegrees = 4.0f;
	JetStats->SetBaseRifleGunStats(RifleStats);
	FHomingMissileStats MissileStats;
	MissileStats.MissileDamage = 150.0f;
	MissileStats.LockRange = 50000.0f;
	MissileStats.Cooldown = 3.0f;
	JetStats->SetBaseHomingMissileStats(MissileStats);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	HitRewind = CreateDefaultSubobject<UHitRewindComponent>(TEXT("HitRewind"));
	RifleGun = CreateDefaultSubobject<URifleGunComponent>(TEXT("RifleGun"));
	HomingMissiles = CreateDefaultSubobject<UHomingMissileWeaponComponent>(TEXT("HomingMissiles"));
	WeaponSystem = CreateDefaultSubobject<UWeaponSystemComponent>(TEXT("WeaponSystem"));
	EnemyAI = CreateDefaultSubobject<UGroundEnemyAIComponent>(TEXT("EnemyAI"));
	TargetIntent = CreateDefaultSubobject<UTargetIntentComponent>(TEXT("TargetIntent"));
	WorldName = CreateDefaultSubobject<UWorldNameComponent>(TEXT("WorldName"));
	WorldName->SetDefaultDisplayName(NSLOCTEXT("WorldNames", "GroundEnemy", "Ground Enemy"));

	GunMuzzleLeft = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleLeft"));
	GunMuzzleLeft->SetupAttachment(GetCapsuleComponent());
	GunMuzzleLeft->SetRelativeLocation(FVector(70.0f, -35.0f, 20.0f));
	GunMuzzleRight = CreateDefaultSubobject<USceneComponent>(TEXT("GunMuzzleRight"));
	GunMuzzleRight->SetupAttachment(GetCapsuleComponent());
	GunMuzzleRight->SetRelativeLocation(FVector(70.0f, 35.0f, 20.0f));
	MissileMuzzleLeft = CreateDefaultSubobject<USceneComponent>(TEXT("MissileMuzzleLeft"));
	MissileMuzzleLeft->SetupAttachment(GetCapsuleComponent());
	MissileMuzzleLeft->SetRelativeLocation(FVector(50.0f, -45.0f, 35.0f));
	MissileMuzzleRight = CreateDefaultSubobject<USceneComponent>(TEXT("MissileMuzzleRight"));
	MissileMuzzleRight->SetupAttachment(GetCapsuleComponent());
	MissileMuzzleRight->SetRelativeLocation(FVector(50.0f, 45.0f, 35.0f));
}

void AGroundEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !GetController())
	{
		SpawnDefaultController();
	}
	WeaponSystem->ConfigureFirePoints(EWeaponSlot::Gun, GunMuzzleLeft, GunMuzzleRight);
	WeaponSystem->ConfigureFirePoints(EWeaponSlot::Missile, MissileMuzzleLeft, MissileMuzzleRight);
	RifleGun->SetDamagePlayersOnly(true);
}
