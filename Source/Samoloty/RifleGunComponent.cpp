#include "RifleGunComponent.h"

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

URifleGunComponent::URifleGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetWeaponSlot(EWeaponSlot::Gun);
	SetWeaponDisplayName(TEXT("Rifle"));
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

void URifleGunComponent::SetFirePoints(USceneComponent* LeftPoint, USceneComponent* RightPoint)
{
	SetMuzzlePoints(LeftPoint, RightPoint);
}

void URifleGunComponent::SetAimContext(const FVector& AimOrigin, const FVector& AimDirection)
{
	if (!bLocalCameraAimEnabled)
	{
		SetCameraAimEnabled(true);
	}
	SetCameraAim(AimOrigin, AimDirection);
}

void URifleGunComponent::SetCameraAimEnabled(const bool bEnabled)
{
	bLocalCameraAimEnabled = bEnabled;
	if (GetOwner()->HasAuthority())
	{
		bServerCameraAimEnabled = bEnabled;
	}
}

void URifleGunComponent::SetCameraAim(const FVector& CameraOrigin, const FVector& CameraDirection)
{
	if (!bLocalCameraAimEnabled)
	{
		return;
	}
	LocalCameraOrigin = CameraOrigin;
	LocalCameraDirection = CameraDirection.GetSafeNormal();
	if (GetOwner()->HasAuthority())
	{
		ServerCameraOrigin = LocalCameraOrigin;
		ServerCameraDirection = LocalCameraDirection;
		bServerCameraAimEnabled = true;
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextAimUpdateTime)
	{
		NextAimUpdateTime = Now + 1.0 / FMath::Max(1.0f, AimUpdatesPerSecond);
		ServerSetCameraAim(LocalCameraOrigin, LocalCameraDirection);
	}
}

void URifleGunComponent::ServerSetCameraAim_Implementation(
	const FVector_NetQuantize100 CameraOrigin,
	const FVector_NetQuantizeNormal CameraDirection)
{
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	ServerCameraOrigin = OwnerLocation + (FVector(CameraOrigin) - OwnerLocation)
		.GetClampedToMaxSize(MaxCameraOriginDistance);
	ServerCameraDirection = FVector(CameraDirection).GetSafeNormal();
	bServerCameraAimEnabled = true;
}

void URifleGunComponent::SetFireHeld(const bool bHeld)
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

void URifleGunComponent::ServerSetFireHeld_Implementation(const bool bHeld)
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

void URifleGunComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
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
		DrawRifleDebug();
		return;
	}
	if (IsOwnerFireBlocked())
	{
		DrawRifleDebug();
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
	if (GetOwner()->HasAuthority() && bServerFireHeld
		&& (!Pawn || !Pawn->IsPlayerControlled()) && Now >= NextServerShotTime)
	{
		++ServerAiShotSequence;
		if (ServerAiShotSequence == 0)
		{
			++ServerAiShotSequence;
		}
		FireAuthoritativeShot(ServerAiShotSequence, ServerMuzzleIndex);
		NextServerShotTime = Now + ShotInterval;
	}
	DrawRifleDebug();
}

void URifleGunComponent::OnWeaponEquippedChanged()
{
	bLocalFireHeld = false;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bServerFireHeld = false;
	}
}

void URifleGunComponent::StartEquipCooldown()
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	if (!GetWorld() || !Stats)
	{
		return;
	}
	const double ReadyTime = GetWorld()->GetTimeSeconds()
		+ 1.0 / FMath::Max(0.1f, Stats->GetRifleGunStats().ShotsPerSecond);
	NextLocalShotTime = FMath::Max(NextLocalShotTime, ReadyTime);
	if (GetOwner()->HasAuthority())
	{
		NextServerShotTime = FMath::Max(NextServerShotTime, ReadyTime);
	}
}

