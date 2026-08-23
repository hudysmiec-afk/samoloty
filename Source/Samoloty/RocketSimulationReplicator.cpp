#include "RocketSimulationReplicator.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "MissileWarningComponent.h"
#include "NiagaraCommon.h"
#include "NiagaraComponent.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool IsRocketPerfLoggingEnabled()
	{
		const IConsoleVariable* PerfLog = IConsoleManager::Get().FindConsoleVariable(
			TEXT("samoloty.Rockets.PerfLog"));
		return PerfLog && PerfLog->GetInt() != 0;
	}

	double GetRocketPerfLogInterval()
	{
		const IConsoleVariable* Interval = IConsoleManager::Get().FindConsoleVariable(
			TEXT("samoloty.Rockets.PerfLogInterval"));
		return Interval ? FMath::Max(0.1, static_cast<double>(Interval->GetFloat())) : 1.0;
	}

	const TCHAR* GetNetModeName(const ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}
}

ARocketSimulationReplicator::ARocketSimulationReplicator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	static ConstructorHelpers::FObjectFinder<UNiagaraDataChannelAsset> DataChannelFinder(
		TEXT("/Game/Main/SystemNiagara/NDC_RocketTrails.NDC_RocketTrails"));
	if (DataChannelFinder.Succeeded())
	{
		TrailDataChannel = DataChannelFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrailSystemFinder(
		TEXT("/Game/Main/SystemNiagara/NS_RocketTrail_Batched.NS_RocketTrail_Batched"));
	if (TrailSystemFinder.Succeeded())
	{
		BatchedTrailSystem = TrailSystemFinder.Object;
	}
}

void ARocketSimulationReplicator::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const double TickStartSeconds = FPlatformTime::Seconds();
	bPerfLoggingThisTick = IsRocketPerfLoggingEnabled();
	if (!bPerfLoggingThisTick)
	{
		PerfCounters.Reset(TickStartSeconds);
	}
	else if (PerfCounters.WindowStartSeconds <= 0.0)
	{
		PerfCounters.WindowStartSeconds = TickStartSeconds;
	}
	TArray<uint32> Expired;
	TArray<bool> DirtyGroups;
	DirtyGroups.Init(false, VisualGroups.Num());
	if (bPerfLoggingThisTick)
	{
		PerfCounters.MaxVisualRockets = FMath::Max(
			PerfCounters.MaxVisualRockets, VisualRockets.Num());
	}
	for (TPair<uint32, FVisualRocketRecord>& Pair : VisualRockets)
	{
		FVisualRocketRecord& Record = Pair.Value;
		AdvanceVisualRocket(Record, DeltaSeconds);
		if (!VisualGroups.IsValidIndex(Record.VisualGroupIndex))
		{
			Expired.Add(Pair.Key);
			continue;
		}
		FVisualRocketGroup& Group = VisualGroups[Record.VisualGroupIndex];
		if (Group.Instances)
		{
			Group.Instances->UpdateInstanceTransform(Record.InstanceIndex,
				Group.MeshRelativeTransform * GetRocketTransform(Record),
				false, false, true);
			if (bPerfLoggingThisTick)
			{
				++PerfCounters.InstanceUpdates;
			}
			DirtyGroups[Record.VisualGroupIndex] = true;
		}
		if (Record.DistanceTraveled >= Record.LaunchData.MaxTravelDistance - KINDA_SMALL_NUMBER)
		{
			Expired.Add(Pair.Key);
		}
	}
	for (int32 GroupIndex = 0; GroupIndex < VisualGroups.Num(); ++GroupIndex)
	{
		if (DirtyGroups[GroupIndex] && VisualGroups[GroupIndex].Instances)
		{
			VisualGroups[GroupIndex].Instances->MarkRenderStateDirty();
			if (bPerfLoggingThisTick)
			{
				++PerfCounters.DirtyGroups;
			}
		}
	}
	PublishTrailData();
	for (const uint32 RocketId : Expired)
	{
		ReleaseVisualRocket(RocketId, nullptr);
	}
	if (bPerfLoggingThisTick)
	{
		PerfCounters.DistanceExpired += Expired.Num();
		FinishPerfTick(TickStartSeconds);
	}
}

