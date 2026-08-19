#include "HomingMissileWeaponComponent.h"

#include "ArcadeFlightComponent.h"
#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "MissileTargetingComponent.h"
#include "RadarComponent.h"
#include "RocketLaunchPattern.h"
#include "RocketProjectile.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UHomingMissileWeaponComponent::UHomingMissileWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetWeaponSlot(EWeaponSlot::Missile);
	SetWeaponDisplayName(TEXT("Homing Missile"));
	MissileClass = ARocketProjectile::StaticClass();
}

void UHomingMissileWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UMissileTargetingComponent* Targeting = GetTargetingComponent())
	{
		Targeting->SetTargetingEnabled(IsWeaponEquipped());
	}
}

void UHomingMissileWeaponComponent::SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint)
{
	LeftSpawnPoint = LeftPoint;
	RightSpawnPoint = RightPoint;
}

void UHomingMissileWeaponComponent::SetFireHeld(const bool bHeld)
{
	if (bHeld && !IsWeaponEquipped())
	{
		return;
	}
	bLocalFireHeld = bHeld;
	AActor* LocalTarget = nullptr;
	if (const UMissileTargetingComponent* Targeting = GetTargetingComponent())
	{
		LocalTarget = Targeting->GetCurrentTarget();
	}
	LastTargetSentToServer = LocalTarget;
	if (GetOwner()->HasAuthority())
	{
		SetAuthoritativeFireHeld(bHeld, LocalTarget);
	}
	else
	{
		ServerSetFireHeld(bHeld, LocalTarget);
	}
}

void UHomingMissileWeaponComponent::RejectCurrentTarget()
{
	if (!IsWeaponEquipped())
	{
		return;
	}
	if (UMissileTargetingComponent* Targeting = GetTargetingComponent())
	{
		Targeting->RejectCurrentTarget();
	}
	UpdateRequestedTargetFromLocal();
}

void UHomingMissileWeaponComponent::SetAITargetAndFire(AActor* TargetActor, const bool bHeld)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	SetAuthoritativeFireHeld(bHeld, bHeld ? TargetActor : nullptr);
}

void UHomingMissileWeaponComponent::ServerSetFireHeld_Implementation(const bool bHeld,
	AActor* RequestedTargetActor)
{
	if (bHeld && !IsWeaponEquipped())
	{
		return;
	}
	SetAuthoritativeFireHeld(bHeld, RequestedTargetActor);
}

void UHomingMissileWeaponComponent::ServerUpdateRequestedTarget_Implementation(AActor* RequestedTargetActor)
{
	if (IsWeaponEquipped())
	{
		RequestedTarget = RequestedTargetActor;
	}
}

void UHomingMissileWeaponComponent::SetAuthoritativeFireHeld(const bool bHeld,
	AActor* RequestedTargetActor)
{
	bServerFireHeld = bHeld;
	// Releasing the trigger must not erase a still-valid lock while the remaining
	// salvos of the already-started sequence are pending.
	RequestedTarget = RequestedTargetActor;
	if (bHeld && WeaponState == EHomingMissileWeaponState::Ready
		&& !IsOwnerFireBlocked())
	{
		StartSequence();
	}
}

void UHomingMissileWeaponComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsWeaponEquipped())
	{
		return;
	}
	if (IsOwnerFireBlocked())
	{
		DrawWeaponDebug();
		return;
	}
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled()
		&& (bLocalFireHeld || WeaponState == EHomingMissileWeaponState::FiringSalvos))
	{
		UpdateRequestedTargetFromLocal();
	}

	if (GetOwner()->HasAuthority())
	{
		const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
		if (Flight && Flight->GetHoverState() != EArcadeHoverState::Flying)
		{
			bServerFireHeld = false;
			RequestedTarget.Reset();
			if (WeaponState == EHomingMissileWeaponState::FiringSalvos)
			{
				WeaponState = EHomingMissileWeaponState::Ready;
				SalvosFired = 0;
				NextActionServerTime = 0.0;
				SequenceTarget.Reset();
			}
			DrawWeaponDebug();
			return;
		}

		const double ServerNow = GetWorld()->GetTimeSeconds();
		if (WeaponState == EHomingMissileWeaponState::FiringSalvos && ServerNow >= NextActionServerTime)
		{
			FireSalvo();
		}
		else if (WeaponState == EHomingMissileWeaponState::Cooldown && ServerNow >= NextActionServerTime)
		{
			WeaponState = EHomingMissileWeaponState::Ready;
			NextActionServerTime = 0.0;
			if (bServerFireHeld)
			{
				StartSequence();
			}
		}
		else if (WeaponState == EHomingMissileWeaponState::Ready && bServerFireHeld)
		{
			StartSequence();
		}
	}
	DrawWeaponDebug();
}

