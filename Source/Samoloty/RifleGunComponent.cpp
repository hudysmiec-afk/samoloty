#include "RifleGunComponent.h"

#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RifleTracerVisual.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

URifleGunComponent::URifleGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	TracerVisualClass = ARifleTracerVisual::StaticClass();
}

void URifleGunComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URifleGunComponent::SetMuzzlePoints(USceneComponent* InLeftMuzzle, USceneComponent* InRightMuzzle)
{
	LeftMuzzle = InLeftMuzzle;
	RightMuzzle = InRightMuzzle;
}

void URifleGunComponent::SetFireHeld(const bool bHeld)
{
	bLocalFireHeld = bHeld;
	if (bHeld)
	{
		NextLocalShotTime = 0.0;
	}
	if (GetOwner()->HasAuthority())
	{
		bServerFireHeld = bHeld;
		if (bHeld)
		{
			NextServerShotTime = 0.0;
		}
	}
	else
	{
		ServerSetFireHeld(bHeld);
	}
}

void URifleGunComponent::ServerSetFireHeld_Implementation(const bool bHeld)
{
	bServerFireHeld = bHeld;
	if (bHeld)
	{
		NextServerShotTime = 0.0;
	}
}

void URifleGunComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UJetStatsComponent* Stats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	if (!Stats || (Health && Health->IsDead()))
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const double ShotInterval = 1.0 / FMath::Max(0.1f, Stats->GetRifleGunStats().ShotsPerSecond);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled() && bLocalFireHeld && Now >= NextLocalShotTime)
	{
		FirePredictedShot();
		NextLocalShotTime = Now + ShotInterval;
	}
	if (GetOwner()->HasAuthority() && bServerFireHeld && Now >= NextServerShotTime)
	{
		FireAuthoritativeShot();
		NextServerShotTime = Now + ShotInterval;
	}
	DrawRifleDebug();
}

void URifleGunComponent::FirePredictedShot()
{
	FVector Start, End, Direction;
	bool bHit = false;
	BuildShot(LocalMuzzleIndex, false, Start, End, Direction, bHit);
	PlayShotVisual(Start, End, Direction, LocalMuzzleIndex, bHit);
	LocalMuzzleIndex ^= 1;
}

void URifleGunComponent::FireAuthoritativeShot()
{
	FVector Start, End, Direction;
	bool bHit = false;
	if (!BuildShot(ServerMuzzleIndex, true, Start, End, Direction, bHit))
	{
		return;
	}
	++ServerShotsFired;
	ServerHits += bHit ? 1 : 0;
	MulticastPlayShot(Start, End, Direction, ServerMuzzleIndex, bHit);
	ServerMuzzleIndex ^= 1;
}

bool URifleGunComponent::BuildShot(const uint8 MuzzleIndex, const bool bApplyDamage,
	FVector& OutStart, FVector& OutEnd, FVector& OutDirection, bool& bOutHit) const
{
	const USceneComponent* Muzzle = GetMuzzle(MuzzleIndex);
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!Muzzle || !StatsComponent)
	{
		return false;
	}
	const FRifleGunStats& Stats = StatsComponent->GetRifleGunStats();
	OutStart = Muzzle->GetComponentLocation();
	OutDirection = FMath::VRandCone(Muzzle->GetForwardVector(),
		FMath::DegreesToRadians(Stats.SpreadAngleDegrees));
	OutEnd = OutStart + OutDirection * Stats.MaxRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RifleGunTrace), true, GetOwner());
	FHitResult Hit;
	bOutHit = GetWorld()->LineTraceSingleByChannel(Hit, OutStart, OutEnd, ECC_Visibility, QueryParams);
	if (bOutHit)
	{
		OutEnd = Hit.ImpactPoint;
		if (bApplyDamage && Hit.GetActor())
		{
			const APawn* OwnerPawn = Cast<APawn>(GetOwner());
			UGameplayStatics::ApplyDamage(Hit.GetActor(), Stats.Damage,
				OwnerPawn ? OwnerPawn->GetController() : nullptr, GetOwner(), nullptr);
		}
	}
	return true;
}

void URifleGunComponent::MulticastPlayShot_Implementation(const FVector_NetQuantize Start,
	const FVector_NetQuantize End, const FVector_NetQuantizeNormal Direction,
	const uint8 MuzzleIndex, const bool bHit)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		return; // Owner already played an immediate predicted visual.
	}
	PlayShotVisual(Start, End, Direction, MuzzleIndex, bHit);
}

void URifleGunComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URifleGunComponent, ServerShotsFired, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URifleGunComponent, ServerHits, COND_OwnerOnly);
}

void URifleGunComponent::PlayShotVisual(const FVector& Start, const FVector& End,
	const FVector& Direction, const uint8 MuzzleIndex, const bool bHit) const
{
	const UJetStatsComponent* Stats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!Stats)
	{
		return;
	}
	if (TracerVisualClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ARifleTracerVisual* Tracer = GetWorld()->SpawnActor<ARifleTracerVisual>(TracerVisualClass,
			Start, Direction.Rotation(), Params))
		{
			const FRifleGunStats& RifleStats = Stats->GetRifleGunStats();
			Tracer->InitializeTracer(Start, End, RifleStats.TracerSpeed, RifleStats.TracerLength,
				RifleStats.TracerWidth);
		}
	}
	if (MuzzleFlashEffect)
	{
		if (USceneComponent* Muzzle = GetMuzzle(MuzzleIndex))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				MuzzleFlashEffect,
				Muzzle,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true,
				true,
				ENCPoolMethod::None,
				true);
		}
	}
	if (RifleFireSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, RifleFireSound, Start);
	}
	if (bHit && ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactEffect, End, (-Direction).Rotation());
	}
	if (bHit && RifleImpactSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, RifleImpactSound, End);
	}
	if (bDrawShotDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.25f, 0, 1.0f);
	}
}

USceneComponent* URifleGunComponent::GetMuzzle(const uint8 MuzzleIndex) const
{
	return MuzzleIndex == 0 ? LeftMuzzle.Get() : RightMuzzle.Get();
}

void URifleGunComponent::DrawRifleDebug() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bShowRifleDebug || !Pawn || !Pawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}
	const FString Message = FString::Printf(TEXT("RIFLE: %s | Server shots: %d | hits: %d"),
		bLocalFireHeld ? TEXT("FIRING") : TEXT("READY"), ServerShotsFired, ServerHits);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Cyan, Message);
}
