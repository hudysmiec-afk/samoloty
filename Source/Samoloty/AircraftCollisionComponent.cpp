#include "AircraftCollisionComponent.h"

#include "HealthComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

const FName UAircraftCollisionComponent::DamageHitboxTag(TEXT("DamageHitbox"));
const FName UAircraftCollisionComponent::MovementBodyTag(TEXT("AircraftMovementBody"));

UAircraftCollisionComponent::UAircraftCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAircraftCollisionComponent::SetMovementCapsule(UCapsuleComponent* InMovementCapsule)
{
	MovementCapsule = InMovementCapsule;
	if (MovementCapsule)
	{
		// Movement collision is evaluated manually. It must never intercept weapon traces.
		MovementCapsule->ComponentTags.AddUnique(MovementBodyTag);
		MovementCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MovementCapsule->SetCollisionObjectType(ECC_Vehicle);
		MovementCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		MovementCapsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
		MovementCapsule->SetGenerateOverlapEvents(false);
		MovementCapsule->SetCanEverAffectNavigation(false);
		MovementCapsule->SetHiddenInGame(true);
		MovementCapsule->ShapeColor = FColor::Green;
	}
}

void UAircraftCollisionComponent::ConfigureDamageHitbox(UShapeComponent* Hitbox)
{
	if (!Hitbox)
	{
		return;
	}
	Hitbox->ComponentTags.AddUnique(DamageHitboxTag);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionObjectType(ECC_Pawn);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->SetCanEverAffectNavigation(false);
	Hitbox->SetHiddenInGame(true);
	Hitbox->ShapeColor = FColor::Red;
}

bool UAircraftCollisionComponent::SweepMovementCapsule(const FVector& Start,
	const FVector& End, FHitResult& OutHit) const
{
	if (!GetWorld() || !MovementCapsule)
	{
		return false;
	}
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AircraftMovementCapsule), false, GetOwner());
	QueryParams.AddIgnoredActor(GetOwner());
	TArray<FHitResult> Hits;
	if (!GetWorld()->SweepMultiByObjectType(Hits, Start, End,
		MovementCapsule->GetComponentQuat(), ObjectTypes,
		FCollisionShape::MakeCapsule(MovementCapsule->GetScaledCapsuleRadius(),
			MovementCapsule->GetScaledCapsuleHalfHeight()),
		QueryParams))
	{
		return false;
	}
	// Damage shapes may be much wider than the physical body. They belong only to
	// weapon queries; movement uses terrain and rounded movement bodies.
	for (const FHitResult& Candidate : Hits)
	{
		const UPrimitiveComponent* HitComponent = Candidate.GetComponent();
		if (HitComponent && HitComponent->ComponentHasTag(DamageHitboxTag))
		{
			continue;
		}
		OutHit = Candidate;
		return true;
	}
	return false;
}

FVector UAircraftCollisionComponent::MoveOwner(const FVector& RequestedMove,
	const FVector& RequestedVelocity, const bool bEnableImpactResponse)
{
	AActor* Owner = GetOwner();
	if (!Owner || RequestedMove.IsNearlyZero())
	{
		return RequestedVelocity;
	}
	if (!MovementCapsule || !GetWorld())
	{
		Owner->AddActorWorldOffset(RequestedMove, false, nullptr, ETeleportType::None);
		return RequestedVelocity;
	}

	const FVector CapsuleStart = MovementCapsule->GetComponentLocation();
	FHitResult Hit;
	if (!SweepMovementCapsule(CapsuleStart, CapsuleStart + RequestedMove, Hit))
	{
		Owner->AddActorWorldOffset(RequestedMove, false, nullptr, ETeleportType::None);
		return RequestedVelocity;
	}

	const float SafeMoveAlpha = FMath::Clamp(Hit.Time - 0.001f, 0.0f, 1.0f);
	Owner->AddActorWorldOffset(RequestedMove * SafeMoveAlpha,
		false, nullptr, ETeleportType::None);
	if (!bEnableImpactResponse)
	{
		return FVector::ZeroVector;
	}

	const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
	FVector RelativeVelocity = RequestedVelocity;
	const AActor* HitActor = Hit.GetActor();
	const UHealthComponent* OtherHealth = HitActor
		? HitActor->FindComponentByClass<UHealthComponent>() : nullptr;
	const bool bOtherActorDamageable = OtherHealth && !OtherHealth->IsDead();
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const bool bAircraftBodyCollision = HitComponent
		&& HitComponent->ComponentHasTag(MovementBodyTag);
	const bool bUseSoftCollision = bDamageableActorsUseSoftCollision
		&& bOtherActorDamageable && !bAircraftBodyCollision;
	const float ImpactSpeed = FMath::Max(
		0.0f, -FVector::DotProduct(RelativeVelocity, SurfaceNormal));

	if (bUseSoftCollision)
	{
		// Combat hitboxes receive damage but never become invisible walls for aircraft.
		Owner->AddActorWorldOffset(RequestedMove * (1.0f - SafeMoveAlpha),
			false, nullptr, ETeleportType::None);
		HandleImpact(Hit, ImpactSpeed, true);
		return RequestedVelocity;
	}

	Owner->AddActorWorldOffset(SurfaceNormal * SeparationDistance,
		false, nullptr, ETeleportType::None);
	const float ActiveSeparationDistance = bAircraftBodyCollision
		? AircraftSeparationDistance : SeparationDistance;
	// Replace the initial world separation with the stronger aircraft separation.
	if (bAircraftBodyCollision && ActiveSeparationDistance > SeparationDistance)
	{
		Owner->AddActorWorldOffset(SurfaceNormal
			* (ActiveSeparationDistance - SeparationDistance), false, nullptr,
			ETeleportType::None);
	}
	const float ActiveRestitution = bAircraftBodyCollision
		? AircraftRestitution : Restitution;
	const float ActiveSpeedRetention = bAircraftBodyCollision
		? AircraftSurfaceSpeedRetention : SurfaceSpeedRetention;
	const float IntoSurfaceSpeed = FVector::DotProduct(RequestedVelocity, SurfaceNormal);
	const FVector TangentialVelocity = RequestedVelocity - SurfaceNormal * IntoSurfaceSpeed;
	FVector PostCollisionVelocity = TangentialVelocity * ActiveSpeedRetention;
	if (IntoSurfaceSpeed < 0.0f)
	{
		PostCollisionVelocity += SurfaceNormal * (-IntoSurfaceSpeed * ActiveRestitution);
	}
	if (PostCollisionVelocity.IsNearlyZero())
	{
		PostCollisionVelocity = SurfaceNormal * FMath::Max(100.0f,
			RequestedVelocity.Size() * ActiveRestitution);
	}

	const float StepSeconds = RequestedVelocity.Size() > UE_SMALL_NUMBER
		? RequestedMove.Size() / RequestedVelocity.Size() : 0.0f;
	const FVector RemainingMove = PostCollisionVelocity * StepSeconds
		* FMath::Clamp(1.0f - Hit.Time, 0.0f, 1.0f);
	if (!RemainingMove.IsNearlyZero())
	{
		FHitResult SlideHit;
		const FVector SlideStart = MovementCapsule->GetComponentLocation();
		const bool bSlideBlocked = SweepMovementCapsule(
			SlideStart, SlideStart + RemainingMove, SlideHit);
		const float SlideAlpha = bSlideBlocked
			? FMath::Clamp(SlideHit.Time - 0.001f, 0.0f, 1.0f) : 1.0f;
		Owner->AddActorWorldOffset(RemainingMove * SlideAlpha,
			false, nullptr, ETeleportType::None);
	}

	if (!PostCollisionVelocity.IsNearlyZero())
	{
		const FRotator DirectionRotation = PostCollisionVelocity.Rotation();
		Owner->SetActorRotation(FRotator(DirectionRotation.Pitch, DirectionRotation.Yaw, 0.0f));
	}
	HandleImpact(Hit, ImpactSpeed, bOtherActorDamageable);
	return PostCollisionVelocity;
}