void ARocketSimulationReplicator::BroadcastLaunchBatch(
	const TArray<FManagedRocketLaunchEvent>& Events)
{
	if (HasAuthority() && !Events.IsEmpty())
	{
		MulticastLaunchBatch(Events);
	}
}

void ARocketSimulationReplicator::BroadcastUpdateBatch(
	const TArray<FManagedRocketExplosionEvent>& ExplosionEvents,
	const TArray<FManagedRocketHomingUpdateEvent>& HomingUpdateEvents)
{
	if (HasAuthority() && (!ExplosionEvents.IsEmpty() || !HomingUpdateEvents.IsEmpty()))
	{
		MulticastUpdateBatch(ExplosionEvents, HomingUpdateEvents);
	}
}

void ARocketSimulationReplicator::MulticastLaunchBatch_Implementation(
	const TArray<FManagedRocketLaunchEvent>& Events)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld())
	{
		return;
	}
	const bool bRecordPerf = IsRocketPerfLoggingEnabled();
	if (bRecordPerf)
	{
		if (PerfCounters.WindowStartSeconds <= 0.0)
		{
			PerfCounters.WindowStartSeconds = FPlatformTime::Seconds();
		}
		PerfCounters.LaunchEventsReceived += Events.Num();
		PerfCounters.MaxLaunchBatch = FMath::Max(
			PerfCounters.MaxLaunchBatch, Events.Num());
	}
	for (const FManagedRocketLaunchEvent& Event : Events)
	{
		if (!Event.RocketClass)
		{
			continue;
		}
		const int32 GroupIndex = FindOrCreateVisualGroup(Event.RocketClass);
		if (!VisualGroups.IsValidIndex(GroupIndex))
		{
			continue;
		}
		FVisualRocketGroup& Group = VisualGroups[GroupIndex];
		FVisualRocketRecord Record;
		Record.RocketClass = Event.RocketClass;
		Record.LaunchData = Event.LaunchData;
		Record.VisualGroupIndex = GroupIndex;
		Record.TrailProfileId = Event.RocketClass->GetDefaultObject<ARocketProjectile>()
			->GetManagedTrailProfileId();
		if (Record.LaunchData.GuidanceMode == ERocketGuidanceMode::Homing)
		{
			Record.HomingTarget = Record.LaunchData.HomingTarget;
			Record.WarningTarget = Record.HomingTarget;
			if (AActor* Target = Record.WarningTarget.Get())
			{
				if (UMissileWarningComponent* Warning =
					Target->FindComponentByClass<UMissileWarningComponent>())
				{
					Warning->AddIncomingManagedMissile(Event.RocketId);
				}
			}
			// VisualRockets is not a reflected container. Keep the target only as a
			// weak pointer so a destroyed aircraft cannot leave a stale UObject pointer.
			Record.LaunchData.HomingTarget = nullptr;
		}
		BuildCurveCache(Record);
		Record.Location = Event.LaunchData.StartLocation;
		Record.Direction = Record.LaunchData.bUseSeparationCurve
			? (FVector(Record.LaunchData.SeparationControlPoint1)
				- FVector(Record.LaunchData.StartLocation)).GetSafeNormal(
					SMALL_NUMBER, FVector(Record.LaunchData.Direction))
			: FVector(Record.LaunchData.Direction).GetSafeNormal();
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const double ServerNow = GameState
			? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const float Elapsed = FMath::Max(0.0f,
			static_cast<float>(ServerNow - Record.LaunchData.ServerStartTime));
		AdvanceVisualRocket(Record, Elapsed);
		const FTransform Transform = Group.MeshRelativeTransform * GetRocketTransform(Record);
		if (!Group.FreeInstances.IsEmpty())
		{
			Record.InstanceIndex = Group.FreeInstances.Pop(EAllowShrinking::No);
			Group.Instances->UpdateInstanceTransform(Record.InstanceIndex,
				Transform, false, true, true);
		}
		else
		{
			Record.InstanceIndex = Group.Instances->AddInstance(Transform, false);
		}
		VisualRockets.Add(Event.RocketId, MoveTemp(Record));
		if (bRecordPerf)
		{
			++PerfCounters.VisualRocketsAdded;
		}
	}
}

