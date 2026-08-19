#include "ShotgunGunComponent.h"

#include "ArcadeFlightComponent.h"
#include "HealthComponent.h"
#include "HitRewindComponent.h"
#include "GunAimAssistComponent.h"
#include "JetStatsComponent.h"
#include "RifleTracerVisual.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
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
	if (GetOwner()->HasAuthority() && bServerFireHeld
		&& (!Pawn || !Pawn->IsPlayerControlled()) && Now >= NextServerShotTime)
	{
		++ServerAiShotSequence;
		if (ServerAiShotSequence == 0)
		{
			++ServerAiShotSequence;
		}
		FireAuthoritativeBlast(ServerAiShotSequence,
			static_cast<uint8>((ServerAiShotSequence - 1) & 1));
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

void UShotgunGunComponent::StartEquipCooldown()
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	if (!GetWorld() || !Stats)
	{
		return;
	}
	const double ReadyTime = GetWorld()->GetTimeSeconds()
		+ 1.0 / FMath::Max(0.1f, Stats->GetShotgunStats().ShotsPerSecond);
	NextLocalShotTime = FMath::Max(NextLocalShotTime, ReadyTime);
	if (GetOwner()->HasAuthority())
	{
		NextServerShotTime = FMath::Max(NextServerShotTime, ReadyTime);
	}
}

void UShotgunGunComponent::FirePredictedBlast()
{
	++LocalShotSequence;
	if (LocalShotSequence == 0)
	{
		++LocalShotSequence;
	}
	const uint8 MuzzleIndex = static_cast<uint8>((LocalShotSequence - 1) & 1);
	TArray<FCombatImpactEvent> PelletImpacts;
	BuildPelletPaths(false, ShotgunTuning::SeedOffset + LocalShotSequence,
		LocalShotSequence, PelletImpacts);
	PredictedBlastImpacts.Add(LocalShotSequence, PelletImpacts);
	PredictedBlastImpacts.Remove(static_cast<uint16>(LocalShotSequence - 16));
	PlayBlastVisual(PelletImpacts, MuzzleIndex);

	if (GetOwner()->HasAuthority())
	{
		HandleServerFireBlast(LocalShotSequence, MuzzleIndex,
			LocalCameraOrigin, LocalCameraDirection, bLocalCameraAimEnabled,
			GetEstimatedServerTime());
	}
	else
	{
		ServerFireBlast(LocalShotSequence, MuzzleIndex,
			LocalCameraOrigin, LocalCameraDirection, bLocalCameraAimEnabled,
			GetEstimatedServerTime());
	}
}

void UShotgunGunComponent::ServerFireBlast_Implementation(
	const uint16 Sequence, const uint8 MuzzleIndex,
	const FVector_NetQuantize100 CameraOrigin,
	const FVector_NetQuantizeNormal CameraDirection, const bool bUseCameraAim,
	const double ClientFireServerTime)
{
	HandleServerFireBlast(Sequence, MuzzleIndex,
		CameraOrigin, CameraDirection, bUseCameraAim, ClientFireServerTime);
}

void UShotgunGunComponent::HandleServerFireBlast(
	const uint16 Sequence, const uint8 MuzzleIndex,
	const FVector& CameraOrigin, const FVector& CameraDirection, const bool bUseCameraAim,
	const double ClientFireServerTime)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsWeaponEquipped()
		|| IsOwnerFireBlocked())
	{
		return;
	}
	const UJetStatsComponent* Stats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	if (!Stats || (Health && Health->IsDead())
		|| (Flight && Flight->GetHoverState() != EArcadeHoverState::Flying))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now + 0.002 < NextServerShotTime)
	{
		return;
	}
	NextServerShotTime = Now
		+ 1.0 / FMath::Max(0.1f, Stats->GetShotgunStats().ShotsPerSecond);
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	ServerCameraOrigin = OwnerLocation + (CameraOrigin - OwnerLocation)
		.GetClampedToMaxSize(MaxCameraOriginDistance);
	ServerCameraDirection = CameraDirection.GetSafeNormal(
		UE_SMALL_NUMBER, GetOwner()->GetActorForwardVector());
	bServerCameraAimEnabled = bUseCameraAim;
	const double RewindTime = UHitRewindComponent::ClampRequestedServerTime(
		Now, ClientFireServerTime);
	FireAuthoritativeBlast(Sequence, MuzzleIndex & 1, RewindTime);
}

