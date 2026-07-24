#include "RocketWeaponComponent.h"

#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RocketProjectile.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

URocketWeaponComponent::URocketWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	RocketClass = ARocketProjectile::StaticClass();
}

void URocketWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		ServerActiveRocketCountForDebug = ARocketProjectile::GetServerActiveRocketCount();
	}
}

void URocketWeaponComponent::SetSpawnPoints(USceneComponent* InLeftSpawnPoint, USceneComponent* InRightSpawnPoint)
{
	LeftSpawnPoint = InLeftSpawnPoint;
	RightSpawnPoint = InRightSpawnPoint;
}

void URocketWeaponComponent::SetFireHeld(const bool bHeld)
{
	bLocalFireHeld = bHeld;
	if (GetOwner()->HasAuthority())
	{
		SetAuthoritativeFireHeld(bHeld);
	}
	else
	{
		ServerSetFireHeld(bHeld);
	}
}

void URocketWeaponComponent::ServerSetFireHeld_Implementation(const bool bHeld)
{
	SetAuthoritativeFireHeld(bHeld);
}

void URocketWeaponComponent::SetAuthoritativeFireHeld(const bool bHeld)
{
	bServerFireHeld = bHeld;
	if (bHeld && WeaponState == ERocketWeaponState::Ready)
	{
		StartBarrage();
	}
}

void URocketWeaponComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner()->HasAuthority())
	{
		const double ServerNow = GetWorld()->GetTimeSeconds();
		if (WeaponState == ERocketWeaponState::FiringSalvos && ServerNow >= NextActionServerTime)
		{
			FireSalvo();
		}
		else if (WeaponState == ERocketWeaponState::Cooldown && ServerNow >= NextActionServerTime)
		{
			WeaponState = ERocketWeaponState::Ready;
			NextActionServerTime = 0.0;
			if (bServerFireHeld)
			{
				StartBarrage();
			}
		}
		UpdateDebugCounts(DeltaTime);
	}
	DrawWeaponDebug();
}

void URocketWeaponComponent::StartBarrage()
{
	if (!GetOwner()->HasAuthority() || WeaponState != ERocketWeaponState::Ready)
	{
		return;
	}
	const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	if ((Health && Health->IsDead()) || !LeftSpawnPoint.IsValid() || !RightSpawnPoint.IsValid() || !RocketClass)
	{
		return;
	}
	SalvosFired = 0;
	WeaponState = ERocketWeaponState::FiringSalvos;
	FireSalvo();
}

void URocketWeaponComponent::FireSalvo()
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent || !RocketClass)
	{
		EnterCooldown();
		return;
	}

	const FRocketBarrageStats& Stats = StatsComponent->GetRocketBarrageStats();
	const int32 RocketCount = FMath::Max(1, Stats.RocketsPerSalvo);
	const int32 LeftCount = (RocketCount + 1) / 2;
	const int32 RightCount = RocketCount / 2;
	FRandomStream RandomStream(FMath::Rand());
	int32 LeftIndex = 0;
	int32 RightIndex = 0;
	for (int32 Index = 0; Index < RocketCount; ++Index)
	{
		const bool bLeft = Index % 2 == 0;
		SpawnRocket(bLeft ? LeftSpawnPoint.Get() : RightSpawnPoint.Get(), bLeft,
			bLeft ? LeftIndex++ : RightIndex++, bLeft ? LeftCount : RightCount, RandomStream);
	}

	if (RocketLaunchSound && LeftSpawnPoint.IsValid() && RightSpawnPoint.IsValid())
	{
		const FVector LaunchSoundLocation =
			(LeftSpawnPoint->GetComponentLocation() + RightSpawnPoint->GetComponentLocation()) * 0.5f;
		MulticastPlaySalvoLaunchSound(LaunchSoundLocation);
	}

	++SalvosFired;
	if (SalvosFired >= FMath::Max(1, Stats.SalvoCount))
	{
		EnterCooldown();
	}
	else
	{
		WeaponState = ERocketWeaponState::FiringSalvos;
		NextActionServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, Stats.SalvoInterval);
	}
}

void URocketWeaponComponent::MulticastPlaySalvoLaunchSound_Implementation(
	const FVector_NetQuantize Location)
{
	if (RocketLaunchSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, RocketLaunchSound, Location);
	}
}

void URocketWeaponComponent::EnterCooldown()
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const float Cooldown = StatsComponent ? StatsComponent->GetRocketBarrageStats().Cooldown : 0.0f;
	WeaponState = ERocketWeaponState::Cooldown;
	NextActionServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, Cooldown);
}

