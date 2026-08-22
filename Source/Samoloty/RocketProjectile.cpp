#include "RocketProjectile.h"

#include "EvasiveRollComponent.h"
#include "HealthComponent.h"
#include "MissileWarningComponent.h"
#include "RocketSimulationManager.h"
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

namespace
{
	bool IsActorEvadingMissiles(const AActor* Actor)
	{
		const UEvasiveRollComponent* Roll = Actor
			? Actor->FindComponentByClass<UEvasiveRollComponent>() : nullptr;
		return Roll && Roll->IsEvadingMissiles();
	}
}

ARocketProjectile::ARocketProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
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
	if (URocketSimulationManager* Manager =
		GetWorld()->GetSubsystem<URocketSimulationManager>())
	{
		SimulationManagerHandle = Manager->RegisterRocket(this, LaunchData, GetOwner());
	}
	ApplyLaunchState();
	if (LaunchData.GuidanceMode == ERocketGuidanceMode::Homing)
	{
		UpdateHomingNetState();
	}
	if (HasActorBegunPlay() && !bCountedOnServer)
	{
		++ServerActiveRocketCount;
		bCountedOnServer = true;
	}
	ForceNetUpdate();
}

void ARocketProjectile::InitializeManagedVisual(const FRocketLaunchData& InLaunchData)
{
	LaunchData = InLaunchData;
	bInitialized = true;
	bManagedVisualOnly = true;
	ApplyLaunchState();
	if (URocketSimulationManager* Manager =
		GetWorld()->GetSubsystem<URocketSimulationManager>())
	{
		SimulationManagerHandle = Manager->RegisterRocket(this, LaunchData, nullptr);
	}
	const float RemainingLifetime = LaunchData.Speed > KINDA_SMALL_NUMBER
		? LaunchData.MaxTravelDistance / LaunchData.Speed + 2.0f : 2.0f;
	SetLifeSpan(RemainingLifetime);
}

void ARocketProjectile::PresentManagedExplosion(const FCombatImpactEvent& Impact)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	ExplosionImpact = Impact;
	FVector ResolvedLocation;
	FVector ResolvedNormal;
	ExplosionImpact.Resolve(ResolvedLocation, ResolvedNormal);
	SetActorLocation(ResolvedLocation);
	PlayExplosionCosmetics();
	SetLifeSpan(ExplosionReplicationDelay);
}

void ARocketProjectile::PlayManagedExplosionCosmetics(UWorld* World,
	const FRocketLaunchData& InLaunchData, const FCombatImpactEvent& Impact) const
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FVector Location;
	FVector Normal;
	Impact.Resolve(Location, Normal);
	if (ExplosionSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(World, ExplosionSound, Location);
	}
	if (ExplosionEffect)
	{
		UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, ExplosionEffect, Location, FRotator::ZeroRotator,
			FVector::OneVector, true, false);
		if (Component)
		{
			Component->SetVariableFloat(TEXT("User.ExplosionRadius"),
				FMath::Max(InLaunchData.ProximityRadius, MinimumExplosionVisualRadius));
			Component->Activate(true);
		}
	}
	if (InLaunchData.bDrawDebug)
	{
		DrawDebugSphere(World, Location, 150.0f, 16, FColor::Orange,
			false, 1.0f, 0, 3.0f);
	}
}

