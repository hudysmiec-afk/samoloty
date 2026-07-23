#include "RocketProjectile.h"

#include "HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

int32 ARocketProjectile::ServerActiveRocketCount = 0;

ARocketProjectile::ARocketProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(5.0f);
	SetNetCullDistanceSquared(FMath::Square(500000.0f));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
	RocketMesh->SetupAttachment(SceneRoot);
	RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RocketMesh->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	RocketMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.35f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DebugRocketMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (DebugRocketMesh.Succeeded())
	{
		RocketMesh->SetStaticMesh(DebugRocketMesh.Object);
	}

	RocketTrail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RocketTrail"));
	RocketTrail->SetupAttachment(SceneRoot);
	RocketTrail->bAutoActivate = true;
}

void ARocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bInitialized && !bCountedOnServer)
	{
		++ServerActiveRocketCount;
		bCountedOnServer = true;
	}
}

void ARocketProjectile::InitializeRocket(const FRocketLaunchData& InLaunchData)
{
	check(HasAuthority());
	LaunchData = InLaunchData;
	bInitialized = true;
	ApplyLaunchState();
	if (HasActorBegunPlay() && !bCountedOnServer)
	{
		++ServerActiveRocketCount;
		bCountedOnServer = true;
	}
	ForceNetUpdate();
}

void ARocketProjectile::OnRep_LaunchData()
{
	bInitialized = true;
	ApplyLaunchState();
}

void ARocketProjectile::ApplyLaunchState()
{
	BuildSeparationCurveCache();
	if (!HasAuthority())
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const float Elapsed = FMath::Max(0.0f, static_cast<float>(ServerNow - LaunchData.ServerStartTime));
		DistanceTraveled = FMath::Min(LaunchData.Speed * Elapsed, LaunchData.MaxTravelDistance);
	}
	const FVector Location = GetLocationAtDistance(DistanceTraveled);
	const FVector Direction = GetDirectionAtDistance(DistanceTraveled);
	SetActorLocationAndRotation(Location, Direction.Rotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
}

void ARocketProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || bExploded)
	{
		return;
	}
	SimulateRocketMovement(DeltaSeconds);
}

void ARocketProjectile::BuildSeparationCurveCache()
{
	SeparationCurveLength = 0.0f;
	SeparationCurveCumulativeDistances[0] = 0.0f;
	if (!LaunchData.bUseSeparationCurve)
	{
		return;
	}

	FVector PreviousPoint = EvaluateSeparationCurve(0.0f);
	for (int32 SampleIndex = 1; SampleIndex <= SeparationCurveSamples; ++SampleIndex)
	{
		const float Parameter = static_cast<float>(SampleIndex) / SeparationCurveSamples;
		const FVector Point = EvaluateSeparationCurve(Parameter);
		SeparationCurveLength += FVector::Distance(PreviousPoint, Point);
		SeparationCurveCumulativeDistances[SampleIndex] = SeparationCurveLength;
		PreviousPoint = Point;
	}
}

FVector ARocketProjectile::EvaluateSeparationCurve(const float Parameter) const
{
	const float T = FMath::Clamp(Parameter, 0.0f, 1.0f);
	const float OneMinusT = 1.0f - T;
	const FVector P0 = LaunchData.StartLocation;
	const FVector P1 = LaunchData.SeparationControlPoint1;
	const FVector P2 = LaunchData.SeparationControlPoint2;
	const FVector P3 = LaunchData.SeparationEndPoint;
	return P0 * (OneMinusT * OneMinusT * OneMinusT)
		+ P1 * (3.0f * OneMinusT * OneMinusT * T)
		+ P2 * (3.0f * OneMinusT * T * T)
		+ P3 * (T * T * T);
}