void URifleGunComponent::FirePredictedShot()
{
	FVector Start, End, Direction;
	bool bHit = false;
	FHitResult Hit;
	if (!BuildShot(LocalMuzzleIndex, false, Start, End, Direction, bHit, Hit))
	{
		return;
	}
	++LocalShotSequence;
	if (LocalShotSequence == 0)
	{
		++LocalShotSequence;
	}
	const FCombatImpactEvent PredictedImpact = bHit
		? FCombatImpactEvent::MakeFromHit(LocalShotSequence, 0, Hit, -Direction)
		: FCombatImpactEvent::MakeMiss(LocalShotSequence, 0, End, Direction);
	PredictedImpacts.Add(LocalShotSequence, PredictedImpact);
	PredictedImpacts.Remove(static_cast<uint16>(LocalShotSequence - 64));
	PlayShotVisual(Start, Direction, LocalMuzzleIndex, PredictedImpact);

	if (GetOwner()->HasAuthority())
	{
		HandleServerFireShot(LocalShotSequence, LocalMuzzleIndex,
			LocalCameraOrigin, LocalCameraDirection, bLocalCameraAimEnabled,
			GetEstimatedServerTime());
	}
	else
	{
		ServerFireShot(LocalShotSequence, LocalMuzzleIndex,
			LocalCameraOrigin, LocalCameraDirection, bLocalCameraAimEnabled,
			GetEstimatedServerTime());
	}
	LocalMuzzleIndex ^= 1;
}

void URifleGunComponent::ServerFireShot_Implementation(
	const uint16 Sequence, const uint8 MuzzleIndex,
	const FVector_NetQuantize100 CameraOrigin,
	const FVector_NetQuantizeNormal CameraDirection, const bool bUseCameraAim,
	const double ClientFireServerTime)
{
	HandleServerFireShot(Sequence, MuzzleIndex, CameraOrigin, CameraDirection,
		bUseCameraAim, ClientFireServerTime);
}

void URifleGunComponent::HandleServerFireShot(
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
	const double ShotInterval =
		1.0 / FMath::Max(0.1f, Stats->GetRifleGunStats().ShotsPerSecond);
	const bool bHasAcceptedClientShot = LastAcceptedClientFireServerTime > 0.0;
	const bool bSequenceIsNewer = !bHasAcceptedClientShot
		|| static_cast<int16>(Sequence - LastAcceptedClientShotSequence) > 0;
	// Validate the cadence in the client's synchronized server-time domain. At
	// high ping/jitter several unreliable RPCs may arrive in one server frame;
	// arrival-time throttling incorrectly discarded every shot after the first.
	// Network delay must not invalidate the shot itself. The requested rewind is
	// independently clamped below, so a late packet never receives more than the
	// configured compensation window.
	const bool bClientTimeIsSane = ClientFireServerTime <= Now + 0.050;
	const bool bCadenceIsValid = !bHasAcceptedClientShot
		|| ClientFireServerTime + 0.002 >= LastAcceptedClientFireServerTime + ShotInterval;
	const bool bEquipCooldownComplete = Now + 0.002 >= NextServerShotTime;
	if (!bSequenceIsNewer || !bClientTimeIsSane || !bCadenceIsValid
		|| !bEquipCooldownComplete)
	{
		++ServerRejectedShots;
		return;
	}
	LastAcceptedClientShotSequence = Sequence;
	LastAcceptedClientFireServerTime = ClientFireServerTime;
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	ServerCameraOrigin = OwnerLocation + (CameraOrigin - OwnerLocation)
		.GetClampedToMaxSize(MaxCameraOriginDistance);
	ServerCameraDirection = CameraDirection.GetSafeNormal(
		UE_SMALL_NUMBER, GetOwner()->GetActorForwardVector());
	bServerCameraAimEnabled = bUseCameraAim;
	const double RewindTime = UHitRewindComponent::ClampRequestedServerTime(
		Now, ClientFireServerTime);
	FireAuthoritativeShot(Sequence, MuzzleIndex & 1, RewindTime);
}