void ARocketProjectile::OnRep_LaunchData()
{
	bInitialized = true;
	if (SimulationManagerHandle == INDEX_NONE)
	{
		if (URocketSimulationManager* Manager =
			GetWorld()->GetSubsystem<URocketSimulationManager>())
		{
			SimulationManagerHandle = Manager->RegisterRocket(this, LaunchData, GetOwner());
		}
	}
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
	CurrentHomingDirection = Direction.GetSafeNormal(SMALL_NUMBER, FVector(LaunchData.Direction));
	SetActorLocationAndRotation(Location, Direction.Rotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
	RefreshMissileWarningRegistration();
}

void ARocketProjectile::OnRep_HomingDisabled()
{
	RefreshMissileWarningRegistration();
}

void ARocketProjectile::SimulateFromManager(const float DeltaSeconds)
{
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
	if (LaunchData.GuidanceMode == ERocketGuidanceMode::Homing)
	{
		SimulateHomingMovement(DeltaSeconds);
		return;
	}

	const FVector Start = GetActorLocation();
	const float RemainingDistance = FMath::Max(0.0f, LaunchData.MaxTravelDistance - DistanceTraveled);
	const float StepDistance = FMath::Min(LaunchData.Speed * DeltaSeconds, RemainingDistance);
	const float NewDistanceTraveled = DistanceTraveled + StepDistance;
	const FVector End = GetLocationAtDistance(NewDistanceTraveled);

	if (HasAuthority() && !bManagedVisualOnly)
	{
		FHitResult Hit;
		if (CheckPhysicalCollision(Start, End, Hit))
		{
			if (!IsActorEvadingMissiles(Hit.GetActor()))
			{
				AActor* DamageTarget = Hit.GetActor()
					&& Hit.GetActor()->FindComponentByClass<UHealthComponent>()
					? Hit.GetActor() : nullptr;
				Explode(DamageTarget, Hit.ImpactPoint, &Hit);
				return;
			}
		}

		TimeSinceProximityCheck += DeltaSeconds;
		if (TimeSinceProximityCheck >= LaunchData.ProximityCheckInterval)
		{
			TimeSinceProximityCheck = 0.0f;
			if (AActor* Target = FindProximityTarget(End))
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
	if (HasAuthority() && !bManagedVisualOnly
		&& DistanceTraveled >= LaunchData.MaxTravelDistance - KINDA_SMALL_NUMBER)
	{
		Explode(nullptr, End);
	}
}

void ARocketProjectile::SimulateHomingMovement(const float DeltaSeconds)
{
	const FVector Start = GetActorLocation();
	const float RemainingDistance = FMath::Max(0.0f, LaunchData.MaxTravelDistance - DistanceTraveled);
	const float StepDistance = FMath::Min(LaunchData.Speed * DeltaSeconds, RemainingDistance);
	const float NewDistanceTraveled = DistanceTraveled + StepDistance;
	const bool bFollowingLaunchCurve = LaunchData.bUseSeparationCurve
		&& DistanceTraveled < SeparationCurveLength;

	FVector MovementDirection = CurrentHomingDirection.GetSafeNormal(
		SMALL_NUMBER, FVector(LaunchData.Direction));
	FVector End;
	if (bFollowingLaunchCurve)
	{
		End = GetLocationAtDistance(NewDistanceTraveled);
		MovementDirection = (End - Start).GetSafeNormal(
			SMALL_NUMBER, GetDirectionAtDistance(NewDistanceTraveled));
	}
	else
	{
		if (const AActor* Target = GetLiveHomingTarget())
		{
			const FVector DesiredDirection =
				(Target->GetActorLocation() - Start).GetSafeNormal(SMALL_NUMBER, MovementDirection);
			MovementDirection = RotateDirectionTowards(
				MovementDirection, DesiredDirection, DeltaSeconds);
		}
		End = Start + MovementDirection * StepDistance;
	}

	if (HasAuthority())
	{
		FHitResult Hit;
		if (CheckPhysicalCollision(Start, End, Hit))
		{
			if (IsActorEvadingMissiles(Hit.GetActor()))
			{
				BreakHomingAfterEvade(Hit.GetActor());
			}
			else
			{
				AActor* DamageTarget = Hit.GetActor()
					&& Hit.GetActor()->FindComponentByClass<UHealthComponent>()
					? Hit.GetActor() : nullptr;
				Explode(DamageTarget, Hit.ImpactPoint, &Hit);
				return;
			}
		}

		TimeSinceProximityCheck += DeltaSeconds;
		if (TimeSinceProximityCheck >= LaunchData.ProximityCheckInterval)
		{
			TimeSinceProximityCheck = 0.0f;
			if (AActor* Target = FindProximityTarget(End))
			{
				if (IsActorEvadingMissiles(Target))
				{
					BreakHomingAfterEvade(Target);
				}
				else
				{
					Explode(Target, End);
					return;
				}
			}
		}
	}
	else if (!bFollowingLaunchCurve && bHasHomingCorrection)
	{
		HomingCorrectionLocation += HomingCorrectionDirection * LaunchData.Speed * DeltaSeconds;
		const float CorrectionAlpha = 1.0f - FMath::Exp(-8.0f * DeltaSeconds);
		End = FMath::Lerp(End, HomingCorrectionLocation, CorrectionAlpha);
		MovementDirection = FMath::Lerp(
			MovementDirection, HomingCorrectionDirection, CorrectionAlpha).GetSafeNormal();
	}

	CurrentHomingDirection = MovementDirection.GetSafeNormal(
		SMALL_NUMBER, FVector(LaunchData.Direction));
	SetActorLocationAndRotation(End, CurrentHomingDirection.Rotation(), false, nullptr,
		ETeleportType::None);
	DistanceTraveled = NewDistanceTraveled;

	if (LaunchData.bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Magenta, false, 0.15f, 0, 1.5f);
	}
	if (HasAuthority())
	{
		HomingNetUpdateAccumulator += DeltaSeconds;
		if (HomingNetUpdateAccumulator >= 0.1f)
		{
			HomingNetUpdateAccumulator = 0.0f;
			UpdateHomingNetState();
		}
		if (DistanceTraveled >= LaunchData.MaxTravelDistance - KINDA_SMALL_NUMBER)
		{
			Explode(nullptr, End);
		}
	}
}

FVector ARocketProjectile::RotateDirectionTowards(const FVector& CurrentDirection,
	const FVector& DesiredDirection, const float DeltaSeconds) const
{
	const FVector Current = CurrentDirection.GetSafeNormal();
	const FVector Desired = DesiredDirection.GetSafeNormal();
	if (Current.IsNearlyZero() || Desired.IsNearlyZero())
	{
		return CurrentDirection;
	}

	const float AngleRadians = FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Current, Desired), -1.0f, 1.0f));
	if (AngleRadians <= KINDA_SMALL_NUMBER)
	{
		return Desired;
	}
	const float MaxStepRadians = FMath::DegreesToRadians(
		FMath::Max(0.0f, LaunchData.MaxTurnRateDegreesPerSecond)) * DeltaSeconds;
	const float Alpha = FMath::Clamp(MaxStepRadians / AngleRadians, 0.0f, 1.0f);
	const FQuat DeltaRotation = FQuat::FindBetweenNormals(Current, Desired);
	return FQuat::Slerp(FQuat::Identity, DeltaRotation, Alpha)
		.RotateVector(Current).GetSafeNormal();
}

