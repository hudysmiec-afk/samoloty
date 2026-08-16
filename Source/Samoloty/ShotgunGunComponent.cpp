#include "ShotgunGunComponent.h"

#include "ArcadeFlightComponent.h"
#include "HealthComponent.h"
#include "GunAimAssistComponent.h"
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

namespace ShotgunTuning
{
	constexpr int32 SeedOffset = 17389;
}

UShotgunGunComponent::UShotgunGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetWeaponSlot(EWeaponSlot::Gun);
	SetWeaponDisplayName(TEXT("Shotgun"));
	TracerVisualClass = ARifleTracerVisual::StaticClass();
}

void UShotgunGunComponent::SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint)
{
	LeftMuzzle = LeftPoint;
	RightMuzzle = RightPoint;
}

void UShotgunGunComponent::SetAimContext(const FVector& AimOrigin, const FVector& AimDirection)
{
	bLocalCameraAimEnabled = true;
	LocalCameraOrigin = AimOrigin;
	LocalCameraDirection = AimDirection.GetSafeNormal();
	if (GetOwner()->HasAuthority())
	{
		bServerCameraAimEnabled = true;
		ServerCameraOrigin = LocalCameraOrigin;
		ServerCameraDirection = LocalCameraDirection;
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextAimUpdateTime)
	{
		NextAimUpdateTime = Now + 1.0 / FMath::Max(1.0f, AimUpdatesPerSecond);
		ServerSetCameraAim(LocalCameraOrigin, LocalCameraDirection);
	}
}

void UShotgunGunComponent::ServerSetCameraAim_Implementation(
	const FVector_NetQuantize100 CameraOrigin,
	const FVector_NetQuantizeNormal CameraDirection)
{
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	ServerCameraOrigin = OwnerLocation + (FVector(CameraOrigin) - OwnerLocation)
		.GetClampedToMaxSize(MaxCameraOriginDistance);
	ServerCameraDirection = FVector(CameraDirection).GetSafeNormal();
	bServerCameraAimEnabled = true;
}

void UShotgunGunComponent::SetFireHeld(const bool bHeld)
{
	if (bHeld && !IsWeaponEquipped())
	{
		return;
	}
	bLocalFireHeld = bHeld;

	if (GetOwner()->HasAuthority())
	{
		bServerFireHeld = bHeld;
	}
	else
	{
		ServerSetFireHeld(bHeld);
	}
}

void UShotgunGunComponent::ServerSetFireHeld_Implementation(const bool bHeld)
{
	if (bHeld && !IsWeaponEquipped())
	{
		return;
	}
	const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	if (bHeld && Flight && Flight->GetHoverState() != EArcadeHoverState::Flying)
	{
		bServerFireHeld = false;
		return;
	}
	bServerFireHeld = bHeld;
}

void UShotgunGunComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsWeaponEquipped())
	{
		return;
	}

	const UJetStatsComponent* Stats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	if (!Stats || (Health && Health->IsDead())
		|| (Flight && Flight->GetHoverState() != EArcadeHoverState::Flying))
	{
		if (Flight && Flight->GetHoverState() != EArcadeHoverState::Flying)
		{
			bLocalFireHeld = false;
			if (GetOwner()->HasAuthority())
			{
				bServerFireHeld = false;
			}
		}
		DrawShotgunDebug();
		return;
	}
	if (IsOwnerFireBlocked())
	{
		DrawShotgunDebug();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const double ShotInterval = 1.0 / FMath::Max(0.1f, Stats->GetShotgunStats().ShotsPerSecond);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled() && bLocalFireHeld && Now >= NextLocalShotTime)
	{
		FirePredictedBlast();
		NextLocalShotTime = Now + ShotInterval;
	}
	if (GetOwner()->HasAuthority() && bServerFireHeld && Now >= NextServerShotTime)
	{
		FireAuthoritativeBlast();
		NextServerShotTime = Now + ShotInterval;
	}
	DrawShotgunDebug();
}

void UShotgunGunComponent::OnWeaponEquippedChanged()
{
	bLocalFireHeld = false;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bServerFireHeld = false;
	}
}

void UShotgunGunComponent::FirePredictedBlast()
{
	TArray<FVector_NetQuantize> PelletEnds;
	TArray<uint8> HitFlags;
	const int32 ShotSequence = LocalShotSequence++;
	const uint8 MuzzleIndex = static_cast<uint8>(ShotSequence & 1);
	BuildPelletPaths(false, ShotgunTuning::SeedOffset + ShotSequence, PelletEnds, HitFlags);
	// Keep the weapon responsive, but never predict pellet impacts. Those are shown
	// only after the server confirms its authoritative traces.
	TArray<uint8> NoPredictedHits;
	NoPredictedHits.Init(0, PelletEnds.Num());
	PlayBlastVisual(PelletEnds, NoPredictedHits, MuzzleIndex);
}