float ARocketProjectile::FindCurveParameterForDistance(const float Distance) const
{
	if (!LaunchData.bUseSeparationCurve || SeparationCurveLength <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	const float ClampedDistance = FMath::Clamp(Distance, 0.0f, SeparationCurveLength);
	for (int32 SampleIndex = 1; SampleIndex <= SeparationCurveSamples; ++SampleIndex)
	{
		if (SeparationCurveCumulativeDistances[SampleIndex] >= ClampedDistance)
		{
			const float PreviousDistance = SeparationCurveCumulativeDistances[SampleIndex - 1];
			const float SegmentLength = SeparationCurveCumulativeDistances[SampleIndex] - PreviousDistance;
			const float SegmentAlpha = SegmentLength > KINDA_SMALL_NUMBER
				? (ClampedDistance - PreviousDistance) / SegmentLength : 0.0f;
			return (static_cast<float>(SampleIndex - 1) + SegmentAlpha) / SeparationCurveSamples;
		}
	}
	return 1.0f;
}

FVector ARocketProjectile::GetLocationAtDistance(const float Distance) const
{
	if (LaunchData.bUseSeparationCurve && Distance < SeparationCurveLength)
	{
		return EvaluateSeparationCurve(FindCurveParameterForDistance(Distance));
	}
	const FVector StraightStart = LaunchData.bUseSeparationCurve
		? FVector(LaunchData.SeparationEndPoint) : FVector(LaunchData.StartLocation);
	const float StraightDistance = LaunchData.bUseSeparationCurve
		? FMath::Max(0.0f, Distance - SeparationCurveLength) : Distance;
	return StraightStart + FVector(LaunchData.Direction) * StraightDistance;
}

FVector ARocketProjectile::GetDirectionAtDistance(const float Distance) const
{
	if (!LaunchData.bUseSeparationCurve || Distance >= SeparationCurveLength)
	{
		return FVector(LaunchData.Direction).GetSafeNormal();
	}
	const float T = FindCurveParameterForDistance(Distance);
	const float OneMinusT = 1.0f - T;
	const FVector P0 = LaunchData.StartLocation;
	const FVector P1 = LaunchData.SeparationControlPoint1;
	const FVector P2 = LaunchData.SeparationControlPoint2;
	const FVector P3 = LaunchData.SeparationEndPoint;
	const FVector Derivative = (P1 - P0) * (3.0f * OneMinusT * OneMinusT)
		+ (P2 - P1) * (6.0f * OneMinusT * T)
		+ (P3 - P2) * (3.0f * T * T);
	return Derivative.GetSafeNormal(SMALL_NUMBER, FVector(LaunchData.Direction));
}

void ARocketProjectile::SimulateRocketMovement(const float DeltaSeconds)
{
	const FVector Start = GetActorLocation();
	const float RemainingDistance = FMath::Max(0.0f, LaunchData.MaxTravelDistance - DistanceTraveled);
	const float StepDistance = FMath::Min(LaunchData.Speed * DeltaSeconds, RemainingDistance);
	const float NewDistanceTraveled = DistanceTraveled + StepDistance;
	const FVector End = GetLocationAtDistance(NewDistanceTraveled);

	if (HasAuthority())
	{
		FHitResult Hit;
		if (CheckPhysicalCollision(Start, End, Hit))
		{
			AActor* DamageTarget = Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UHealthComponent>()
				? Hit.GetActor() : nullptr;
			Explode(DamageTarget, Hit.ImpactPoint);
			return;
		}

		TimeSinceProximityCheck += DeltaSeconds;
		if (TimeSinceProximityCheck >= LaunchData.ProximityCheckInterval)
		{
			TimeSinceProximityCheck = 0.0f;
			if (AActor* Target = FindProximityTarget())
			{
				Explode(Target, End);
				return;
			}
		}
	}

	const FVector MovementDirection = (End - Start).GetSafeNormal(
		SMALL_NUMBER, GetDirectionAtDistance(NewDistanceTraveled));
	SetActorLocationAndRotation(End, MovementDirection.Rotation(), false, nullptr, ETeleportType::None);
	DistanceTraveled = NewDistanceTraveled;
	if (LaunchData.bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Cyan, false, 0.15f, 0, 1.0f);
	}
	if (HasAuthority() && DistanceTraveled >= LaunchData.MaxTravelDistance - KINDA_SMALL_NUMBER)
	{
		Explode(nullptr, End);
	}
}

bool ARocketProjectile::CheckPhysicalCollision(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RocketPhysicalCollision), false, this);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(GetInstigator());
	return GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(PhysicalCollisionRadius), Params);
}

AActor* ARocketProjectile::FindProximityTarget() const
{
	if (LaunchData.ProximityRadius <= 0.0f)
	{
		return nullptr;
	}

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RocketProximity), false, this);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(GetInstigator());

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectTypes,
		FCollisionShape::MakeSphere(LaunchData.ProximityRadius), Params);

	AActor* ClosestTarget = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const UHealthComponent* Health = Candidate ? Candidate->FindComponentByClass<UHealthComponent>() : nullptr;
		if (!Health || Health->IsDead())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestTarget = Candidate;
		}
	}

	if (LaunchData.bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), LaunchData.ProximityRadius, 16,
			ClosestTarget ? FColor::Green : FColor::Silver, false, LaunchData.ProximityCheckInterval);
	}
	return ClosestTarget;
}

void ARocketProjectile::Explode(AActor* DamageTarget, const FVector& ImpactLocation)
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}

	bExploded = true;
	ExplosionLocation = ImpactLocation;
	SetActorLocation(ImpactLocation);
	if (DamageTarget)
	{
		UGameplayStatics::ApplyDamage(DamageTarget, LaunchData.Damage, GetInstigatorController(), this, nullptr);
	}
	PlayExplosionCosmetics();
	ForceNetUpdate();
	SetLifeSpan(ExplosionReplicationDelay);
}

void ARocketProjectile::OnRep_Exploded()
{
	if (bExploded)
	{
		SetActorLocation(ExplosionLocation);
		PlayExplosionCosmetics();
	}
}

void ARocketProjectile::PlayExplosionCosmetics()
{
	RocketMesh->SetVisibility(false, true);
	RocketTrail->Deactivate();
	if (ExplosionEffect)
	{
		UNiagaraComponent* ExplosionComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionEffect, ExplosionLocation, FRotator::ZeroRotator,
			FVector::OneVector, true, false);
		if (ExplosionComponent)
		{
			// Niagara sprite size is a full width/diameter. The Niagara system can
			// multiply this gameplay radius by two when it needs an exact diameter.
			ExplosionComponent->SetVariableFloat(TEXT("User.ExplosionRadius"), LaunchData.ProximityRadius);
			ExplosionComponent->Activate(true);
		}
	}
	if (LaunchData.bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), ExplosionLocation, 150.0f, 16, FColor::Orange, false, 1.0f, 0, 3.0f);
	}
}

void ARocketProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnRocketFinished.Broadcast(this);
	if (bCountedOnServer)
	{
		ServerActiveRocketCount = FMath::Max(0, ServerActiveRocketCount - 1);
		bCountedOnServer = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ARocketProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARocketProjectile, LaunchData);
	DOREPLIFETIME(ARocketProjectile, bExploded);
	DOREPLIFETIME(ARocketProjectile, ExplosionLocation);
}
