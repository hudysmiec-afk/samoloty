#include "RocketSimulationManager.h"

#include "EvasiveRollComponent.h"
#include "HealthComponent.h"
#include "RocketSimulationReplicator.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	bool IsEvadingManagedRocket(const AActor* Actor)
	{
		const UEvasiveRollComponent* Roll = Actor
			? Actor->FindComponentByClass<UEvasiveRollComponent>() : nullptr;
		return Roll && Roll->IsEvadingMissiles();
	}
}

void URocketSimulationManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() != NM_Client)
	{
		ARocketSimulationReplicator* EventReplicator =
			InWorld.SpawnActor<ARocketSimulationReplicator>();
		if (EventReplicator)
		{
			Replicator = EventReplicator;
		}
	}
}

void URocketSimulationManager::Tick(const float DeltaTime)
{
	for (int32 Handle = 0; Handle < Records.Num(); ++Handle)
	{
		FManagedRocketRecord& Record = Records[Handle];
		if (!Record.bActive)
		{
			continue;
		}
		if (Record.bDataOnly)
		{
			SimulateDataOnlyRocket(Handle, DeltaTime);
		}
		else if (ARocketProjectile* Projectile = Record.Projectile.Get())
		{
			Projectile->SimulateFromManager(DeltaTime);
		}
		else
		{
			UnregisterRocket(Handle);
		}
	}
	if (ARocketSimulationReplicator* EventReplicator = Replicator.Get())
	{
		EventReplicator->BroadcastLaunchBatch(PendingLaunchEvents);
		EventReplicator->BroadcastExplosionBatch(PendingExplosionEvents);
	}
	PendingLaunchEvents.Reset();
	PendingExplosionEvents.Reset();
}

TStatId URocketSimulationManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URocketSimulationManager, STATGROUP_Tickables);
}

int32 URocketSimulationManager::RegisterRocket(ARocketProjectile* Projectile,
	const FRocketLaunchData& LaunchData, AActor* Owner)
{
	const int32 Handle = !FreeHandles.IsEmpty()
		? FreeHandles.Pop(EAllowShrinking::No) : Records.AddDefaulted();
	FManagedRocketRecord& Record = Records[Handle];
	Record.Projectile = Projectile;
	Record.Owner = Owner;
	Record.Location = LaunchData.StartLocation;
	Record.Direction = LaunchData.Direction;
	Record.Speed = LaunchData.Speed;
	Record.Damage = LaunchData.Damage;
	Record.MaxTravelDistance = LaunchData.MaxTravelDistance;
	Record.ProximityRadius = LaunchData.ProximityRadius;
	Record.bActive = true;
	++ActiveRocketCount;
	return Handle;
}

int32 URocketSimulationManager::LaunchDataOnlyStraightRocket(
	TSubclassOf<ARocketProjectile> RocketClass, const FRocketLaunchData& LaunchData,
	AActor* Owner, APawn* Instigator)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !RocketClass
		|| LaunchData.GuidanceMode != ERocketGuidanceMode::Straight
		|| !Replicator.IsValid())
	{
		return INDEX_NONE;
	}
	const int32 Handle = !FreeHandles.IsEmpty()
		? FreeHandles.Pop(EAllowShrinking::No) : Records.AddDefaulted();
	FManagedRocketRecord& Record = Records[Handle];
	Record.Owner = Owner;
	Record.InstigatorController = Instigator ? Instigator->GetController() : nullptr;
	Record.RocketClass = RocketClass;
	Record.StartLocation = LaunchData.StartLocation;
	Record.ControlPoint1 = LaunchData.SeparationControlPoint1;
	Record.ControlPoint2 = LaunchData.SeparationControlPoint2;
	Record.SeparationEndPoint = LaunchData.SeparationEndPoint;
	Record.Location = LaunchData.StartLocation;
	Record.Direction = FVector(LaunchData.Direction).GetSafeNormal();
	Record.Speed = LaunchData.Speed;
	Record.Damage = LaunchData.Damage;
	Record.MaxTravelDistance = LaunchData.MaxTravelDistance;
	Record.ProximityRadius = LaunchData.ProximityRadius;
	Record.ProximityCheckInterval = FMath::Max(0.001f, LaunchData.ProximityCheckInterval);
	Record.PhysicalCollisionRadius = RocketClass->GetDefaultObject<ARocketProjectile>()
		->GetPhysicalCollisionRadius();
	Record.bUseSeparationCurve = LaunchData.bUseSeparationCurve;
	Record.bDrawDebug = LaunchData.bDrawDebug;
	Record.bDataOnly = true;
	Record.bActive = true;
	Record.RocketId = NextRocketId++;
	if (Record.RocketId == 0)
	{
		Record.RocketId = NextRocketId++;
	}
	BuildCurveCache(Record);
	++ActiveRocketCount;
	FManagedRocketLaunchEvent& Event = PendingLaunchEvents.AddDefaulted_GetRef();
	Event.RocketId = Record.RocketId;
	Event.RocketClass = RocketClass;
	Event.LaunchData = LaunchData;
	return Handle;
}

