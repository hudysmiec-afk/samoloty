#include "ForwardDashComponent.h"

#include "ArcadeFlightComponent.h"
#include "BackwardDashComponent.h"
#include "HealthComponent.h"
#include "QuickReversalComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace ForwardDashTuning
{
	constexpr double Duration = 2.0;
	constexpr double Cooldown = 6.0;
	constexpr double AccelerationDuration = 0.20;
	constexpr float MaximumSpeedMultiplier = 4.0f;
}

UForwardDashComponent::UForwardDashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UForwardDashComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
		CachedBackwardDashComponent = Owner->FindComponentByClass<UBackwardDashComponent>();
		CachedQuickReversalComponent = Owner->FindComponentByClass<UQuickReversalComponent>();
		CachedHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
	}
}

bool UForwardDashComponent::TryActivateFromAbilityQueue()
{
	if (!CanActivate())
	{
		return false;
	}
	StartAuthoritativeDash();
	return true;
}

bool UForwardDashComponent::CanActivate() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || DashState.bActive
		|| GetSynchronizedTime() < NextAllowedServerTime)
	{
		return false;
	}
	return CachedFlightComponent && CachedFlightComponent->IsCombatFlightEnabled()
		&& (!CachedHealthComponent || !CachedHealthComponent->IsDead())
		&& (!CachedBackwardDashComponent || !CachedBackwardDashComponent->IsDashActive())
		&& (!CachedQuickReversalComponent || !CachedQuickReversalComponent->IsReversalActive());
}

void UForwardDashComponent::StartAuthoritativeDash()
{
	const double Now = GetSynchronizedTime();
	DashState.bActive = true;
	DashState.ServerStartTime = Now;
	++DashState.Sequence;
	InitialFlightRotation = GetOwner()->GetActorQuat().GetNormalized();
	DashState.InitialFlightRotation = InitialFlightRotation.Rotator();
	InitialForwardSpeed = CachedFlightComponent
		? FMath::Max(0.0f, FVector::DotProduct(
			CachedFlightComponent->GetCurrentVelocity(),
			InitialFlightRotation.GetForwardVector()))
		: 0.0f;
	NextAllowedServerTime = Now + ForwardDashTuning::Cooldown;
	MulticastPlayForwardDashSound();
	GetOwner()->ForceNetUpdate();
}

bool UForwardDashComponent::GetAuthoritativeFlightRotation(FQuat& OutRotation) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DashState.bActive)
	{
		return false;
	}
	OutRotation = InitialFlightRotation;
	return true;
}

bool UForwardDashComponent::ConsumeAuthoritativeMovement(
	const float NormalForwardSpeed, FVector& OutVelocity,
	float& OutSignedForwardSpeed)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DashState.bActive)
	{
		return false;
	}

	const double Elapsed = FMath::Max(
		0.0, GetSynchronizedTime() - DashState.ServerStartTime);
	const float AccelerationProgress = FMath::Clamp(static_cast<float>(
		Elapsed / ForwardDashTuning::AccelerationDuration), 0.0f, 1.0f);
	const float MaximumSpeed = GetMaximumSpeed(NormalForwardSpeed);
	const float Speed = FMath::Lerp(
		InitialForwardSpeed, MaximumSpeed,
		FMath::SmoothStep(0.0f, 1.0f, AccelerationProgress));
	OutVelocity = InitialFlightRotation.GetForwardVector() * Speed;
	OutSignedForwardSpeed = Speed;

	if (Elapsed >= ForwardDashTuning::Duration)
	{
		DashState.bActive = false;
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

void UForwardDashComponent::OnRep_DashState()
{
	if (DashState.bActive)
	{
		InitialFlightRotation =
			DashState.InitialFlightRotation.Quaternion().GetNormalized();
	}
}

void UForwardDashComponent::MulticastPlayForwardDashSound_Implementation()

{
	PlayPredictedActivationEffect();
}

void UForwardDashComponent::PlayPredictedActivationEffect()
{
	if (GetNetMode() == NM_DedicatedServer || !GetOwner())
	{
		return;
	}
	if (ForwardDashSound && GetOwner()->GetRootComponent())
	{
		UGameplayStatics::SpawnSoundAttached(
			ForwardDashSound, GetOwner()->GetRootComponent());
	}
	if (ForwardDashCameraShake && ForwardDashCameraShakeScale > 0.0f)
	{
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		const APlayerController* PlayerController = OwnerPawn && OwnerPawn->IsLocallyControlled()
			? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraShake(
				ForwardDashCameraShake, ForwardDashCameraShakeScale);
		}
	}
}

bool UForwardDashComponent::IsDashActive() const
{
	return CachedFlightComponent && CachedFlightComponent->IsForwardDashActive();
}

float UForwardDashComponent::GetCooldownRemaining() const
{
	return CachedFlightComponent
		? CachedFlightComponent->GetForwardDashCooldownRemaining() : 0.0f;
}

float UForwardDashComponent::GetManeuverProgress() const
{
	return CachedFlightComponent && CachedFlightComponent->IsForwardDashActive()
		? CachedFlightComponent->GetMobilityProgress() : 0.0f;
}

float UForwardDashComponent::GetMaximumSpeed(const float NormalForwardSpeed) const
{
	return NormalForwardSpeed * ForwardDashTuning::MaximumSpeedMultiplier;
}

double UForwardDashComponent::GetSynchronizedTime() const
{
	if (!GetWorld())
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return GetWorld()->GetTimeSeconds();
}

void UForwardDashComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForwardDashComponent, DashState);
}