void UShotgunGunComponent::FireAuthoritativeBlast(
	const uint16 Sequence, const uint8 MuzzleIndex, const double RewindServerTime)
{
	TArray<FCombatImpactEvent> PelletImpacts;
	int32 PelletHits = 0;
	{
		FScopedHitboxRewind RewindScope(GetWorld(), RewindServerTime, GetOwner());
		PelletHits = BuildPelletPaths(
			true, ShotgunTuning::SeedOffset + Sequence, Sequence, PelletImpacts);
	}
	if (PelletImpacts.IsEmpty())
	{
		return;
	}
	++ServerBlastsFired;
	ServerPelletHits += PelletHits;
	MulticastPlayBlast(PelletImpacts, MuzzleIndex, Sequence);
}

double UShotgunGunComponent::GetEstimatedServerTime() const
{
	if (!GetWorld())
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		return UHitRewindComponent::EstimateClientViewServerTime(
			GetOwner(), GameState->GetServerWorldTimeSeconds());
	}
	return UHitRewindComponent::EstimateClientViewServerTime(
		GetOwner(), GetWorld()->GetTimeSeconds());
}

int32 UShotgunGunComponent::BuildPelletPaths(const bool bApplyDamage, const int32 ShotSeed,
	const uint16 Sequence, TArray<FCombatImpactEvent>& OutPelletImpacts) const
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent || (!LeftMuzzle.IsValid() && !RightMuzzle.IsValid()))
	{
		return 0;
	}

	const FShotgunStats& Stats = StatsComponent->GetShotgunStats();
	const int32 PelletCount = FMath::Max(1, Stats.PelletCount);
	OutPelletImpacts.Reserve(PelletCount);

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

		OutPelletImpacts.Add(bHit
			? FCombatImpactEvent::MakeFromHit(Sequence,
				static_cast<uint8>(PelletIndex), Hit, -PelletDirection)
			: FCombatImpactEvent::MakeMiss(Sequence,
				static_cast<uint8>(PelletIndex), PelletEnd, PelletDirection));
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
	const TArray<FCombatImpactEvent>& PelletImpacts,
	const uint8 MuzzleIndex, const uint16 Sequence)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		const TArray<FCombatImpactEvent>* Predicted = PredictedBlastImpacts.Find(Sequence);
		PlayConfirmedBlastImpacts(PelletImpacts, Predicted);
		PredictedBlastImpacts.Remove(Sequence);
		return;
	}
	PlayBlastVisual(PelletImpacts, MuzzleIndex);
}

void UShotgunGunComponent::PlayBlastVisual(
	const TArray<FCombatImpactEvent>& PelletImpacts, const uint8 MuzzleIndex) const
{
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!StatsComponent)
	{
		return;
	}
	const FShotgunStats& Stats = StatsComponent->GetShotgunStats();
	bool bPlayedImpactSound = false;

	for (const FCombatImpactEvent& Impact : PelletImpacts)
	{
		const USceneComponent* Muzzle = GetMuzzle(MuzzleIndex);
		const FVector Start = Muzzle ? Muzzle->GetComponentLocation() : GetOwner()->GetActorLocation();
		FVector End;
		FVector ImpactNormal;
		Impact.Resolve(End, ImpactNormal);
		const FVector Direction = (End - Start).GetSafeNormal();

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
		if (Impact.bBlockingHit && ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), ImpactEffect, End, ImpactNormal.Rotation());
		}
		if (Impact.bBlockingHit && !bPlayedImpactSound
			&& ShotgunImpactSound && GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, ShotgunImpactSound, End);
			bPlayedImpactSound = true;
		}
		if (bDrawPelletDebug)
		{
			DrawDebugLine(GetWorld(), Start, End,
				Impact.bBlockingHit ? FColor::Red : FColor::Yellow,
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

void UShotgunGunComponent::PlayConfirmedBlastImpacts(
	const TArray<FCombatImpactEvent>& PelletImpacts,
	const TArray<FCombatImpactEvent>* PredictedImpacts) const
{
	bool bPlayedImpactSound = false;
	for (const FCombatImpactEvent& Impact : PelletImpacts)
	{
		if (!Impact.bBlockingHit)
		{
			continue;
		}
		const FCombatImpactEvent* Predicted = PredictedImpacts
			? PredictedImpacts->FindByPredicate(
				[&Impact](const FCombatImpactEvent& Candidate)
				{
					return Candidate.SubIndex == Impact.SubIndex;
				})
			: nullptr;
		if (Predicted && Predicted->MatchesPredictedContact(Impact))
		{
			continue;
		}
		FVector End;
		FVector ImpactNormal;
		Impact.Resolve(End, ImpactNormal);
		if (ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), ImpactEffect, End, ImpactNormal.Rotation());
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