void URifleGunComponent::FireAuthoritativeShot(
	const uint16 Sequence, const uint8 MuzzleIndex, const double RewindServerTime)
{
	FVector Start, End, Direction;
	bool bHit = false;
	FHitResult Hit;
	FCombatImpactEvent Impact;
	{
		FScopedHitboxRewind RewindScope(GetWorld(), RewindServerTime, GetOwner());
		if (!BuildShot(MuzzleIndex, true, Start, End, Direction, bHit, Hit))
		{
			return;
		}
		Impact = bHit
			? FCombatImpactEvent::MakeFromHit(Sequence, 0, Hit, -Direction)
			: FCombatImpactEvent::MakeMiss(Sequence, 0, End, Direction);
	}
	++ServerShotsFired;
	ServerHits += bHit ? 1 : 0;
	MulticastPlayShot(Start, Direction, MuzzleIndex, Impact);
	ServerMuzzleIndex = MuzzleIndex ^ 1;
}

double URifleGunComponent::GetEstimatedServerTime() const
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

bool URifleGunComponent::BuildShot(const uint8 MuzzleIndex, const bool bApplyDamage,
	FVector& OutStart, FVector& OutEnd, FVector& OutDirection,
	bool& bOutHit, FHitResult& OutHit) const
{
	const USceneComponent* Muzzle = GetMuzzle(MuzzleIndex);
	const UJetStatsComponent* StatsComponent = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!Muzzle || !StatsComponent)
	{
		return false;
	}
	const FRifleGunStats& Stats = StatsComponent->GetRifleGunStats();
	OutStart = Muzzle->GetComponentLocation();
	const bool bUseCameraAim = bApplyDamage ? bServerCameraAimEnabled : bLocalCameraAimEnabled;
	FHitResult& Hit = OutHit;
	Hit = FHitResult();
	if (bUseCameraAim)
	{
		const FVector CameraOrigin = bApplyDamage ? ServerCameraOrigin : LocalCameraOrigin;
		const FVector RequestedDirection = bApplyDamage ? ServerCameraDirection : LocalCameraDirection;
		FVector CameraDirection = RequestedDirection.GetSafeNormal();
		if (const UGunAimAssistComponent* AimAssist =
			GetOwner()->FindComponentByClass<UGunAimAssistComponent>())
		{
			FVector AssistedAimPoint;
			if (AimAssist->FindAssistedAimPoint(
				CameraOrigin, CameraDirection, Stats.MaxRange, AssistedAimPoint))
			{
				CameraDirection = (AssistedAimPoint - CameraOrigin).GetSafeNormal();
			}
		}
		FVector AimPoint = FindCameraAimPoint(
			CameraOrigin, CameraDirection, Stats.MaxRange, Hit);
		const float HitForwardDistance = FVector::DotProduct(
			AimPoint - GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());
		if (Hit.bBlockingHit && HitForwardDistance < MinimumCameraHitForwardDistance)
		{
			// A target between the chase camera and the aircraft is behind the fixed guns.
			// Keep firing along the camera direction instead of bending tracers backwards.
			Hit = FHitResult();
			AimPoint = CameraOrigin + CameraDirection * Stats.MaxRange;
		}
		OutDirection = (AimPoint - OutStart).GetSafeNormal();
		OutEnd = AimPoint;
		bOutHit = Hit.bBlockingHit;
		if (bDrawShotDebug)
		{
			DrawDebugLine(GetWorld(), CameraOrigin, CameraOrigin + RequestedDirection * 20000.0f,
				FColor::Blue, false, 0.1f, 0, 1.0f);
		}
	}
	else
	{
		OutDirection = FMath::VRandCone(Muzzle->GetForwardVector(),
			FMath::DegreesToRadians(Stats.SpreadAngleDegrees));
		OutEnd = OutStart + OutDirection * Stats.MaxRange;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RifleGunTrace), true, GetOwner());
		bOutHit = GetWorld()->LineTraceSingleByChannel(
			Hit, OutStart, OutEnd, ECC_Visibility, QueryParams);
		if (bOutHit)
		{
			OutEnd = Hit.ImpactPoint;
		}
	}
	if (bOutHit)
	{
		if (bApplyDamage && Hit.GetActor())
		{
			const APawn* OwnerPawn = Cast<APawn>(GetOwner());
			const APawn* HitPawn = Cast<APawn>(Hit.GetActor());
			const bool bMayDamage = !bDamagePlayersOnly
				|| (HitPawn && HitPawn->IsPlayerControlled());
			if (bMayDamage)
			{
				UGameplayStatics::ApplyDamage(Hit.GetActor(), Stats.Damage,
					OwnerPawn ? OwnerPawn->GetController() : nullptr, GetOwner(), nullptr);
			}
		}
	}
	return true;
}