void ARocketSimulationReplicator::MulticastUpdateBatch_Implementation(
	const TArray<FManagedRocketExplosionEvent>& ExplosionEvents,
	const TArray<FManagedRocketHomingUpdateEvent>& HomingUpdateEvents)
{
	if (IsRocketPerfLoggingEnabled())
	{
		if (PerfCounters.WindowStartSeconds <= 0.0)
		{
			PerfCounters.WindowStartSeconds = FPlatformTime::Seconds();
		}
		PerfCounters.ExplosionEventsReceived += ExplosionEvents.Num();
		PerfCounters.HomingUpdatesReceived += HomingUpdateEvents.Num();
		PerfCounters.MaxUpdateBatch = FMath::Max(PerfCounters.MaxUpdateBatch,
			ExplosionEvents.Num() + HomingUpdateEvents.Num());
	}
	for (const FManagedRocketHomingUpdateEvent& Event : HomingUpdateEvents)
	{
		ApplyHomingUpdate(Event);
	}
	for (const FManagedRocketExplosionEvent& Event : ExplosionEvents)
	{
		ReleaseVisualRocket(Event.RocketId, &Event.Impact);
	}
}

int32 ARocketSimulationReplicator::FindOrCreateVisualGroup(
	const TSubclassOf<ARocketProjectile> RocketClass)
{
	if (const int32* Existing = VisualGroupByClass.Find(RocketClass))
	{
		return *Existing;
	}
	const ARocketProjectile* DefaultRocket = RocketClass
		? RocketClass->GetDefaultObject<ARocketProjectile>() : nullptr;
	const UStaticMeshComponent* MeshComponent = DefaultRocket
		? DefaultRocket->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return INDEX_NONE;
	}
	const int32 GroupIndex = VisualGroups.AddDefaulted();
	FVisualRocketGroup& Group = VisualGroups[GroupIndex];
	Group.MeshRelativeTransform = MeshComponent->GetRelativeTransform();
	if (const UNiagaraComponent* TrailComponent =
		DefaultRocket->FindComponentByClass<UNiagaraComponent>())
	{
		Group.TrailRelativeTransform = TrailComponent->GetRelativeTransform();
	}
	Group.Instances = NewObject<UInstancedStaticMeshComponent>(this);
	Group.Instances->SetupAttachment(GetRootComponent());
	Group.Instances->SetStaticMesh(MeshComponent->GetStaticMesh());
	for (int32 MaterialIndex = 0; MaterialIndex < MeshComponent->GetNumMaterials(); ++MaterialIndex)
	{
		Group.Instances->SetMaterial(MaterialIndex, MeshComponent->GetMaterial(MaterialIndex));
	}
	Group.Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Group.Instances->SetCastShadow(MeshComponent->CastShadow);
	Group.Instances->RegisterComponent();
	AddInstanceComponent(Group.Instances);
	VisualGroupByClass.Add(RocketClass, GroupIndex);
	return GroupIndex;
}

void ARocketSimulationReplicator::BuildCurveCache(FVisualRocketRecord& Record) const
{
	Record.CurveDistances[0] = 0.0f;
	if (!Record.LaunchData.bUseSeparationCurve)
	{
		return;
	}
	FVector Previous = EvaluateCurve(Record, 0.0f);
	for (int32 Index = 1; Index <= FVisualRocketRecord::CurveSamples; ++Index)
	{
		const FVector Point = EvaluateCurve(Record,
			static_cast<float>(Index) / FVisualRocketRecord::CurveSamples);
		Record.CurveLength += FVector::Distance(Previous, Point);
		Record.CurveDistances[Index] = Record.CurveLength;
		Previous = Point;
	}
}