void URocketSimulationManager::BuildCurveCache(FManagedRocketRecord& Record)
{
	Record.SeparationCurveLength = 0.0f;
	Record.CurveCumulativeDistances[0] = 0.0f;
	if (!Record.bUseSeparationCurve)
	{
		return;
	}
	FVector Previous = EvaluateCurve(Record, 0.0f);
	for (int32 Index = 1; Index <= FManagedRocketRecord::CurveSamples; ++Index)
	{
		const FVector Point = EvaluateCurve(Record,
			static_cast<float>(Index) / FManagedRocketRecord::CurveSamples);
		Record.SeparationCurveLength += FVector::Distance(Previous, Point);
		Record.CurveCumulativeDistances[Index] = Record.SeparationCurveLength;
		Previous = Point;
	}
}

FVector URocketSimulationManager::EvaluateCurve(
	const FManagedRocketRecord& Record, const float Parameter) const
{
	const float T = FMath::Clamp(Parameter, 0.0f, 1.0f);
	const float O = 1.0f - T;
	return Record.StartLocation * (O * O * O)
		+ Record.ControlPoint1 * (3.0f * O * O * T)
		+ Record.ControlPoint2 * (3.0f * O * T * T)
		+ Record.SeparationEndPoint * (T * T * T);
}

float URocketSimulationManager::FindCurveParameter(
	const FManagedRocketRecord& Record, const float Distance) const
{
	if (!Record.bUseSeparationCurve || Record.SeparationCurveLength <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	const float Clamped = FMath::Clamp(Distance, 0.0f, Record.SeparationCurveLength);
	for (int32 Index = 1; Index <= FManagedRocketRecord::CurveSamples; ++Index)
	{
		if (Record.CurveCumulativeDistances[Index] >= Clamped)
		{
			const float Previous = Record.CurveCumulativeDistances[Index - 1];
			const float Span = Record.CurveCumulativeDistances[Index] - Previous;
			const float Alpha = Span > KINDA_SMALL_NUMBER ? (Clamped - Previous) / Span : 0.0f;
			return (static_cast<float>(Index - 1) + Alpha) / FManagedRocketRecord::CurveSamples;
		}
	}
	return 1.0f;
}

FVector URocketSimulationManager::GetLocationAtDistance(
	const FManagedRocketRecord& Record, const float Distance) const
{
	if (Record.bUseSeparationCurve && Distance < Record.SeparationCurveLength)
	{
		return EvaluateCurve(Record, FindCurveParameter(Record, Distance));
	}
	const FVector Start = Record.bUseSeparationCurve
		? Record.SeparationEndPoint : Record.StartLocation;
	const float StraightDistance = Record.bUseSeparationCurve
		? FMath::Max(0.0f, Distance - Record.SeparationCurveLength) : Distance;
	return Start + Record.Direction * StraightDistance;
}

void URocketSimulationManager::SimulateDataOnlyRocket(
	const int32 Handle, const float DeltaTime)
{
	if (!Records.IsValidIndex(Handle) || !Records[Handle].bActive)
	{
		return;
	}
	FManagedRocketRecord& Record = Records[Handle];
	const FVector Start = Record.Location;
	const float Remaining = FMath::Max(0.0f,
		Record.MaxTravelDistance - Record.DistanceTraveled);
	const float Step = FMath::Min(Record.Speed * DeltaTime, Remaining);
	const float NewDistance = Record.DistanceTraveled + Step;
	const FVector End = GetLocationAtDistance(Record, NewDistance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ManagedRocketCollision), false);
	Params.AddIgnoredActor(Record.Owner.Get());
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(Record.PhysicalCollisionRadius), Params)
		&& !IsEvadingManagedRocket(Hit.GetActor()))
	{
		AActor* DamageTarget = Hit.GetActor()
			&& Hit.GetActor()->FindComponentByClass<UHealthComponent>() ? Hit.GetActor() : nullptr;
		ExplodeDataOnlyRocket(Handle, DamageTarget, Hit.ImpactPoint, &Hit);
		return;
	}
	Record.TimeSinceProximityCheck += DeltaTime;
	if (Record.ProximityRadius > 0.0f
		&& Record.TimeSinceProximityCheck >= Record.ProximityCheckInterval)
	{
		Record.TimeSinceProximityCheck = 0.0f;
		FCollisionObjectQueryParams Types;
		Types.AddObjectTypesToQuery(ECC_Pawn);
		Types.AddObjectTypesToQuery(ECC_WorldDynamic);
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, End, FQuat::Identity, Types,
			FCollisionShape::MakeSphere(Record.ProximityRadius), Params);
		AActor* Closest = nullptr;
		float ClosestDistance = TNumericLimits<float>::Max();
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Candidate = Overlap.GetActor();
			const UHealthComponent* Health = Candidate
				? Candidate->FindComponentByClass<UHealthComponent>() : nullptr;
			if (!Health || Health->IsDead() || IsEvadingManagedRocket(Candidate))
			{
				continue;
			}
			const float DistanceSquared = FVector::DistSquared(End, Candidate->GetActorLocation());
			if (DistanceSquared < ClosestDistance)
			{
				ClosestDistance = DistanceSquared;
				Closest = Candidate;
			}
		}
		if (Closest)
		{
			ExplodeDataOnlyRocket(Handle, Closest, End, nullptr);
			return;
		}
	}
	Record.Location = End;
	Record.DistanceTraveled = NewDistance;
	if (Record.bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Cyan, false, 0.15f, 0, 1.0f);
	}
	if (NewDistance >= Record.MaxTravelDistance - KINDA_SMALL_NUMBER)
	{
		ExplodeDataOnlyRocket(Handle, nullptr, End, nullptr);
	}
}

