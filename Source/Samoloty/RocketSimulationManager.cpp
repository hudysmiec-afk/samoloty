#include "RocketSimulationManager.h"

#include "EvasiveRollComponent.h"
#include "HealthComponent.h"
#include "RocketSimulationReplicator.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	TAutoConsoleVariable<int32> CVarRocketPerfLog(
		TEXT("samoloty.Rockets.PerfLog"), 0,
		TEXT("Logs managed-rocket server and visual telemetry. 0=off, 1=on."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRocketPerfLogInterval(
		TEXT("samoloty.Rockets.PerfLogInterval"), 1.0f,
		TEXT("Seconds between managed-rocket telemetry summaries."),
		ECVF_Default);

	constexpr int32 LargeLaunchBatchSoftLimit = 400;
	constexpr int32 MaxTerminalHomingCorrectionsPerTick = 32;
	constexpr float HomingTerminalCorrectionLeadSeconds = 0.75f;

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
	const double TickStartSeconds = FPlatformTime::Seconds();
	bPerfLoggingThisTick = CVarRocketPerfLog.GetValueOnGameThread() != 0;
	if (!bPerfLoggingThisTick)
	{
		PerfCounters.Reset(TickStartSeconds);
	}
	else if (PerfCounters.WindowStartSeconds <= 0.0)
	{
		PerfCounters.Reset(TickStartSeconds);
	}
	TerminalHomingCorrectionsThisTick = 0;

	for (int32 Handle = 0; Handle < Records.Num(); ++Handle)
	{
		FManagedRocketRecord& Record = Records[Handle];
		if (!Record.bActive)
		{
			continue;
		}
		if (Record.bDataOnly)
		{
			if (bPerfLoggingThisTick)
			{
				++PerfCounters.DataOnlySimulations;
			}
			SimulateDataOnlyRocket(Handle, DeltaTime);
		}
		else if (ARocketProjectile* Projectile = Record.Projectile.Get())
		{
			if (bPerfLoggingThisTick)
			{
				++PerfCounters.ActorSimulations;
			}
			Projectile->SimulateFromManager(DeltaTime);
		}
		else
		{
			UnregisterRocket(Handle);
		}
	}
	const int32 LaunchBatchSize = PendingLaunchEvents.Num();
	const int32 ExplosionBatchSize = PendingExplosionEvents.Num();
	const int32 HomingUpdateBatchSize = PendingHomingUpdateEvents.Num();
	if (bPerfLoggingThisTick)
	{
		PerfCounters.LaunchEvents += LaunchBatchSize;
		PerfCounters.ExplosionEvents += ExplosionBatchSize;
		PerfCounters.HomingUpdateEvents += HomingUpdateBatchSize;
		PerfCounters.MaxLaunchBatch = FMath::Max(
			PerfCounters.MaxLaunchBatch, LaunchBatchSize);
		PerfCounters.MaxUpdateBatch = FMath::Max(PerfCounters.MaxUpdateBatch,
			ExplosionBatchSize + HomingUpdateBatchSize);
		if (LaunchBatchSize > 0)
		{
			++PerfCounters.LaunchBatches;
			if (LaunchBatchSize > LargeLaunchBatchSoftLimit)
			{
				++PerfCounters.LargeLaunchBatches;
			}
		}
		if (ExplosionBatchSize > 0 || HomingUpdateBatchSize > 0)
		{
			++PerfCounters.UpdateBatches;
		}
	}
	if (ARocketSimulationReplicator* EventReplicator = Replicator.Get())
	{
		EventReplicator->BroadcastLaunchBatch(PendingLaunchEvents);
		EventReplicator->BroadcastUpdateBatch(
			PendingExplosionEvents, PendingHomingUpdateEvents);
	}
	PendingLaunchEvents.Reset();
	PendingExplosionEvents.Reset();
	PendingHomingUpdateEvents.Reset();
	if (bPerfLoggingThisTick)
	{
		FinishPerfTick(TickStartSeconds);
	}
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
	Record.PathDirection = FVector(LaunchData.Direction).GetSafeNormal();
	Record.Direction = Record.PathDirection;
	Record.Speed = LaunchData.Speed;
	Record.Damage = LaunchData.Damage;
	Record.MaxTravelDistance = LaunchData.MaxTravelDistance;
	Record.ProximityRadius = LaunchData.ProximityRadius;
	Record.bActive = true;
	++ActiveRocketCount;
	return Handle;
}