void ARocketProjectile::UpdateHomingNetState()
{
	if (!HasAuthority() || LaunchData.GuidanceMode != ERocketGuidanceMode::Homing)
	{
		return;
	}
	HomingNetState.Location = GetActorLocation();
	HomingNetState.Direction = CurrentHomingDirection.GetSafeNormal(
		SMALL_NUMBER, GetActorForwardVector());
	HomingNetState.DistanceTraveled = DistanceTraveled;
	HomingNetState.ServerTime = GetWorld()->GetTimeSeconds();
}

void ARocketProjectile::OnRep_HomingNetState()
{
	if (HasAuthority() || LaunchData.GuidanceMode != ERocketGuidanceMode::Homing || !GetWorld())
	{
		return;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double ServerNow = GameState
		? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Elapsed = FMath::Max(0.0f,
		static_cast<float>(ServerNow - HomingNetState.ServerTime));
	HomingCorrectionDirection = FVector(HomingNetState.Direction).GetSafeNormal(
		SMALL_NUMBER, GetActorForwardVector());
	HomingCorrectionLocation = FVector(HomingNetState.Location)
		+ HomingCorrectionDirection * LaunchData.Speed * Elapsed;
	DistanceTraveled = FMath::Max(DistanceTraveled,
		HomingNetState.DistanceTraveled + LaunchData.Speed * Elapsed);
	bHasHomingCorrection = true;
	if (FVector::DistSquared(GetActorLocation(), HomingCorrectionLocation) > FMath::Square(5000.0f))
	{
		SetActorLocationAndRotation(HomingCorrectionLocation,
			HomingCorrectionDirection.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		CurrentHomingDirection = HomingCorrectionDirection;
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

AActor* ARocketProjectile::FindProximityTarget(const FVector& QueryLocation) const
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

	if (LaunchData.GuidanceMode == ERocketGuidanceMode::Homing)
	{
		AActor* AssignedTarget = GetLiveHomingTarget();
		return AssignedTarget && FVector::DistSquared(QueryLocation, AssignedTarget->GetActorLocation())
			<= FMath::Square(LaunchData.ProximityRadius) ? AssignedTarget : nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, QueryLocation, FQuat::Identity, ObjectTypes,
		FCollisionShape::MakeSphere(LaunchData.ProximityRadius), Params);

	AActor* ClosestTarget = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const UHealthComponent* Health = Candidate ? Candidate->FindComponentByClass<UHealthComponent>() : nullptr;
		if (!Health || Health->IsDead() || IsActorEvadingMissiles(Candidate))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(QueryLocation, Candidate->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestTarget = Candidate;
		}
	}

	if (LaunchData.bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), QueryLocation, LaunchData.ProximityRadius, 16,
			ClosestTarget ? FColor::Green : FColor::Silver, false, LaunchData.ProximityCheckInterval);
	}
	return ClosestTarget;
}

AActor* ARocketProjectile::GetLiveHomingTarget() const
{
	if (bHomingDisabled)
	{
		return nullptr;
	}
	AActor* Target = LaunchData.HomingTarget.Get();
	const UHealthComponent* Health = Target ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
	return Health && !Health->IsDead() ? Target : nullptr;
}

