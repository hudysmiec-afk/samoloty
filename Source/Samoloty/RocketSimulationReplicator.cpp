#include "RocketSimulationReplicator.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraCommon.h"
#include "NiagaraComponent.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

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
	TArray<uint32> Expired;
	TArray<bool> DirtyGroups;
	DirtyGroups.Init(false, VisualGroups.Num());
	for (TPair<uint32, FVisualRocketRecord>& Pair : VisualRockets)
	{
		FVisualRocketRecord& Record = Pair.Value;
		Record.DistanceTraveled = FMath::Min(
			Record.DistanceTraveled + Record.LaunchData.Speed * DeltaSeconds,
			Record.LaunchData.MaxTravelDistance);
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
		}
	}
	PublishTrailData();
	for (const uint32 RocketId : Expired)
	{
		ReleaseVisualRocket(RocketId, nullptr);
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

void ARocketSimulationReplicator::BroadcastExplosionBatch(
	const TArray<FManagedRocketExplosionEvent>& Events)
{
	if (HasAuthority() && !Events.IsEmpty())
	{
		MulticastExplosionBatch(Events);
	}
}

void ARocketSimulationReplicator::MulticastLaunchBatch_Implementation(
	const TArray<FManagedRocketLaunchEvent>& Events)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld())
	{
		return;
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
		BuildCurveCache(Record);
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const double ServerNow = GameState
			? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const float Elapsed = FMath::Max(0.0f,
			static_cast<float>(ServerNow - Record.LaunchData.ServerStartTime));
		Record.DistanceTraveled = FMath::Min(
			Record.LaunchData.Speed * Elapsed,
			Record.LaunchData.MaxTravelDistance);
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
	}
}

void ARocketSimulationReplicator::MulticastExplosionBatch_Implementation(
	const TArray<FManagedRocketExplosionEvent>& Events)
{
	for (const FManagedRocketExplosionEvent& Event : Events)
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
	FVector Location;
	FVector Direction = FVector(Record.LaunchData.Direction).GetSafeNormal();
	if (Record.LaunchData.bUseSeparationCurve
		&& Record.DistanceTraveled < Record.CurveLength)
	{
		const float T = FindCurveParameter(Record, Record.DistanceTraveled);
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
			? FMath::Max(0.0f, Record.DistanceTraveled - Record.CurveLength)
			: Record.DistanceTraveled;
		Location = Start + Direction * StraightDistance;
	}
	return FTransform(Direction.Rotation(), Location);
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