int32 URocketSimulationManager::LaunchDataOnlyRocket(
	TSubclassOf<ARocketProjectile> RocketClass, const FRocketLaunchData& LaunchData,
	AActor* Owner, APawn* Instigator, UObject* SourceWeapon)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !RocketClass
		|| !Replicator.IsValid())
	{
		return INDEX_NONE;
	}
	const int32 Handle = !FreeHandles.IsEmpty()
		? FreeHandles.Pop(EAllowShrinking::No) : Records.AddDefaulted();
	FManagedRocketRecord& Record = Records[Handle];
	Record.Owner = Owner;
	Record.SourceWeapon = SourceWeapon;
	Record.InstigatorController = Instigator ? Instigator->GetController() : nullptr;
	Record.RocketClass = RocketClass;
	Record.HomingTarget = LaunchData.HomingTarget;
	Record.StartLocation = LaunchData.StartLocation;
	Record.ControlPoint1 = LaunchData.SeparationControlPoint1;
	Record.ControlPoint2 = LaunchData.SeparationControlPoint2;
	Record.SeparationEndPoint = LaunchData.SeparationEndPoint;
	Record.Location = LaunchData.StartLocation;
	Record.PathDirection = FVector(LaunchData.Direction).GetSafeNormal();
	Record.Direction = Record.PathDirection;
	Record.Speed = LaunchData.Speed;
	Record.Damage = LaunchData.Damage;
	Record.MaxTravelDistance = LaunchData.MaxTravelDistance;
	Record.ProximityRadius = LaunchData.ProximityRadius;
	Record.ProximityCheckInterval = FMath::Max(0.001f, LaunchData.ProximityCheckInterval);
	Record.PhysicalCollisionRadius = RocketClass->GetDefaultObject<ARocketProjectile>()
		->GetPhysicalCollisionRadius();
	Record.MaxTurnRateDegreesPerSecond = LaunchData.MaxTurnRateDegreesPerSecond;
	Record.GuidanceMode = LaunchData.GuidanceMode;
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