FVector URifleGunComponent::FindCameraAimPoint(const FVector& CameraOrigin,
	const FVector& CameraDirection, const float MaxRange, FHitResult& OutHit) const
{
	FCollisionQueryParams CameraParams(SCENE_QUERY_STAT(RifleCameraTrace), true, GetOwner());
	const FVector CameraEnd = CameraOrigin + CameraDirection * MaxRange;
	if (GetWorld()->LineTraceSingleByChannel(OutHit, CameraOrigin, CameraEnd,
		ECC_Visibility, CameraParams))
	{
		return OutHit.ImpactPoint;
	}
	return CameraEnd;
}

void URifleGunComponent::MulticastPlayShot_Implementation(
	const FVector_NetQuantize Start,
	const FVector_NetQuantizeNormal Direction, const uint8 MuzzleIndex,
	const FCombatImpactEvent& Impact)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		const FCombatImpactEvent* Predicted = PredictedImpacts.Find(Impact.Sequence);
		const bool bAlreadyPresented = Predicted
			&& Predicted->MatchesPredictedContact(Impact);
		if (Impact.bBlockingHit && !bAlreadyPresented)
		{
			PlayImpactVisual(Impact);
		}
		PredictedImpacts.Remove(Impact.Sequence);
		return;
	}
	PlayShotVisual(Start, Direction, MuzzleIndex, Impact);
}

void URifleGunComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URifleGunComponent, ServerShotsFired, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URifleGunComponent, ServerHits, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URifleGunComponent, ServerRejectedShots, COND_OwnerOnly);
}

void URifleGunComponent::PlayShotVisual(const FVector& Start,
	const FVector& Direction, const uint8 MuzzleIndex,
	const FCombatImpactEvent& Impact) const
{
	const UJetStatsComponent* Stats = GetOwner()->FindComponentByClass<UJetStatsComponent>();
	if (!Stats)
	{
		return;
	}
	FVector End;
	FVector ImpactNormal;
	Impact.Resolve(End, ImpactNormal);
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
	if (Impact.bBlockingHit)
	{
		PlayImpactVisual(Impact);
	}
	if (bDrawShotDebug)
	{
		DrawDebugLine(GetWorld(), Start, End,
			Impact.bBlockingHit ? FColor::Red : FColor::Green,
			false, 0.25f, 0, 1.0f);
	}
}

void URifleGunComponent::PlayImpactVisual(const FCombatImpactEvent& Impact) const
{
	FVector ImpactPoint;
	FVector ImpactNormal;
	Impact.Resolve(ImpactPoint, ImpactNormal);
	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ImpactEffect, ImpactPoint, ImpactNormal.Rotation());
	}
	if (RifleImpactSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, RifleImpactSound, ImpactPoint);
	}
}

USceneComponent* URifleGunComponent::GetMuzzle(const uint8 MuzzleIndex) const
{
	return MuzzleIndex == 0 ? LeftMuzzle.Get() : RightMuzzle.Get();
}

void URifleGunComponent::DrawRifleDebug() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsWeaponEquipped() || !bShowRifleDebug || !Pawn || !Pawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}
	const FString Message = FString::Printf(
		TEXT("RIFLE: %s | Server shots: %d | hits: %d | rejected: %d"),
		bLocalFireHeld ? TEXT("FIRING") : TEXT("READY"), ServerShotsFired, ServerHits,
		ServerRejectedShots);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Cyan, Message);
}
bool URifleGunComponent::GetCooldownStatus(float& OutRemainingSeconds,
	float& OutDurationSeconds) const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	OutDurationSeconds = Stats
		? 1.0f / FMath::Max(0.1f, Stats->GetRifleGunStats().ShotsPerSecond) : 0.0f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	OutRemainingSeconds = FMath::Clamp(
		static_cast<float>(NextLocalShotTime - Now), 0.0f, OutDurationSeconds);
	return OutDurationSeconds > UE_SMALL_NUMBER;
}