void URocketWeaponComponent::SpawnRocket(USceneComponent* SpawnPoint, const bool bLeftSide,
	const int32 SideIndex, const int32 SideCount, FRandomStream& RandomStream)
{
	if (!SpawnPoint || SideCount <= 0)
	{
		return;
	}
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}
	const FRocketBarrageStats& Stats = StatsComponent->GetRocketBarrageStats();

	const float SectorAlpha = (static_cast<float>(SideIndex) + RandomStream.FRand()) / SideCount;
	const float StartAngle = bLeftSide ? HALF_PI : -HALF_PI;
	const float DiskAngle = StartAngle + SectorAlpha * PI;
	const float RadiusAlpha = FMath::Sqrt(RandomStream.FRand());
	const float SpreadTangent = FMath::Tan(FMath::DegreesToRadians(Stats.SpreadAngleDegrees));
	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector SideOffset = GetOwner()->GetActorRightVector() * FMath::Cos(DiskAngle)
		+ GetOwner()->GetActorUpVector() * FMath::Sin(DiskAngle);
	const FVector Direction = (Forward + SideOffset * SpreadTangent * RadiusAlpha).GetSafeNormal();
	const float SeparationForwardDistance = FMath::Max(0.0f, Stats.SeparationForwardDistance);
	const FVector SeparationOffset = SideOffset * Stats.SeparationRadius * RadiusAlpha;
	const FVector StartLocation = SpawnPoint->GetComponentLocation();
	const FVector CommonFormationCenter = LeftSpawnPoint.IsValid() && RightSpawnPoint.IsValid()
		? (LeftSpawnPoint->GetComponentLocation() + RightSpawnPoint->GetComponentLocation()) * 0.5f
		: GetOwner()->GetActorLocation();
	const FVector SeparationEnd = CommonFormationCenter + Forward * SeparationForwardDistance + SeparationOffset;
	const FVector LateralTravel = SeparationEnd - (StartLocation + Forward * SeparationForwardDistance);
	const float ControlHandleLength = SeparationForwardDistance * Stats.SeparationCurveStrength;
	const FVector ControlPoint1 = StartLocation + Forward * (ControlHandleLength * 0.35f)
		+ LateralTravel * 0.35f;
	const FVector ControlPoint2 = SeparationEnd - Direction * ControlHandleLength;

	FRocketLaunchData Data;
	Data.StartLocation = StartLocation;
	Data.Direction = Direction;
	Data.SeparationControlPoint1 = ControlPoint1;
	Data.SeparationControlPoint2 = ControlPoint2;
	Data.SeparationEndPoint = SeparationEnd;
	Data.bUseSeparationCurve = SeparationForwardDistance > KINDA_SMALL_NUMBER;
	Data.Speed = Stats.RocketSpeed;
	Data.Damage = Stats.RocketDamage;
	Data.MaxTravelDistance = Stats.MaxTravelDistance;
	Data.ProximityRadius = Stats.ProximityRadius;
	Data.ProximityCheckInterval = Stats.ProximityCheckInterval;
	Data.ServerStartTime = GetWorld()->GetTimeSeconds();
	Data.GuidanceMode = ERocketGuidanceMode::Straight;
	Data.bDrawDebug = bDrawRocketDebug;

	const FVector InitialDirection = (ControlPoint1 - StartLocation).GetSafeNormal(SMALL_NUMBER, Direction);
	const FTransform SpawnTransform(InitialDirection.Rotation(), Data.StartLocation);
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ARocketProjectile* Rocket = GetWorld()->SpawnActorDeferred<ARocketProjectile>(RocketClass, SpawnTransform,
		GetOwner(), OwnerPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Rocket)
	{
		Rocket->InitializeRocket(Data);
		Rocket->OnRocketFinished.AddUObject(this, &URocketWeaponComponent::HandleRocketFinished);
		Rocket->FinishSpawning(SpawnTransform);
		ActiveOwnedRockets.Add(Rocket);
		ActiveOwnedRocketCount = ActiveOwnedRockets.Num();
	}
}

void URocketWeaponComponent::HandleRocketFinished(ARocketProjectile* Rocket)
{
	ActiveOwnedRockets.Remove(Rocket);
	ActiveOwnedRocketCount = ActiveOwnedRockets.Num();
}

void URocketWeaponComponent::UpdateDebugCounts(const float DeltaTime)
{
	DebugCountAccumulator += DeltaTime;
	if (DebugCountAccumulator >= 0.25f)
	{
		DebugCountAccumulator = 0.0f;
		ServerActiveRocketCountForDebug = ARocketProjectile::GetServerActiveRocketCount();
	}
}

double URocketWeaponComponent::GetServerTimeSeconds() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void URocketWeaponComponent::DrawWeaponDebug() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bShowWeaponDebug || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	if (!StatsComponent || !Health)
	{
		return;
	}
	const FRocketBarrageStats& Stats = StatsComponent->GetRocketBarrageStats();
	const TCHAR* StateText = WeaponState == ERocketWeaponState::Ready ? TEXT("READY")
		: WeaponState == ERocketWeaponState::FiringSalvos ? TEXT("FIRING") : TEXT("COOLDOWN");
	const float Remaining = FMath::Max(0.0f, static_cast<float>(NextActionServerTime - GetServerTimeSeconds()));
	const FString Message = FString::Printf(
		TEXT("HP: %.0f / %.0f\nROCKET BARRAGE [%s]\nSalvos: %d / %d | Next: %.2fs\nFire held: %s\nActive owned: %d | Server active: %d"),
		Health->GetCurrentHealth(), Health->GetMaxHealth(), StateText, SalvosFired, Stats.SalvoCount,
		Remaining, bLocalFireHeld ? TEXT("YES") : TEXT("NO"), ActiveOwnedRocketCount,
		ServerActiveRocketCountForDebug);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow, Message);
}

void URocketWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URocketWeaponComponent, WeaponState, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URocketWeaponComponent, SalvosFired, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URocketWeaponComponent, NextActionServerTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URocketWeaponComponent, ActiveOwnedRocketCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URocketWeaponComponent, ServerActiveRocketCountForDebug, COND_OwnerOnly);
}