int32 URocketSimulationManager::GetActiveRocketCountForSource(
	const UObject* SourceWeapon) const
{
	int32 Count = 0;
	for (const FManagedRocketRecord& Record : Records)
	{
		if (Record.bActive && Record.bDataOnly
			&& Record.SourceWeapon.Get() == SourceWeapon)
		{
			++Count;
		}
	}
	return Count;
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
	return Start + Record.PathDirection * StraightDistance;
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
	const bool bFollowingLaunchCurve = Record.bUseSeparationCurve
		&& Record.DistanceTraveled < Record.SeparationCurveLength;
	FVector MovementDirection = Record.Direction.GetSafeNormal();
	FVector End;
	if (Record.GuidanceMode == ERocketGuidanceMode::Homing && !bFollowingLaunchCurve)
	{
		if (const AActor* Target = GetLiveHomingTarget(Record))
		{
			const FVector DesiredDirection =
				(Target->GetActorLocation() - Start).GetSafeNormal(SMALL_NUMBER, MovementDirection);
			MovementDirection = RotateDirectionTowards(MovementDirection, DesiredDirection,
				Record.MaxTurnRateDegreesPerSecond, DeltaTime);
		}
		End = Start + MovementDirection * Step;
	}
	else
	{
		End = GetLocationAtDistance(Record, NewDistance);
		MovementDirection = (End - Start).GetSafeNormal(SMALL_NUMBER, MovementDirection);
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ManagedRocketCollision), false);
	Params.AddIgnoredActor(Record.Owner.Get());
	if (const AController* Controller = Record.InstigatorController.Get())
	{
		Params.AddIgnoredActor(Controller->GetPawn());
	}
	FHitResult Hit;
	if (bPerfLoggingThisTick)
	{
		++PerfCounters.Sweeps;
	}
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(Record.PhysicalCollisionRadius), Params))
	{
		if (bPerfLoggingThisTick)
		{
			++PerfCounters.SweepHits;
		}
		if (IsEvadingManagedRocket(Hit.GetActor()))
		{
			if (Record.GuidanceMode == ERocketGuidanceMode::Homing
				&& Hit.GetActor() == Record.HomingTarget.Get() && !Record.bHomingDisabled)
			{
				Record.bHomingDisabled = true;
				QueueHomingUpdate(Record);
			}
		}
		else
		{
			AActor* DamageTarget = Hit.GetActor()
				&& Hit.GetActor()->FindComponentByClass<UHealthComponent>() ? Hit.GetActor() : nullptr;
			ExplodeDataOnlyRocket(Handle, DamageTarget, Hit.ImpactPoint, &Hit);
			return;
		}
	}
	Record.TimeSinceProximityCheck += DeltaTime;
	if (Record.ProximityRadius > 0.0f
		&& Record.TimeSinceProximityCheck >= Record.ProximityCheckInterval)
	{
		Record.TimeSinceProximityCheck = 0.0f;
		if (bPerfLoggingThisTick)
		{
			++PerfCounters.ProximityChecks;
		}
		AActor* Closest = nullptr;
		if (Record.GuidanceMode == ERocketGuidanceMode::Homing)
		{
			AActor* Target = GetLiveHomingTarget(Record);
			if (Target && FVector::DistSquared(End, Target->GetActorLocation())
				<= FMath::Square(Record.ProximityRadius))
			{
				if (IsEvadingManagedRocket(Target))
				{
					Record.bHomingDisabled = true;
					QueueHomingUpdate(Record);
				}
				else
				{
					Closest = Target;
				}
			}
		}
		else
		{
			if (bPerfLoggingThisTick)
			{
				++PerfCounters.OverlapQueries;
			}
			FCollisionObjectQueryParams Types;
			Types.AddObjectTypesToQuery(ECC_Pawn);
			Types.AddObjectTypesToQuery(ECC_WorldDynamic);
			TArray<FOverlapResult> Overlaps;
			GetWorld()->OverlapMultiByObjectType(Overlaps, End, FQuat::Identity, Types,
				FCollisionShape::MakeSphere(Record.ProximityRadius), Params);
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
		}
		if (Closest)
		{
			ExplodeDataOnlyRocket(Handle, Closest, End, nullptr);
			return;
		}
	}
	Record.Location = End;
	Record.Direction = MovementDirection;
	Record.DistanceTraveled = NewDistance;
	if (Record.GuidanceMode == ERocketGuidanceMode::Homing
		&& !Record.bHomingDisabled && !Record.bTerminalCorrectionHandled
		&& !bFollowingLaunchCurve)
	{
		if (const AActor* Target = GetLiveHomingTarget(Record))
		{
			const float CorrectionDistance = FMath::Max(
				Record.ProximityRadius * 2.0f,
				Record.Speed * HomingTerminalCorrectionLeadSeconds);
			if (FVector::DistSquared(End, Target->GetActorLocation())
				<= FMath::Square(CorrectionDistance))
			{
				Record.bTerminalCorrectionHandled = true;
				if (TerminalHomingCorrectionsThisTick
					< MaxTerminalHomingCorrectionsPerTick)
				{
					++TerminalHomingCorrectionsThisTick;
					QueueHomingUpdate(Record);
					if (bPerfLoggingThisTick)
					{
						++PerfCounters.TerminalCorrectionsSent;
					}
				}
				else if (bPerfLoggingThisTick)
				{
					++PerfCounters.TerminalCorrectionsSkipped;
				}
			}
		}
	}
	if (Record.bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End,
			Record.GuidanceMode == ERocketGuidanceMode::Homing ? FColor::Magenta : FColor::Cyan,
			false, 0.15f, 0, 1.0f);
	}
	if (NewDistance >= Record.MaxTravelDistance - KINDA_SMALL_NUMBER)
	{
		ExplodeDataOnlyRocket(Handle, nullptr, End, nullptr);
	}
}

FVector URocketSimulationManager::RotateDirectionTowards(
	const FVector& CurrentDirection, const FVector& DesiredDirection,
	const float MaxTurnRateDegreesPerSecond, const float DeltaTime) const
{
	const FVector Current = CurrentDirection.GetSafeNormal();
	const FVector Desired = DesiredDirection.GetSafeNormal();
	if (Current.IsNearlyZero() || Desired.IsNearlyZero())
	{
		return CurrentDirection;
	}
	const float Angle = FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Current, Desired), -1.0f, 1.0f));
	if (Angle <= KINDA_SMALL_NUMBER)
	{
		return Desired;
	}
	const float MaxStep = FMath::DegreesToRadians(
		FMath::Max(0.0f, MaxTurnRateDegreesPerSecond)) * DeltaTime;
	const float Alpha = FMath::Clamp(MaxStep / Angle, 0.0f, 1.0f);
	return FQuat::Slerp(FQuat::Identity,
		FQuat::FindBetweenNormals(Current, Desired), Alpha)
		.RotateVector(Current).GetSafeNormal();
}