void UHomingMissileWeaponComponent::UpdateRequestedTargetFromLocal()
{
	AActor* LocalTarget = nullptr;
	if (const UMissileTargetingComponent* Targeting = GetTargetingComponent())
	{
		LocalTarget = Targeting->GetCurrentTarget();
	}
	if (LastTargetSentToServer.Get() == LocalTarget)
	{
		return;
	}
	LastTargetSentToServer = LocalTarget;
	if (GetOwner()->HasAuthority())
	{
		RequestedTarget = LocalTarget;
	}
	else if (bLocalFireHeld || WeaponState == EHomingMissileWeaponState::FiringSalvos)
	{
		ServerUpdateRequestedTarget(LocalTarget);
	}
}

void UHomingMissileWeaponComponent::OnWeaponEquippedChanged()
{
	bLocalFireHeld = false;
	LastTargetSentToServer.Reset();
	if (UMissileTargetingComponent* Targeting = GetTargetingComponent())
	{
		Targeting->SetTargetingEnabled(IsWeaponEquipped());
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bServerFireHeld = false;
	RequestedTarget.Reset();
	if (WeaponState == EHomingMissileWeaponState::FiringSalvos)
	{
		EnterCooldown();
	}
}

void UHomingMissileWeaponComponent::StartEquipCooldown()
{
	const UJetStatsComponent* StatsComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	if (!GetWorld() || !StatsComponent)
	{
		return;
	}
	const double ReadyTime = GetServerTimeSeconds()
		+ FMath::Max(0.0f, StatsComponent->GetHomingMissileStats().Cooldown);
	WeaponState = EHomingMissileWeaponState::Cooldown;
	SalvosFired = 0;
	NextActionServerTime = FMath::Max(NextActionServerTime, ReadyTime);
	SequenceTarget.Reset();
}

void UHomingMissileWeaponComponent::StartSequence()
{
	if (!GetOwner()->HasAuthority() || WeaponState != EHomingMissileWeaponState::Ready
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
	if ((Health && Health->IsDead()) || !LeftSpawnPoint.IsValid() || !RightSpawnPoint.IsValid() || !MissileClass)
	{
		return;
	}

	SequenceTarget.Reset();
	SalvosFired = 0;
	WeaponState = EHomingMissileWeaponState::FiringSalvos;
	FireSalvo();
}

void UHomingMissileWeaponComponent::FireSalvo()
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent || !MissileClass)
	{
		EnterCooldown();
		return;
	}

	const FHomingMissileStats& Stats = StatsComponent->GetHomingMissileStats();
	const int32 MissileCount = FMath::Max(1, Stats.MissilesPerSalvo);
	const int32 LeftCount = (MissileCount + 1) / 2;
	const int32 RightCount = MissileCount / 2;
	FRandomStream RandomStream(FMath::Rand());
	int32 LeftIndex = 0;
	int32 RightIndex = 0;
	// A target belongs to one salvo, not to the entire trigger sequence. Already
	// launched missiles retain their assigned target; later salvos use the current lock.
	SequenceTarget = ValidateRequestedTarget(RequestedTarget.Get());
	AActor* SalvoTarget = GetLiveSequenceTarget();
	for (int32 Index = 0; Index < MissileCount; ++Index)
	{
		const bool bLeft = Index % 2 == 0;
		SpawnMissile(bLeft ? LeftSpawnPoint.Get() : RightSpawnPoint.Get(), bLeft,
			bLeft ? LeftIndex++ : RightIndex++, bLeft ? LeftCount : RightCount,
			RandomStream, SalvoTarget);
	}

	if (MissileLaunchSound && LeftSpawnPoint.IsValid() && RightSpawnPoint.IsValid())
	{
		const FVector SoundLocation =
			(LeftSpawnPoint->GetComponentLocation() + RightSpawnPoint->GetComponentLocation()) * 0.5f;
		MulticastPlaySalvoLaunchSound(SoundLocation);
	}

	++SalvosFired;
	if (SalvosFired >= FMath::Max(1, Stats.SalvoCount))
	{
		EnterCooldown();
	}
	else
	{
		WeaponState = EHomingMissileWeaponState::FiringSalvos;
		NextActionServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, Stats.SalvoInterval);
	}
}