void ARocketProjectile::BreakHomingAfterEvade(AActor* EvadingActor)
{
	if (!HasAuthority() || bHomingDisabled
		|| LaunchData.GuidanceMode != ERocketGuidanceMode::Homing
		|| EvadingActor != LaunchData.HomingTarget.Get())
	{
		return;
	}

	// Only the missile which has actually reached the protected aircraft loses
	// guidance. Other missiles keep tracking regardless of how far away they are.
	bHomingDisabled = true;
	RefreshMissileWarningRegistration();
	ForceNetUpdate();
}

void ARocketProjectile::RefreshMissileWarningRegistration()
{
	AActor* DesiredTarget = !bExploded && !bHomingDisabled
		&& LaunchData.GuidanceMode == ERocketGuidanceMode::Homing
		? LaunchData.HomingTarget.Get() : nullptr;
	if (RegisteredWarningTarget.Get() == DesiredTarget)
	{
		return;
	}
	ClearMissileWarningRegistration();
	if (DesiredTarget)
	{
		if (UMissileWarningComponent* Warning =
			DesiredTarget->FindComponentByClass<UMissileWarningComponent>())
		{
			Warning->AddIncomingMissile(this);
			RegisteredWarningTarget = DesiredTarget;
		}
	}
}

void ARocketProjectile::ClearMissileWarningRegistration()
{
	if (AActor* PreviousTarget = RegisteredWarningTarget.Get())
	{
		if (UMissileWarningComponent* Warning =
			PreviousTarget->FindComponentByClass<UMissileWarningComponent>())
		{
			Warning->RemoveIncomingMissile(this);
		}
	}
	RegisteredWarningTarget.Reset();
}

void ARocketProjectile::Explode(AActor* DamageTarget, const FVector& ImpactLocation,
	const FHitResult* PhysicalHit)
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}

	bExploded = true;
	ClearMissileWarningRegistration();
	ExplosionImpact = PhysicalHit
		? FCombatImpactEvent::MakeFromHit(1, 0, *PhysicalHit,
			-GetActorForwardVector())
		: (DamageTarget
			? FCombatImpactEvent::MakeOnTarget(1, 0, DamageTarget, ImpactLocation,
				(ImpactLocation - DamageTarget->GetActorLocation()).GetSafeNormal(
					UE_SMALL_NUMBER, -GetActorForwardVector()))
			: FCombatImpactEvent::MakeMiss(1, 0, ImpactLocation, GetActorForwardVector()));
	FVector ResolvedLocation;
	FVector ResolvedNormal;
	ExplosionImpact.Resolve(ResolvedLocation, ResolvedNormal);
	ExplosionLocation = ResolvedLocation;
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
		ClearMissileWarningRegistration();
		FVector ResolvedLocation;
		FVector ResolvedNormal;
		ExplosionImpact.Resolve(ResolvedLocation, ResolvedNormal);
		ExplosionLocation = ResolvedLocation;
		SetActorLocation(ResolvedLocation);
		PlayExplosionCosmetics();
	}
}

void ARocketProjectile::PlayExplosionCosmetics()
{
	FVector ResolvedLocation;
	FVector ResolvedNormal;
	ExplosionImpact.Resolve(ResolvedLocation, ResolvedNormal);
	ExplosionLocation = ResolvedLocation;
	RocketMesh->SetVisibility(false, true);
	RocketTrail->Deactivate();
	if (ExplosionSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ExplosionSound, ExplosionLocation);
	}
	if (ExplosionEffect)
	{
		UNiagaraComponent* ExplosionComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionEffect, ExplosionLocation, FRotator::ZeroRotator,
			FVector::OneVector, true, false);
		if (ExplosionComponent)
		{
			// Niagara sprite size is a full width/diameter. The Niagara system can
			// multiply this gameplay radius by two when it needs an exact diameter.
			const float VisualExplosionRadius =
				FMath::Max(LaunchData.ProximityRadius, MinimumExplosionVisualRadius);
			ExplosionComponent->SetVariableFloat(
				TEXT("User.ExplosionRadius"), VisualExplosionRadius);
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
	if (SimulationManagerHandle != INDEX_NONE)
	{
		if (URocketSimulationManager* Manager =
			GetWorld()->GetSubsystem<URocketSimulationManager>())
		{
			Manager->UnregisterRocket(SimulationManagerHandle);
		}
		SimulationManagerHandle = INDEX_NONE;
	}
	ClearMissileWarningRegistration();
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
	DOREPLIFETIME(ARocketProjectile, ExplosionImpact);
	DOREPLIFETIME(ARocketProjectile, HomingNetState);
	DOREPLIFETIME(ARocketProjectile, bHomingDisabled);
}
