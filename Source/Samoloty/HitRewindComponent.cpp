#include "HitRewindComponent.h"

#include "AircraftCollisionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

TArray<TWeakObjectPtr<UHitRewindComponent>> UHitRewindComponent::Registry;

UHitRewindComponent::UHitRewindComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

double UHitRewindComponent::EstimateClientViewServerTime(
	const AActor* Viewer, const double CurrentEstimatedServerTime)
{
	// A listen server renders authoritative actors without the simulated-proxy
	// interpolation delay.
	if (!Viewer || Viewer->HasAuthority())
	{
		return CurrentEstimatedServerTime;
	}

	double RoundTripSeconds = 0.0;
	if (const APawn* Pawn = Cast<APawn>(Viewer))
	{
		if (const APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			RoundTripSeconds = FMath::Max(
				0.0, static_cast<double>(PlayerState->GetPingInMilliseconds()) / 1000.0);
		}
	}

	const double VisibleStateAge = FMath::Min(
		MaxCompensatedViewAgeSeconds,
		InterpolationBufferSeconds + RoundTripSeconds * 0.5);
	return CurrentEstimatedServerTime - VisibleStateAge;
}

double UHitRewindComponent::ClampRequestedServerTime(
	const double ServerNow, const double RequestedServerTime)
{
	return FMath::Clamp(
		RequestedServerTime,
		ServerNow - MaxServerRewindSeconds,
		ServerNow);
}

void UHitRewindComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(GetOwner());
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (Component
			&& Component->ComponentHasTag(UAircraftCollisionComponent::DamageHitboxTag))
		{
			DamageHitboxes.Add(Component);
		}
	}
	if (!DamageHitboxes.IsEmpty())
	{
		Registry.AddUnique(this);
		RecordSnapshot();
	}
}

void UHitRewindComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Registry.RemoveAll([this](const TWeakObjectPtr<UHitRewindComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == this;
	});
	Super::EndPlay(EndPlayReason);
}

void UHitRewindComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RecordSnapshot();
}

void UHitRewindComponent::RecordSnapshot()
{
	if (!GetWorld() || DamageHitboxes.IsEmpty())
	{
		return;
	}
	FSnapshot& Snapshot = History.AddDefaulted_GetRef();
	Snapshot.ServerTime = GetWorld()->GetTimeSeconds();
	Snapshot.HitboxTransforms.Reserve(DamageHitboxes.Num());
	for (const UPrimitiveComponent* Hitbox : DamageHitboxes)
	{
		Snapshot.HitboxTransforms.Add(
			Hitbox ? Hitbox->GetComponentTransform() : FTransform::Identity);
	}
	const double OldestAllowedTime = Snapshot.ServerTime - HistoryDuration;
	while (History.Num() > 2 && History[1].ServerTime < OldestAllowedTime)
	{
		History.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

bool UHitRewindComponent::BuildSnapshotAt(
	const double ServerTime, TArray<FTransform>& OutTransforms) const
{
	if (History.IsEmpty() || DamageHitboxes.IsEmpty())
	{
		return false;
	}
	// A shot newer than the last completed history sample should use the target's
	// current transform. Reapplying the previous PostPhysics sample would add an
	// artificial one-frame rewind for listen-server and very low-latency players.
	if (ServerTime >= History.Last().ServerTime)
	{
		return false;
	}
	const FSnapshot* From = &History[0];
	const FSnapshot* To = From;
	for (int32 Index = 1; Index < History.Num(); ++Index)
	{
		To = &History[Index];
		if (To->ServerTime >= ServerTime)
		{
			break;
		}
		From = To;
	}
	const double Span = To->ServerTime - From->ServerTime;
	const float Alpha = Span > UE_SMALL_NUMBER
		? FMath::Clamp(static_cast<float>((ServerTime - From->ServerTime) / Span), 0.0f, 1.0f)
		: 0.0f;
	const int32 Count = FMath::Min3(
		DamageHitboxes.Num(), From->HitboxTransforms.Num(), To->HitboxTransforms.Num());
	OutTransforms.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutTransforms[Index].Blend(
			From->HitboxTransforms[Index], To->HitboxTransforms[Index], Alpha);
	}
	return Count > 0;
}

FScopedHitboxRewind::FScopedHitboxRewind(
	UWorld* World, const double ServerTime, const AActor* IgnoredActor)
{
	if (!World || ServerTime <= 0.0)
	{
		return;
	}
	UHitRewindComponent::Registry.RemoveAll(
		[](const TWeakObjectPtr<UHitRewindComponent>& Entry)
		{
			return !Entry.IsValid();
		});
	for (const TWeakObjectPtr<UHitRewindComponent>& Entry
		: UHitRewindComponent::Registry)
	{
		UHitRewindComponent* Rewind = Entry.Get();
		if (!Rewind || Rewind->GetWorld() != World
			|| Rewind->GetOwner() == IgnoredActor)
		{
			continue;
		}
		TArray<FTransform> HistoricalTransforms;
		if (!Rewind->BuildSnapshotAt(ServerTime, HistoricalTransforms))
		{
			continue;
		}
		const int32 Count = FMath::Min(
			Rewind->DamageHitboxes.Num(), HistoricalTransforms.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UPrimitiveComponent* Hitbox = Rewind->DamageHitboxes[Index];
			if (!IsValid(Hitbox))
			{
				continue;
			}
			FRestoreEntry& Restore = RestoreEntries.AddDefaulted_GetRef();
			Restore.Component = Hitbox;
			Restore.Transform = Hitbox->GetComponentTransform();
			Hitbox->SetWorldTransform(HistoricalTransforms[Index], false, nullptr,
				ETeleportType::TeleportPhysics);
		}
	}
}

FScopedHitboxRewind::~FScopedHitboxRewind()
{
	for (const FRestoreEntry& Restore : RestoreEntries)
	{
		if (UPrimitiveComponent* Component = Restore.Component.Get())
		{
			Component->SetWorldTransform(Restore.Transform, false, nullptr,
				ETeleportType::TeleportPhysics);
		}
	}
}