FVector ARocketSimulationReplicator::EvaluateCurve(
	const FVisualRocketRecord& Record, const float Parameter) const
{
	const float T = FMath::Clamp(Parameter, 0.0f, 1.0f);
	const float O = 1.0f - T;
	return FVector(Record.LaunchData.StartLocation) * (O * O * O)
		+ FVector(Record.LaunchData.SeparationControlPoint1) * (3.0f * O * O * T)
		+ FVector(Record.LaunchData.SeparationControlPoint2) * (3.0f * O * T * T)
		+ FVector(Record.LaunchData.SeparationEndPoint) * (T * T * T);
}

float ARocketSimulationReplicator::FindCurveParameter(
	const FVisualRocketRecord& Record, const float Distance) const
{
	const float Clamped = FMath::Clamp(Distance, 0.0f, Record.CurveLength);
	for (int32 Index = 1; Index <= FVisualRocketRecord::CurveSamples; ++Index)
	{
		if (Record.CurveDistances[Index] >= Clamped)
		{
			const float Previous = Record.CurveDistances[Index - 1];
			const float Span = Record.CurveDistances[Index] - Previous;
			const float Alpha = Span > KINDA_SMALL_NUMBER
				? (Clamped - Previous) / Span : 0.0f;
			return (static_cast<float>(Index - 1) + Alpha)
				/ FVisualRocketRecord::CurveSamples;
		}
	}
	return 1.0f;
}

FTransform ARocketSimulationReplicator::GetRocketTransform(
	const FVisualRocketRecord& Record) const
{
	if (Record.LaunchData.GuidanceMode == ERocketGuidanceMode::Homing)
	{
		return FTransform(Record.Direction.Rotation(), Record.Location);
	}
	return GetPathTransform(Record, Record.DistanceTraveled);
}

FTransform ARocketSimulationReplicator::GetPathTransform(
	const FVisualRocketRecord& Record, const float Distance) const
{
	FVector Location;
	FVector Direction = FVector(Record.LaunchData.Direction).GetSafeNormal();
	if (Record.LaunchData.bUseSeparationCurve
		&& Distance < Record.CurveLength)
	{
		const float T = FindCurveParameter(Record, Distance);
		Location = EvaluateCurve(Record, T);
		const float O = 1.0f - T;
		const FVector P0 = Record.LaunchData.StartLocation;
		const FVector P1 = Record.LaunchData.SeparationControlPoint1;
		const FVector P2 = Record.LaunchData.SeparationControlPoint2;
		const FVector P3 = Record.LaunchData.SeparationEndPoint;
		Direction = ((P1 - P0) * (3.0f * O * O)
			+ (P2 - P1) * (6.0f * O * T)
			+ (P3 - P2) * (3.0f * T * T)).GetSafeNormal(
				SMALL_NUMBER, Direction);
	}
	else
	{
		const FVector Start = Record.LaunchData.bUseSeparationCurve
			? FVector(Record.LaunchData.SeparationEndPoint)
			: FVector(Record.LaunchData.StartLocation);
		const float StraightDistance = Record.LaunchData.bUseSeparationCurve
			? FMath::Max(0.0f, Distance - Record.CurveLength)
			: Distance;
		Location = Start + Direction * StraightDistance;
	}
	return FTransform(Direction.Rotation(), Location);
}