AActor* URocketSimulationManager::GetLiveHomingTarget(
	const FManagedRocketRecord& Record) const
{
	if (Record.bHomingDisabled)
	{
		return nullptr;
	}
	AActor* Target = Record.HomingTarget.Get();
	const UHealthComponent* Health = Target
		? Target->FindComponentByClass<UHealthComponent>() : nullptr;
	return Health && !Health->IsDead() ? Target : nullptr;
}

void URocketSimulationManager::QueueHomingUpdate(FManagedRocketRecord& Record)
{
	FManagedRocketHomingUpdateEvent& Event = PendingHomingUpdateEvents.AddDefaulted_GetRef();
	Event.RocketId = Record.RocketId;
	Event.Location = Record.Location;
	Event.Direction = Record.Direction;
	Event.DistanceTraveled = Record.DistanceTraveled;
	Event.ServerTime = GetWorld()->GetTimeSeconds();
	Event.bHomingDisabled = Record.bHomingDisabled;
}

void URocketSimulationManager::FinishPerfTick(const double TickStartSeconds)
{
	const double NowSeconds = FPlatformTime::Seconds();
	const double TickMilliseconds = (NowSeconds - TickStartSeconds) * 1000.0;
	PerfCounters.TotalTickMilliseconds += TickMilliseconds;
	PerfCounters.MaxTickMilliseconds = FMath::Max(
		PerfCounters.MaxTickMilliseconds, TickMilliseconds);
	++PerfCounters.TickCount;
	PerfCounters.MaxActiveRockets = FMath::Max(
		PerfCounters.MaxActiveRockets, ActiveRocketCount);

	const double IntervalSeconds = FMath::Max(0.1,
		static_cast<double>(CVarRocketPerfLogInterval.GetValueOnGameThread()));
	if (NowSeconds - PerfCounters.WindowStartSeconds < IntervalSeconds)
	{
		return;
	}

	const double AverageTickMilliseconds = PerfCounters.TickCount > 0
		? PerfCounters.TotalTickMilliseconds / static_cast<double>(PerfCounters.TickCount)
		: 0.0;
	UE_LOG(LogTemp, Display,
		TEXT("[RocketPerf][Server] World=%s Active=%d MaxActive=%d Records=%d Ticks=%llu AvgTickMs=%.3f MaxTickMs=%.3f DataOnlySteps=%llu ActorSteps=%llu Sweeps=%llu SweepHits=%llu ProximityChecks=%llu OverlapQueries=%llu LaunchEvents=%llu LaunchBatches=%llu MaxLaunchBatch=%d LargeLaunchBatches=%llu Explosions=%llu HomingUpdates=%llu TerminalCorrections=%llu TerminalSkipped=%llu UpdateBatches=%llu MaxUpdateBatch=%d"),
		*GetNameSafe(GetWorld()), ActiveRocketCount, PerfCounters.MaxActiveRockets,
		Records.Num(), static_cast<unsigned long long>(PerfCounters.TickCount),
		AverageTickMilliseconds, PerfCounters.MaxTickMilliseconds,
		static_cast<unsigned long long>(PerfCounters.DataOnlySimulations),
		static_cast<unsigned long long>(PerfCounters.ActorSimulations),
		static_cast<unsigned long long>(PerfCounters.Sweeps),
		static_cast<unsigned long long>(PerfCounters.SweepHits),
		static_cast<unsigned long long>(PerfCounters.ProximityChecks),
		static_cast<unsigned long long>(PerfCounters.OverlapQueries),
		static_cast<unsigned long long>(PerfCounters.LaunchEvents),
		static_cast<unsigned long long>(PerfCounters.LaunchBatches),
		PerfCounters.MaxLaunchBatch,
		static_cast<unsigned long long>(PerfCounters.LargeLaunchBatches),
		static_cast<unsigned long long>(PerfCounters.ExplosionEvents),
		static_cast<unsigned long long>(PerfCounters.HomingUpdateEvents),
		static_cast<unsigned long long>(PerfCounters.TerminalCorrectionsSent),
		static_cast<unsigned long long>(PerfCounters.TerminalCorrectionsSkipped),
		static_cast<unsigned long long>(PerfCounters.UpdateBatches),
		PerfCounters.MaxUpdateBatch);
	PerfCounters.Reset(NowSeconds);
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