void UShotgunGunComponent::FireAuthoritativeBlast()
{
	TArray<FVector_NetQuantize> PelletEnds;
	TArray<uint8> HitFlags;
	const int32 ShotSequence = ServerShotSequence++;
	const uint8 MuzzleIndex = static_cast<uint8>(ShotSequence & 1);
	const int32 PelletHits = BuildPelletPaths(
		true, ShotgunTuning::SeedOffset + ShotSequence, PelletEnds, HitFlags);
	if (PelletEnds.IsEmpty())
	{
		return;
	}
	++ServerBlastsFired;
	ServerPelletHits += PelletHits;
	MulticastPlayBlast(PelletEnds, HitFlags, MuzzleIndex);
}

int32 UShotgunGunComponent::BuildPelletPaths(const bool bApplyDamage, const int32 ShotSeed,
	TArray<FVector_NetQuantize>& OutPelletEnds, TArray<uint8>& OutHitFlags) const
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent || (!LeftMuzzle.IsValid() && !RightMuzzle.IsValid()))
	{
		return 0;
	}

	const FShotgunStats& Stats = StatsComponent->GetShotgunStats();
	const int32 PelletCount = FMath::Max(1, Stats.PelletCount);
	OutPelletEnds.Reserve(PelletCount);
	OutHitFlags.Reserve(PelletCount);

	const bool bUseCameraAim = bApplyDamage ? bServerCameraAimEnabled : bLocalCameraAimEnabled;
	const FVector AimOrigin = bUseCameraAim
		? (bApplyDamage ? ServerCameraOrigin : LocalCameraOrigin)
		: GetOwner()->GetActorLocation();
	FVector AimDirection = bUseCameraAim
		? (bApplyDamage ? ServerCameraDirection : LocalCameraDirection).GetSafeNormal()
		: GetOwner()->GetActorForwardVector();
	if (const UGunAimAssistComponent* AimAssist =
		GetOwner()->FindComponentByClass<UGunAimAssistComponent>())
	{
		FVector AssistedAimPoint;
		if (AimAssist->FindAssistedAimPoint(
			AimOrigin, AimDirection, Stats.MaxRange, AssistedAimPoint))
		{
			AimDirection = (AssistedAimPoint - AimOrigin).GetSafeNormal();
		}
	}
	const float SpreadRadians = FMath::DegreesToRadians(Stats.SpreadAngleDegrees);
	FRandomStream RandomStream(ShotSeed);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShotgunTrace), true, GetOwner());
	TMap<AActor*, float> DamageByActor;
	int32 PelletHits = 0;

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const FVector PelletDirection = RandomStream.VRandCone(AimDirection, SpreadRadians);
		const FVector TraceEnd = AimOrigin + PelletDirection * Stats.MaxRange;
		FHitResult Hit;
		bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit, AimOrigin, TraceEnd, ECC_Visibility, QueryParams);
		FVector PelletEnd = bHit ? Hit.ImpactPoint : TraceEnd;
		if (bHit)
		{
			const float ForwardDistance = FVector::DotProduct(
				PelletEnd - GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());
			if (ForwardDistance < MinimumCameraHitForwardDistance)
			{
				bHit = false;
				PelletEnd = TraceEnd;
			}
		}

		OutPelletEnds.Add(PelletEnd);
		OutHitFlags.Add(bHit ? 1 : 0);
		if (bHit)
		{
			++PelletHits;
			if (bApplyDamage && Hit.GetActor())
			{
				DamageByActor.FindOrAdd(Hit.GetActor()) += Stats.DamagePerPellet;
			}
		}
	}

	if (bApplyDamage)
	{
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		for (const TPair<AActor*, float>& DamageEntry : DamageByActor)
		{
			if (IsValid(DamageEntry.Key))
			{
				UGameplayStatics::ApplyDamage(DamageEntry.Key, DamageEntry.Value,
					OwnerPawn ? OwnerPawn->GetController() : nullptr, GetOwner(), nullptr);
			}
		}
	}
	return PelletHits;
}

void UShotgunGunComponent::MulticastPlayBlast_Implementation(
	const TArray<FVector_NetQuantize>& PelletEnds, const TArray<uint8>& HitFlags,
	const uint8 MuzzleIndex)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		// Local tracers, muzzle flash and fire audio have already been predicted.
		// Only add authoritative pellet impacts to avoid duplicate shot cosmetics.
		PlayBlastImpacts(PelletEnds, HitFlags, MuzzleIndex);
		return;
	}
	PlayBlastVisual(PelletEnds, HitFlags, MuzzleIndex);
}

