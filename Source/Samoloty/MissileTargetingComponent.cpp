#include "MissileTargetingComponent.h"

#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "RadarComponent.h"
#include "TargetSelectionComponent.h"
#include "TargetIntentComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UMissileTargetingComponent::UMissileTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UMissileTargetingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bTargetingEnabled || !OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	ScanAccumulator += DeltaTime;
	if (ScanAccumulator >= FMath::Max(0.02f, TargetScanInterval))
	{
		ScanAccumulator = 0.0f;
		ScanTargets();
	}
	DrawTargetingDebug();
}

void UMissileTargetingComponent::SetTargetingEnabled(const bool bEnabled)
{
	if (bTargetingEnabled == bEnabled)
	{
		return;
	}
	bTargetingEnabled = bEnabled;
	ScanAccumulator = 0.0f;
	if (!bTargetingEnabled)
	{
		SetCurrentTarget(nullptr);
		return;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()); OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		ScanTargets();
	}
}

void UMissileTargetingComponent::RejectCurrentTarget()
{
	if (UTargetSelectionComponent* Selection = GetOwner()
		? GetOwner()->FindComponentByClass<UTargetSelectionComponent>() : nullptr)
	{
		Selection->CycleSelectedTarget();
	}
}

void UMissileTargetingComponent::ScanTargets()
{
	const FHomingMissileStats* Stats = GetStats();
	if (!Stats || !GetWorld() || !GetOwner())
	{
		SetCurrentTarget(nullptr);
		return;
	}

	const UTargetSelectionComponent* Selection =
		GetOwner()->FindComponentByClass<UTargetSelectionComponent>();
	AActor* SelectedTarget = Selection ? Selection->GetSelectedTarget() : nullptr;
	SetCurrentTarget(IsEligibleTarget(SelectedTarget, *Stats) ? SelectedTarget : nullptr);
}

void UMissileTargetingComponent::SetCurrentTarget(AActor* NewTarget)
{
	AActor* PreviousTarget = CurrentTarget.Get();
	if (PreviousTarget == NewTarget)
	{
		return;
	}
	CurrentTarget = NewTarget;
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (GetOwner()->HasAuthority())
		{
			SetAuthoritativeTargetIntent(NewTarget);
		}
		else
		{
			ServerSetLockedTarget(NewTarget);
		}
	}
	OnTargetChanged.Broadcast(PreviousTarget, NewTarget);
}

void UMissileTargetingComponent::ServerSetLockedTarget_Implementation(AActor* NewTarget)
{
	SetAuthoritativeTargetIntent(NewTarget);
}

void UMissileTargetingComponent::SetAuthoritativeTargetIntent(AActor* NewTarget)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FHomingMissileStats* Stats = GetStats();
	AActor* ValidatedTarget = Stats && IsEligibleAuthoritativeTarget(NewTarget, *Stats)
		? NewTarget : nullptr;
	if (UTargetIntentComponent* TargetIntent =
		GetOwner()->FindComponentByClass<UTargetIntentComponent>())
	{
		TargetIntent->SetCurrentTarget(ValidatedTarget);
	}
}

bool UMissileTargetingComponent::IsEligibleTarget(const AActor* Candidate,
	const FHomingMissileStats& Stats) const
{
	if (!IsValid(Candidate) || Candidate == GetOwner() || Candidate->IsActorBeingDestroyed())
	{
		return false;
	}
	const UHealthComponent* Health = Candidate->FindComponentByClass<UHealthComponent>();
	if (!Health || Health->IsDead())
	{
		return false;
	}
	const URadarComponent* Radar = GetOwner()->FindComponentByClass<URadarComponent>();
	if (!Radar || !Radar->IsDetected(Candidate)
		|| !Radar->IsWithinMissileTargetingRange(Candidate, Stats.LockRange))
	{
		return false;
	}

	const FVector ToTarget = Candidate->GetActorLocation() - GetOwner()->GetActorLocation();
	const float DistanceSquared = ToTarget.SizeSquared();
	if (DistanceSquared <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(Stats.LockAngleDegrees));
	return FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToTarget.GetSafeNormal()) >= MinimumDot;
}

bool UMissileTargetingComponent::IsEligibleAuthoritativeTarget(const AActor* Candidate,
	const FHomingMissileStats& Stats) const
{
	if (!IsValid(Candidate) || Candidate == GetOwner() || Candidate->IsActorBeingDestroyed())
	{
		return false;
	}
	const UHealthComponent* Health = Candidate->FindComponentByClass<UHealthComponent>();
	if (!Health || Health->IsDead())
	{
		return false;
	}
	const UTargetSelectionComponent* Selection = GetOwner()
		? GetOwner()->FindComponentByClass<UTargetSelectionComponent>() : nullptr;
	if (!Selection || Selection->GetSelectedTargetForAimAssist() != Candidate)
	{
		return false;
	}
	const URadarComponent* Radar = GetOwner()->FindComponentByClass<URadarComponent>();
	if (!Radar || !Radar->IsWithinMissileTargetingRange(Candidate, Stats.LockRange))
	{
		return false;
	}
	const FVector ToTarget = Candidate->GetActorLocation() - GetOwner()->GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(Stats.LockAngleDegrees));
	return FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToTarget.GetSafeNormal())
		>= MinimumDot;
}

const FHomingMissileStats* UMissileTargetingComponent::GetStats() const
{
	const UJetStatsComponent* StatsComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	return StatsComponent ? &StatsComponent->GetHomingMissileStats() : nullptr;
}

void UMissileTargetingComponent::DrawTargetingDebug() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bShowTargetingDebug || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}

	const AActor* Target = CurrentTarget.Get();
	const float DistanceMeters = Target
		? FVector::Distance(GetOwner()->GetActorLocation(), Target->GetActorLocation()) / 100.0f : 0.0f;
	const FString Message = Target
		? FString::Printf(TEXT("MISSILE LOCK: %s | %.0f m | Shift: next target"),
			*Target->GetName(), DistanceMeters)
		: TEXT("MISSILE LOCK: NO TARGET");
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f,
		Target ? FColor::Green : FColor::Silver, Message);

	if (Target)
	{
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), Target->GetActorLocation(),
			FColor::Green, false, 0.0f, 0, 1.5f);
		DrawDebugSphere(GetWorld(), Target->GetActorLocation(), 250.0f, 16,
			FColor::Green, false, 0.0f, 0, 2.0f);
	}
}