void ARocketSimulationReplicator::AdvanceVisualRocket(
	FVisualRocketRecord& Record, const float DeltaSeconds) const
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}
	const float Remaining = FMath::Max(0.0f,
		Record.LaunchData.MaxTravelDistance - Record.DistanceTraveled);
	const float Step = FMath::Min(Record.LaunchData.Speed * DeltaSeconds, Remaining);
	const float NewDistance = Record.DistanceTraveled + Step;
	if (Record.LaunchData.GuidanceMode != ERocketGuidanceMode::Homing)
	{
		Record.DistanceTraveled = NewDistance;
		return;
	}
	const bool bFollowingLaunchCurve = Record.LaunchData.bUseSeparationCurve
		&& Record.DistanceTraveled < Record.CurveLength;
	if (bFollowingLaunchCurve)
	{
		const FTransform Transform = GetPathTransform(Record, NewDistance);
		Record.Location = Transform.GetLocation();
		Record.Direction = Transform.GetRotation().GetForwardVector();
	}
	else
	{
		if (!Record.bHomingDisabled)
		{
			const AActor* Target = Record.HomingTarget.Get();
			if (IsValid(Target))
			{
				const FVector DesiredDirection =
					(Target->GetActorLocation() - Record.Location).GetSafeNormal(
						SMALL_NUMBER, Record.Direction);
				Record.Direction = RotateDirectionTowards(Record.Direction, DesiredDirection,
					Record.LaunchData.MaxTurnRateDegreesPerSecond, DeltaSeconds);
			}
		}
		Record.Location += Record.Direction * Step;
	}
	Record.DistanceTraveled = NewDistance;
}

FVector ARocketSimulationReplicator::RotateDirectionTowards(
	const FVector& CurrentDirection, const FVector& DesiredDirection,
	const float MaxTurnRateDegreesPerSecond, const float DeltaSeconds) const
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
		FMath::Max(0.0f, MaxTurnRateDegreesPerSecond)) * DeltaSeconds;
	const float Alpha = FMath::Clamp(MaxStep / Angle, 0.0f, 1.0f);
	return FQuat::Slerp(FQuat::Identity,
		FQuat::FindBetweenNormals(Current, Desired), Alpha)
		.RotateVector(Current).GetSafeNormal();
}

void ARocketSimulationReplicator::ApplyHomingUpdate(
	const FManagedRocketHomingUpdateEvent& Event)
{
	FVisualRocketRecord* Record = VisualRockets.Find(Event.RocketId);
	if (!Record || Record->LaunchData.GuidanceMode != ERocketGuidanceMode::Homing)
	{
		return;
	}
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const double ServerNow = GameState
		? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Elapsed = FMath::Max(0.0f,
		static_cast<float>(ServerNow - Event.ServerTime));
	const FVector CorrectedDirection = FVector(Event.Direction).GetSafeNormal(
		SMALL_NUMBER, Record->Direction);
	const FVector CorrectedLocation = FVector(Event.Location)
		+ CorrectedDirection * Record->LaunchData.Speed * Elapsed;
	const float ErrorSquared = FVector::DistSquared(Record->Location, CorrectedLocation);
	const float Alpha = ErrorSquared > FMath::Square(5000.0f) ? 1.0f : 0.35f;
	Record->Location = FMath::Lerp(Record->Location, CorrectedLocation, Alpha);
	Record->Direction = FMath::Lerp(
		Record->Direction, CorrectedDirection, Alpha).GetSafeNormal();
	Record->DistanceTraveled = FMath::Max(Record->DistanceTraveled,
		Event.DistanceTraveled + Record->LaunchData.Speed * Elapsed);
	Record->bHomingDisabled = Event.bHomingDisabled;
	if (Record->bHomingDisabled)
	{
		if (AActor* Target = Record->WarningTarget.Get())
		{
			if (UMissileWarningComponent* Warning =
				Target->FindComponentByClass<UMissileWarningComponent>())
			{
				Warning->RemoveIncomingManagedMissile(Event.RocketId);
			}
		}
		Record->WarningTarget.Reset();
	}
}