void UShotgunGunComponent::PlayBlastVisual(const TArray<FVector_NetQuantize>& PelletEnds,
	const TArray<uint8>& HitFlags, const uint8 MuzzleIndex) const
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}
	const FShotgunStats& Stats = StatsComponent->GetShotgunStats();
	bool bPlayedImpactSound = false;

	for (int32 PelletIndex = 0; PelletIndex < PelletEnds.Num(); ++PelletIndex)
	{
		const USceneComponent* Muzzle = GetMuzzle(MuzzleIndex);
		const FVector Start = Muzzle ? Muzzle->GetComponentLocation() : GetOwner()->GetActorLocation();
		const FVector End = FVector(PelletEnds[PelletIndex]);
		const FVector Direction = (End - Start).GetSafeNormal();
		const bool bHit = HitFlags.IsValidIndex(PelletIndex) && HitFlags[PelletIndex] != 0;

		if (TracerVisualClass)
		{
			FActorSpawnParameters Params;
			Params.Owner = GetOwner();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ARifleTracerVisual* Tracer = GetWorld()->SpawnActor<ARifleTracerVisual>(
				TracerVisualClass, Start, Direction.Rotation(), Params))
			{
				Tracer->InitializeTracer(Start, End, Stats.TracerSpeed,
					Stats.TracerLength, Stats.TracerWidth);
			}
		}
		if (bHit && ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), ImpactEffect, End, (-Direction).Rotation());
		}
		if (bHit && !bPlayedImpactSound && ShotgunImpactSound && GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, ShotgunImpactSound, End);
			bPlayedImpactSound = true;
		}
		if (bDrawPelletDebug)
		{
			DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Yellow,
				false, 0.2f, 0, 1.0f);
		}
	}

	if (MuzzleFlashEffect)
	{
		if (USceneComponent* Muzzle = GetMuzzle(MuzzleIndex))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				MuzzleFlashEffect, Muzzle, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, true, true, ENCPoolMethod::None, true);
		}
	}

	if (ShotgunFireSound && GetNetMode() != NM_DedicatedServer)
	{
		const USceneComponent* Muzzle = GetMuzzle(MuzzleIndex);
		const FVector SoundLocation = Muzzle
			? Muzzle->GetComponentLocation() : GetOwner()->GetActorLocation();
		UGameplayStatics::SpawnSoundAtLocation(this, ShotgunFireSound, SoundLocation);
	}
}

void UShotgunGunComponent::PlayBlastImpacts(
	const TArray<FVector_NetQuantize>& PelletEnds,
	const TArray<uint8>& HitFlags, const uint8 MuzzleIndex) const
{
	bool bPlayedImpactSound = false;
	const USceneComponent* Muzzle = GetMuzzle(MuzzleIndex);
	const FVector Start = Muzzle
		? Muzzle->GetComponentLocation() : GetOwner()->GetActorLocation();

	for (int32 PelletIndex = 0; PelletIndex < PelletEnds.Num(); ++PelletIndex)
	{
		if (!HitFlags.IsValidIndex(PelletIndex) || HitFlags[PelletIndex] == 0)
		{
			continue;
		}

		const FVector End = FVector(PelletEnds[PelletIndex]);
		const FVector Direction = (End - Start).GetSafeNormal();
		if (ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), ImpactEffect, End, (-Direction).Rotation());
		}
		if (!bPlayedImpactSound && ShotgunImpactSound
			&& GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, ShotgunImpactSound, End);
			bPlayedImpactSound = true;
		}
	}
}

USceneComponent* UShotgunGunComponent::GetMuzzle(const uint8 MuzzleIndex) const
{
	USceneComponent* Preferred = MuzzleIndex == 0 ? LeftMuzzle.Get() : RightMuzzle.Get();
	return Preferred ? Preferred : (MuzzleIndex == 0 ? RightMuzzle.Get() : LeftMuzzle.Get());
}

void UShotgunGunComponent::DrawShotgunDebug() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsWeaponEquipped() || !bShowShotgunDebug || !Pawn || !Pawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}
	const FString Message = FString::Printf(
		TEXT("SHOTGUN: %s | Server blasts: %d | pellet hits: %d"),
		bLocalFireHeld ? TEXT("FIRING") : TEXT("READY"), ServerBlastsFired, ServerPelletHits);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Green, Message);
}

void UShotgunGunComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UShotgunGunComponent, ServerBlastsFired, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UShotgunGunComponent, ServerPelletHits, COND_OwnerOnly);
}
bool UShotgunGunComponent::GetCooldownStatus(float& OutRemainingSeconds,
	float& OutDurationSeconds) const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	OutDurationSeconds = Stats
		? 1.0f / FMath::Max(0.1f, Stats->GetShotgunStats().ShotsPerSecond) : 0.0f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	OutRemainingSeconds = FMath::Clamp(
		static_cast<float>(NextLocalShotTime - Now), 0.0f, OutDurationSeconds);
	return OutDurationSeconds > UE_SMALL_NUMBER;
}
