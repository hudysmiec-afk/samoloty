#include "RocketWeaponComponent.h"

#include "ArcadeFlightComponent.h"
#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RocketLaunchPattern.h"
#include "RocketProjectile.h"
#include "RocketSimulationManager.h"
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
	SetWeaponSlot(EWeaponSlot::Missile);
	SetWeaponDisplayName(TEXT("Rocket Barrage"));
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

void URocketWeaponComponent::SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint)
{
	SetSpawnPoints(LeftPoint, RightPoint);
}

void URocketWeaponComponent::SetFireHeld(const bool bHeld)
{
	if (bHeld && !IsWeaponEquipped())
	{
		return;
	}
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
	if (bHeld && !IsWeaponEquipped())
	{
		return;
	}
	SetAuthoritativeFireHeld(bHeld);
}

void URocketWeaponComponent::SetAuthoritativeFireHeld(const bool bHeld)
{
	bServerFireHeld = bHeld;
	if (bHeld && WeaponState == ERocketWeaponState::Ready && !IsOwnerFireBlocked())
	{
		StartBarrage();
	}
}

void URocketWeaponComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsWeaponEquipped())
	{
		return;
	}
	if (IsOwnerFireBlocked())
	{
		UpdateDebugCounts(DeltaTime);
		DrawWeaponDebug();
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
		if (Flight && Flight->GetHoverState() != EArcadeHoverState::Flying)
		{
			bServerFireHeld = false;
			if (WeaponState == ERocketWeaponState::FiringSalvos)
			{
				WeaponState = ERocketWeaponState::Ready;
				SalvosFired = 0;
				NextActionServerTime = 0.0;
			}
			UpdateDebugCounts(DeltaTime);
			DrawWeaponDebug();
			return;
		}
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
		else if (WeaponState == ERocketWeaponState::Ready && bServerFireHeld)
		{
			StartBarrage();
		}
		UpdateDebugCounts(DeltaTime);
	}
	DrawWeaponDebug();
}

void URocketWeaponComponent::OnWeaponEquippedChanged()
{
	bLocalFireHeld = false;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bServerFireHeld = false;
	if (WeaponState == ERocketWeaponState::FiringSalvos)
	{
		EnterCooldown();
	}
}

void URocketWeaponComponent::StartEquipCooldown()
{
	const UJetStatsComponent* StatsComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	if (!GetWorld() || !StatsComponent)
	{
		return;
	}
	const double ReadyTime = GetServerTimeSeconds()
		+ FMath::Max(0.0f, StatsComponent->GetRocketBarrageStats().Cooldown);
	WeaponState = ERocketWeaponState::Cooldown;
	SalvosFired = 0;
	NextActionServerTime = FMath::Max(NextActionServerTime, ReadyTime);
}