FTransform ARocketSimulationReplicator::GetTrailTransform(
	const FVisualRocketRecord& Record) const
{
	const FTransform RocketTransform = GetRocketTransform(Record);
	return VisualGroups.IsValidIndex(Record.VisualGroupIndex)
		? VisualGroups[Record.VisualGroupIndex].TrailRelativeTransform * RocketTransform
		: RocketTransform;
}

void ARocketSimulationReplicator::PublishTrailData()
{
	if (!GetWorld() || VisualRockets.IsEmpty() || !TrailDataChannel
		|| !TrailDataChannel->Get() || !BatchedTrailSystem)
	{
		return;
	}
	if (!BatchedTrailComponent)
	{
		BatchedTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			BatchedTrailSystem, GetRootComponent(), NAME_None, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			false, true, ENCPoolMethod::None, true);
		if (!BatchedTrailComponent)
		{
			return;
		}
	}
	FNDCAccessContextInst& AccessContext =
		UNiagaraDataChannelLibrary::GetUsableAccessContextFromNDC(TrailDataChannel);
	UNiagaraDataChannelWriter* Writer =
		UNiagaraDataChannelLibrary::CreateDataChannelWriter_WithContext(
			this, TrailDataChannel->Get(), AccessContext, VisualRockets.Num(),
			false, true, false, TEXT("Managed Rocket Trails"));
	if (!Writer)
	{
		return;
	}
	const double PublishStartSeconds = bPerfLoggingThisTick
		? FPlatformTime::Seconds() : 0.0;
	int32 DataIndex = 0;
	FBox WorldBounds(EForceInit::ForceInit);
	for (const TPair<uint32, FVisualRocketRecord>& Pair : VisualRockets)
	{
		const FVisualRocketRecord& Record = Pair.Value;
		const FVector TrailLocation = GetTrailTransform(Record).GetLocation();
		Writer->WritePosition(TEXT("Position"), DataIndex,
			TrailLocation);
		WorldBounds += TrailLocation;
		WorldBounds += FVector(Record.LaunchData.StartLocation);
		if (Record.LaunchData.bUseSeparationCurve)
		{
			WorldBounds += FVector(Record.LaunchData.SeparationControlPoint1);
			WorldBounds += FVector(Record.LaunchData.SeparationControlPoint2);
			WorldBounds += FVector(Record.LaunchData.SeparationEndPoint);
		}
		FNiagaraID NiagaraId;
		NiagaraId.Index = static_cast<int32>(Pair.Key);
		NiagaraId.AcquireTag = 0;
		Writer->WriteID(TEXT("RocketId"), DataIndex, NiagaraId);
		Writer->WriteInt(TEXT("ProfileId"), DataIndex, Record.TrailProfileId);
		++DataIndex;
	}
	if (WorldBounds.IsValid)
	{
		const FBox LocalBounds = WorldBounds.TransformBy(GetActorTransform().Inverse())
			.ExpandBy(500.0f);
		BatchedTrailComponent->SetSystemFixedBounds(LocalBounds);
	}
	if (bPerfLoggingThisTick)
	{
		const double PublishMilliseconds =
			(FPlatformTime::Seconds() - PublishStartSeconds) * 1000.0;
		++PerfCounters.NdcPublishes;
		PerfCounters.NdcRows += DataIndex;
		PerfCounters.TotalNdcMilliseconds += PublishMilliseconds;
		PerfCounters.MaxNdcMilliseconds = FMath::Max(
			PerfCounters.MaxNdcMilliseconds, PublishMilliseconds);
	}
}