void UHomingMissileWeaponComponent::SpawnMissile(USceneComponent* SpawnPoint, const bool bLeftSide,
	const int32 SideIndex, const int32 SideCount, FRandomStream& RandomStream, AActor* TargetActor)
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
	const FHomingMissileStats& Stats = StatsComponent->GetHomingMissileStats();
	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector Up = GetOwner()->GetActorUpVector();
	const float BaseSeparationRadius = FMath::Max(0.0f, Stats.SeparationRadius);

	// A salvo needs depth as well as side-to-side spread. A small randomized drop and
	// rear offset keep simultaneous missiles from forming a perfectly flat row.
	const float LaunchDrop = BaseSeparationRadius * RandomStream.FRandRange(0.20f, 0.55f);
	const float LaunchRearOffset = BaseSeparationRadius * RandomStream.FRandRange(0.0f, 0.45f);
	const FVector HardpointLocation = SpawnPoint->GetComponentLocation();
	const FVector StartLocation = HardpointLocation - Up * LaunchDrop - Forward * LaunchRearOffset;
	// Homing missiles form two independent launch groups: one centered on each hardpoint.
	// Barrage rockets intentionally keep using a single shared formation center.
	const FVector FormationCenter = HardpointLocation - Up * LaunchDrop;
	FRocketSeparationSettings SeparationSettings;
	SeparationSettings.SpreadAngleDegrees = Stats.SpreadAngleDegrees
		* RandomStream.FRandRange(1.60f, 2.40f);
	SeparationSettings.ForwardDistance = Stats.SeparationForwardDistance
		* RandomStream.FRandRange(0.82f, 1.18f);
	SeparationSettings.Radius = BaseSeparationRadius
		* RandomStream.FRandRange(1.60f, 2.20f);
	SeparationSettings.CurveStrength = FMath::Clamp(
		Stats.SeparationCurveStrength * RandomStream.FRandRange(0.85f, 1.15f), 0.05f, 0.75f);
	SeparationSettings.bUseFullDisk = true;
	const FRocketSeparationPath SeparationPath = RocketLaunchPattern::BuildSeparationPath(
		StartLocation, FormationCenter, Forward, GetOwner()->GetActorRightVector(),
		Up, bLeftSide, SideIndex, SideCount, RandomStream,
		SeparationSettings);

	FRocketLaunchData Data;
	Data.StartLocation = SeparationPath.StartLocation;
	Data.Direction = SeparationPath.Direction;
	Data.SeparationControlPoint1 = SeparationPath.ControlPoint1;
	Data.SeparationControlPoint2 = SeparationPath.ControlPoint2;
	Data.SeparationEndPoint = SeparationPath.EndPoint;
	Data.bUseSeparationCurve = SeparationPath.bUseCurve;
	Data.Speed = Stats.MissileSpeed;
	Data.Damage = Stats.MissileDamage;
	Data.MaxTravelDistance = Stats.MaxTravelDistance;
	Data.ProximityRadius = Stats.ProximityRadius;
	Data.ProximityCheckInterval = Stats.ProximityCheckInterval;
	Data.ServerStartTime = GetWorld()->GetTimeSeconds();
	Data.GuidanceMode = ERocketGuidanceMode::Homing;
	Data.HomingTarget = TargetActor;
	Data.MaxTurnRateDegreesPerSecond = Stats.MaxTurnRateDegreesPerSecond;
	Data.bDrawDebug = bDrawMissileDebug;

	const FVector InitialDirection =
		(SeparationPath.ControlPoint1 - SeparationPath.StartLocation).GetSafeNormal(
			SMALL_NUMBER, SeparationPath.Direction);
	const FTransform SpawnTransform(InitialDirection.Rotation(), SeparationPath.StartLocation);
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ARocketProjectile* Missile = GetWorld()->SpawnActorDeferred<ARocketProjectile>(MissileClass,
		SpawnTransform, GetOwner(), OwnerPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Missile)
	{
		Missile->InitializeRocket(Data);
		Missile->FinishSpawning(SpawnTransform);
	}
}