void URocketWeaponComponent::StartBarrage()
{
	if (!GetOwner()->HasAuthority() || WeaponState != ERocketWeaponState::Ready
		|| IsOwnerFireBlocked())
	{
		return;
	}
	const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	if (Flight && Flight->GetHoverState() != EArcadeHoverState::Flying)
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
	PrepareSalvo(Stats);
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

	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector StartLocation = SpawnPoint->GetComponentLocation();
	const FVector CommonFormationCenter = LeftSpawnPoint.IsValid() && RightSpawnPoint.IsValid()
		? (LeftSpawnPoint->GetComponentLocation() + RightSpawnPoint->GetComponentLocation()) * 0.5f
		: GetOwner()->GetActorLocation();
	FRocketSeparationSettings SeparationSettings;
	SeparationSettings.SpreadAngleDegrees = Stats.SpreadAngleDegrees;
	SeparationSettings.ForwardDistance = Stats.SeparationForwardDistance;
	SeparationSettings.Radius = Stats.SeparationRadius;
	SeparationSettings.CurveStrength = Stats.SeparationCurveStrength;
	const FRocketSeparationPath SeparationPath = RocketLaunchPattern::BuildSeparationPath(
		StartLocation, CommonFormationCenter, Forward, GetOwner()->GetActorRightVector(),
		GetOwner()->GetActorUpVector(), bLeftSide, SideIndex, SideCount, RandomStream,
		SeparationSettings);

	FRocketLaunchData Data;
	Data.StartLocation = SeparationPath.StartLocation;
	Data.Direction = SeparationPath.Direction;
	Data.SeparationControlPoint1 = SeparationPath.ControlPoint1;
	Data.SeparationControlPoint2 = SeparationPath.ControlPoint2;
	Data.SeparationEndPoint = SeparationPath.EndPoint;
	Data.bUseSeparationCurve = SeparationPath.bUseCurve;
	Data.Speed = Stats.RocketSpeed;
	Data.Damage = Stats.RocketDamage;
	Data.MaxTravelDistance = Stats.MaxTravelDistance;
	Data.ProximityRadius = Stats.ProximityRadius;
	Data.ProximityCheckInterval = Stats.ProximityCheckInterval;
	Data.ServerStartTime = GetWorld()->GetTimeSeconds();
	Data.GuidanceMode = ERocketGuidanceMode::Straight;
	Data.bDrawDebug = bDrawRocketDebug;
	ConfigureRocketLaunchData(Data, SeparationPath, RandomStream);

	const FVector InitialDirection =
		(SeparationPath.ControlPoint1 - SeparationPath.StartLocation).GetSafeNormal(
			SMALL_NUMBER, SeparationPath.Direction);
	const FTransform SpawnTransform(InitialDirection.Rotation(), Data.StartLocation);
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (UsesDataOnlyRocketSimulation())
	{
		if (URocketSimulationManager* Manager =
			GetWorld()->GetSubsystem<URocketSimulationManager>())
		{
			if (Manager->LaunchDataOnlyStraightRocket(
				RocketClass, Data, GetOwner(), OwnerPawn) != INDEX_NONE)
			{
				return;
			}
		}
	}
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

void URocketWeaponComponent::PrepareSalvo(const FRocketBarrageStats& Stats)
{
}

void URocketWeaponComponent::ConfigureRocketLaunchData(FRocketLaunchData& LaunchData,
	const FRocketSeparationPath& SeparationPath, FRandomStream& RandomStream) const
{
}

void URocketWeaponComponent::InheritMissingPresentationFrom(
	const URocketWeaponComponent& Source)
{
	if (RocketClass == ARocketProjectile::StaticClass())
	{
		RocketClass = Source.RocketClass;
	}
	if (!RocketLaunchSound)
	{
		RocketLaunchSound = Source.RocketLaunchSound;
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
		const URocketSimulationManager* Manager =
			GetWorld()->GetSubsystem<URocketSimulationManager>();
		ServerActiveRocketCountForDebug = Manager
			? Manager->GetActiveRocketCount() : ARocketProjectile::GetServerActiveRocketCount();
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
	if (!IsWeaponEquipped() || !bShowWeaponDebug || !OwnerPawn
		|| !OwnerPawn->IsLocallyControlled() || !GEngine)
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
	const FString WeaponName = GetWeaponDisplayName().ToString().ToUpper();
	const FString Message = FString::Printf(
		TEXT("HP: %.0f / %.0f\n%s [%s]\nSalvos: %d / %d | Next: %.2fs\nFire held: %s\nActive owned: %d | Server active: %d"),
		Health->GetCurrentHealth(), Health->GetMaxHealth(), *WeaponName, StateText,
		SalvosFired, Stats.SalvoCount,
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
bool URocketWeaponComponent::GetCooldownStatus(float& OutRemainingSeconds,
	float& OutDurationSeconds) const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	OutDurationSeconds = Stats ? Stats->GetRocketBarrageStats().Cooldown : 0.0f;
	OutRemainingSeconds = WeaponState == ERocketWeaponState::Cooldown
		? FMath::Clamp(static_cast<float>(NextActionServerTime - GetServerTimeSeconds()),
			0.0f, OutDurationSeconds)
		: 0.0f;
	return OutDurationSeconds > UE_SMALL_NUMBER;
}