void ARocketSimulationReplicator::FinishPerfTick(const double TickStartSeconds)
{
	const double NowSeconds = FPlatformTime::Seconds();
	const double TickMilliseconds = (NowSeconds - TickStartSeconds) * 1000.0;
	PerfCounters.TotalTickMilliseconds += TickMilliseconds;
	PerfCounters.MaxTickMilliseconds = FMath::Max(
		PerfCounters.MaxTickMilliseconds, TickMilliseconds);
	++PerfCounters.TickCount;
	PerfCounters.MaxVisualRockets = FMath::Max(
		PerfCounters.MaxVisualRockets, VisualRockets.Num());
	if (NowSeconds - PerfCounters.WindowStartSeconds < GetRocketPerfLogInterval())
	{
		return;
	}

	const double AverageTickMilliseconds = PerfCounters.TickCount > 0
		? PerfCounters.TotalTickMilliseconds / static_cast<double>(PerfCounters.TickCount)
		: 0.0;
	const double AverageNdcMilliseconds = PerfCounters.NdcPublishes > 0
		? PerfCounters.TotalNdcMilliseconds / static_cast<double>(PerfCounters.NdcPublishes)
		: 0.0;
	UE_LOG(LogTemp, Display,
		TEXT("[RocketPerf][Visual] World=%s Mode=%s Visual=%d MaxVisual=%d Groups=%d Ticks=%llu AvgTickMs=%.3f MaxTickMs=%.3f LaunchRecv=%llu LaunchAdded=%llu MaxLaunchBatch=%d ExplosionRecv=%llu HomingRecv=%llu MaxUpdateBatch=%d ISMUpdates=%llu DirtyGroups=%llu NDCPublishes=%llu NDCRows=%llu AvgNDCMs=%.3f MaxNDCMs=%.3f DistanceExpired=%llu"),
		*GetNameSafe(GetWorld()), GetNetModeName(GetNetMode()), VisualRockets.Num(),
		PerfCounters.MaxVisualRockets, VisualGroups.Num(),
		static_cast<unsigned long long>(PerfCounters.TickCount),
		AverageTickMilliseconds, PerfCounters.MaxTickMilliseconds,
		static_cast<unsigned long long>(PerfCounters.LaunchEventsReceived),
		static_cast<unsigned long long>(PerfCounters.VisualRocketsAdded),
		PerfCounters.MaxLaunchBatch,
		static_cast<unsigned long long>(PerfCounters.ExplosionEventsReceived),
		static_cast<unsigned long long>(PerfCounters.HomingUpdatesReceived),
		PerfCounters.MaxUpdateBatch,
		static_cast<unsigned long long>(PerfCounters.InstanceUpdates),
		static_cast<unsigned long long>(PerfCounters.DirtyGroups),
		static_cast<unsigned long long>(PerfCounters.NdcPublishes),
		static_cast<unsigned long long>(PerfCounters.NdcRows),
		AverageNdcMilliseconds, PerfCounters.MaxNdcMilliseconds,
		static_cast<unsigned long long>(PerfCounters.DistanceExpired));
	PerfCounters.Reset(NowSeconds);
}

void ARocketSimulationReplicator::ReleaseVisualRocket(const uint32 RocketId,
	const FCombatImpactEvent* Impact)
{
	FVisualRocketRecord* Record = VisualRockets.Find(RocketId);
	if (!Record)
	{
		return;
	}
	if (Impact && Record->RocketClass)
	{
		Record->RocketClass->GetDefaultObject<ARocketProjectile>()
			->PlayManagedExplosionCosmetics(GetWorld(), Record->LaunchData, *Impact);
	}
	if (AActor* Target = Record->WarningTarget.Get())
	{
		if (UMissileWarningComponent* Warning =
			Target->FindComponentByClass<UMissileWarningComponent>())
		{
			Warning->RemoveIncomingManagedMissile(RocketId);
		}
	}
	if (VisualGroups.IsValidIndex(Record->VisualGroupIndex))
	{
		FVisualRocketGroup& Group = VisualGroups[Record->VisualGroupIndex];
		if (Group.Instances)
		{
			Group.Instances->UpdateInstanceTransform(Record->InstanceIndex,
				FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector),
				false, true, true);
			Group.FreeInstances.Add(Record->InstanceIndex);
		}
	}
	VisualRockets.Remove(RocketId);
}