void UAircraftCollisionComponent::HandleImpact(const FHitResult& Hit,
	const float ImpactSpeed, const bool bDamageableActorCollision)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float DamageAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(MinDamageSpeed, FMath::Max(MinDamageSpeed + 1.0f, MaxDamageSpeed)),
		FVector2D(0.0f, 1.0f), ImpactSpeed);
	if (ImpactSpeed >= MinEffectSpeed && Now >= NextEffectTime)
	{
		NextEffectTime = Now + EffectCooldown;
		MulticastPlayImpactEffects(Hit.ImpactPoint, Hit.ImpactNormal,
			FMath::Clamp(DamageAlpha, 0.15f, 1.0f));
	}
	if (ImpactSpeed < MinDamageSpeed || Now < NextDamageTime)
	{
		return;
	}
	NextDamageTime = Now + DamageCooldown;
	const float DamageFraction = FMath::Lerp(
		MinSelfDamageFraction, MaxSelfDamageFraction, DamageAlpha);
	APawn* OwnerPawn = Cast<APawn>(Owner);
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	if (AActor* OtherActor = Hit.GetActor(); IsValid(OtherActor) && OtherActor != Owner)
	{
		if (UHealthComponent* Health = OtherActor->FindComponentByClass<UHealthComponent>();
			Health && !Health->IsDead())
		{
			UGameplayStatics::ApplyDamage(OtherActor,
				Health->GetMaxHealth() * DamageFraction * OtherActorDamageMultiplier,
				InstigatorController, Owner, nullptr);
		}
	}
	if (UHealthComponent* SelfHealth = Owner->FindComponentByClass<UHealthComponent>();
		SelfHealth && !SelfHealth->IsDead())
	{
		const float SelfMultiplier = bDamageableActorCollision
			? DamageableActorSelfDamageMultiplier : 1.0f;
		UGameplayStatics::ApplyDamage(Owner,
			SelfHealth->GetMaxHealth() * DamageFraction * SelfMultiplier,
			InstigatorController, Owner, nullptr);
	}
}

void UAircraftCollisionComponent::MulticastPlayImpactEffects_Implementation(
	const FVector_NetQuantize ImpactPoint, const FVector_NetQuantizeNormal ImpactNormal,
	const float Intensity)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ImpactEffect)
	{
		if (UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ImpactEffect, ImpactPoint, FVector(ImpactNormal).Rotation()))
		{
			Effect->SetVariableFloat(TEXT("User.ImpactStrength"), Intensity);
		}
	}
	if (ImpactSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ImpactSound, ImpactPoint,
			FRotator::ZeroRotator, FMath::Lerp(0.6f, 1.0f, Intensity));
	}
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (ImpactCameraShake && OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (const APlayerController* PlayerController =
			Cast<APlayerController>(OwnerPawn->GetController());
			PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraShake(
				ImpactCameraShake, Intensity);
		}
	}
}