void UHomingMissileWeaponComponent::EnterCooldown()
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const float Cooldown = StatsComponent ? StatsComponent->GetHomingMissileStats().Cooldown : 0.0f;
	WeaponState = EHomingMissileWeaponState::Cooldown;
	NextActionServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, Cooldown);
	SequenceTarget.Reset();
}

AActor* UHomingMissileWeaponComponent::ValidateRequestedTarget(AActor* TargetActor) const
{
	if (!IsValid(TargetActor) || TargetActor == GetOwner())
	{
		return nullptr;
	}
	const UHealthComponent* TargetHealth = TargetActor->FindComponentByClass<UHealthComponent>();
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const URadarComponent* Radar = GetOwner()->FindComponentByClass<URadarComponent>();
	if (!TargetHealth || TargetHealth->IsDead() || !StatsComponent)
	{
		return nullptr;
	}

	const FHomingMissileStats& Stats = StatsComponent->GetHomingMissileStats();
	const FVector ToTarget = TargetActor->GetActorLocation() - GetOwner()->GetActorLocation();
	const bool bWithinRange = Radar
		? Radar->IsWithinMissileTargetingRange(TargetActor, Stats.LockRange, 1.15f)
		: ToTarget.SizeSquared() <= FMath::Square(Stats.LockRange * 1.15f);
	if (!bWithinRange)
	{
		return nullptr;
	}
	const float ServerToleranceAngle = FMath::Min(180.0f, Stats.LockAngleDegrees + 5.0f);
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(ServerToleranceAngle));
	return FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToTarget.GetSafeNormal()) >= MinimumDot
		? TargetActor : nullptr;
}

AActor* UHomingMissileWeaponComponent::GetLiveSequenceTarget() const
{
	AActor* TargetActor = SequenceTarget.Get();
	const UHealthComponent* Health = TargetActor
		? TargetActor->FindComponentByClass<UHealthComponent>() : nullptr;
	return Health && !Health->IsDead() ? TargetActor : nullptr;
}

UMissileTargetingComponent* UHomingMissileWeaponComponent::GetTargetingComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMissileTargetingComponent>() : nullptr;
}

void UHomingMissileWeaponComponent::MulticastPlaySalvoLaunchSound_Implementation(
	const FVector_NetQuantize Location)
{
	if (MissileLaunchSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, MissileLaunchSound, Location);
	}
}

double UHomingMissileWeaponComponent::GetServerTimeSeconds() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void UHomingMissileWeaponComponent::DrawWeaponDebug() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bShowWeaponDebug || !IsWeaponEquipped() || !OwnerPawn
		|| !OwnerPawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}
	const FHomingMissileStats& Stats = StatsComponent->GetHomingMissileStats();
	const TCHAR* StateText = WeaponState == EHomingMissileWeaponState::Ready ? TEXT("READY")
		: WeaponState == EHomingMissileWeaponState::FiringSalvos ? TEXT("FIRING") : TEXT("COOLDOWN");
	const float Remaining = FMath::Max(0.0f,
		static_cast<float>(NextActionServerTime - GetServerTimeSeconds()));
	const FString Message = FString::Printf(
		TEXT("HOMING MISSILE [%s] | %dx%d | Salvo %d/%d | Next %.2fs | Speed %.0f cm/s"),
		StateText, Stats.SalvoCount, Stats.MissilesPerSalvo, SalvosFired,
		Stats.SalvoCount, Remaining, Stats.MissileSpeed);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Cyan, Message);
}

void UHomingMissileWeaponComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UHomingMissileWeaponComponent, WeaponState, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHomingMissileWeaponComponent, SalvosFired, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHomingMissileWeaponComponent, NextActionServerTime, COND_OwnerOnly);
}
bool UHomingMissileWeaponComponent::GetCooldownStatus(float& OutRemainingSeconds,
	float& OutDurationSeconds) const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	OutDurationSeconds = Stats ? Stats->GetHomingMissileStats().Cooldown : 0.0f;
	OutRemainingSeconds = WeaponState == EHomingMissileWeaponState::Cooldown
		? FMath::Clamp(static_cast<float>(NextActionServerTime - GetServerTimeSeconds()),
			0.0f, OutDurationSeconds)
		: 0.0f;
	return OutDurationSeconds > UE_SMALL_NUMBER;
}
