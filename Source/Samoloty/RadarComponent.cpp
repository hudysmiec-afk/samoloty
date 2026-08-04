#include "RadarComponent.h"

#include "HealthComponent.h"
#include "JetStatsComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

URadarComponent::URadarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void URadarComponent::BeginPlay()
{
	Super::BeginPlay();
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		ScanContacts();
	}
}

void URadarComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	ScanAccumulator += DeltaTime;
	if (ScanAccumulator >= FMath::Max(0.05f, ScanInterval))
	{
		ScanAccumulator = 0.0f;
		ScanContacts();
	}
	DrawRadarDebug();
}

void URadarComponent::ScanContacts()
{
	DetectedContacts.Reset();
	if (!GetWorld() || !GetOwner())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (IsDetectableTarget(Candidate))
		{
			DetectedContacts.Add(Candidate);
		}
	}
}

bool URadarComponent::IsDetected(const AActor* Candidate) const
{
	return Candidate && DetectedContacts.ContainsByPredicate(
		[Candidate](const TWeakObjectPtr<AActor>& Contact)
		{
			return Contact.Get() == Candidate;
		});
}

bool URadarComponent::IsDetectableTarget(const AActor* Candidate, const float RangeTolerance) const
{
	if (!GetOwner() || !IsValid(Candidate) || Candidate == GetOwner()
		|| Candidate->IsActorBeingDestroyed())
	{
		return false;
	}

	const UHealthComponent* Health = Candidate->FindComponentByClass<UHealthComponent>();
	if (!Health || Health->IsDead())
	{
		return false;
	}

	const float EffectiveRange = GetDetectionRange() * FMath::Max(0.0f, RangeTolerance);
	const float DistanceSquared = FVector::DistSquared(
		GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
	return DistanceSquared > KINDA_SMALL_NUMBER
		&& DistanceSquared <= FMath::Square(EffectiveRange);
}

bool URadarComponent::IsWithinMissileTargetingRange(const AActor* Candidate,
	const float WeaponRange, const float RangeTolerance) const
{
	if (!IsDetectableTarget(Candidate, RangeTolerance))
	{
		return false;
	}
	const float AllowedRange = FMath::Min(
		FMath::Max(0.0f, WeaponRange), GetMissileTargetingRange())
		* FMath::Max(0.0f, RangeTolerance);
	return FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation())
		<= FMath::Square(AllowedRange);
}

ERadarAltitudeRelation URadarComponent::GetAltitudeRelation(const AActor* Candidate) const
{
	if (!GetOwner() || !Candidate)
	{
		return ERadarAltitudeRelation::Level;
	}
	const float DeltaZ = Candidate->GetActorLocation().Z - GetOwner()->GetActorLocation().Z;
	if (DeltaZ > AltitudeThreshold)
	{
		return ERadarAltitudeRelation::Above;
	}
	if (DeltaZ < -AltitudeThreshold)
	{
		return ERadarAltitudeRelation::Below;
	}
	return ERadarAltitudeRelation::Level;
}

float URadarComponent::GetDetectionRange() const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	return Stats ? FMath::Max(0.0f, Stats->GetRadarStats().DetectionRange) : 0.0f;
}

float URadarComponent::GetGunTargetingRange() const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	return Stats ? FMath::Max(0.0f, Stats->GetRadarStats().GunTargetingRange) : 0.0f;
}

float URadarComponent::GetMissileTargetingRange() const
{
	const UJetStatsComponent* Stats = GetOwner()
		? GetOwner()->FindComponentByClass<UJetStatsComponent>() : nullptr;
	return Stats ? FMath::Max(0.0f, Stats->GetRadarStats().MissileTargetingRange) : 0.0f;
}

void URadarComponent::DrawRadarDebug() const
{
	if (!bShowRadarDebug || !GEngine || !GetOwner())
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Cyan,
		FString::Printf(TEXT("RADAR: %d contacts | %.0f m"), DetectedContacts.Num(),
			GetDetectionRange() / 100.0f));
	DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation(), GetDetectionRange(),
		48, FColor::Cyan, false, 0.0f, 0, 2.0f);
}