void URocketSimulationManager::ExplodeDataOnlyRocket(const int32 Handle,
	AActor* DamageTarget, const FVector& ImpactLocation, const FHitResult* PhysicalHit)
{
	if (!Records.IsValidIndex(Handle) || !Records[Handle].bActive)
	{
		return;
	}
	FManagedRocketRecord& Record = Records[Handle];
	const FVector Direction = Record.Direction.GetSafeNormal();
	const FCombatImpactEvent Impact = PhysicalHit
		? FCombatImpactEvent::MakeFromHit(1, 0, *PhysicalHit, -Direction)
		: (DamageTarget
			? FCombatImpactEvent::MakeOnTarget(1, 0, DamageTarget, ImpactLocation,
				(ImpactLocation - DamageTarget->GetActorLocation()).GetSafeNormal(
					UE_SMALL_NUMBER, -Direction))
			: FCombatImpactEvent::MakeMiss(1, 0, ImpactLocation, Direction));
	if (DamageTarget)
	{
		UGameplayStatics::ApplyDamage(DamageTarget, Record.Damage,
			Record.InstigatorController.Get(), Record.Owner.Get(), nullptr);
	}
	FManagedRocketExplosionEvent& Event = PendingExplosionEvents.AddDefaulted_GetRef();
	Event.RocketId = Record.RocketId;
	Event.Impact = Impact;
	UnregisterRocket(Handle);
}

void URocketSimulationManager::UnregisterRocket(const int32 Handle)
{
	if (!Records.IsValidIndex(Handle) || !Records[Handle].bActive)
	{
		return;
	}
	Records[Handle] = FManagedRocketRecord();
	FreeHandles.Add(Handle);
	ActiveRocketCount = FMath::Max(0, ActiveRocketCount - 1);
}
