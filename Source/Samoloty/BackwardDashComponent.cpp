#include "BackwardDashComponent.h"

#include "ArcadeFlightComponent.h"
#include "HealthComponent.h"
#include "ForwardDashComponent.h"
#include "QuickReversalComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace BackwardDashTuning
{
	constexpr double BackwardTravelDuration = 1.50;
	constexpr double ForwardRecoveryDuration = 0.35;
	constexpr double Duration = BackwardTravelDuration + ForwardRecoveryDuration;
	constexpr double Cooldown = 4.0;
	constexpr float TravelDistance = 20000.0f;
	constexpr float LateralVelocityResponse = 5.0f;
}

UBackwardDashComponent::UBackwardDashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UBackwardDashComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
		CachedQuickReversalComponent = Owner->FindComponentByClass<UQuickReversalComponent>();
		CachedForwardDashComponent = Owner->FindComponentByClass<UForwardDashComponent>();
		CachedHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
	}
}

void UBackwardDashComponent::RequestActivation()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()
		|| GetLocalTime() < LocalNextAllowedTime)
	{
		return;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead())
		|| (CachedQuickReversalComponent && CachedQuickReversalComponent->IsReversalActive())
		|| (CachedForwardDashComponent && CachedForwardDashComponent->IsDashActive()))
	{
		return;
	}

	const double Now = GetLocalTime();
	LocalNextAllowedTime = Now + BackwardDashTuning::Cooldown;
	bLocalActivationPending = true;
	if (GetOwner()->HasAuthority())
	{
		if (CanActivate())
		{
			StartAuthoritativeDash();
		}
		else
		{
			LocalNextAllowedTime = Now;
			bLocalActivationPending = false;
		}
		return;
	}
	ServerRequestActivation();
}

bool UBackwardDashComponent::TryActivateFromAbilityQueue()
{
	if (!CanActivate())
	{
		return false;
	}
	StartAuthoritativeDash();
	return true;
}

void UBackwardDashComponent::ServerRequestActivation_Implementation()
{
	if (!CanActivate())
	{
		ClientRejectActivation();
		return;
	}
	StartAuthoritativeDash();
}

void UBackwardDashComponent::ClientRejectActivation_Implementation()
{
	LocalNextAllowedTime = GetLocalTime();
	bLocalActivationPending = false;
}

bool UBackwardDashComponent::CanActivate() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || DashState.bActive
		|| GetSynchronizedTime() < NextAllowedServerTime)
	{
		return false;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead())
		|| (CachedQuickReversalComponent && CachedQuickReversalComponent->IsReversalActive())
		|| (CachedForwardDashComponent && CachedForwardDashComponent->IsDashActive()))
	{
		return false;
	}
	return true;
}

void UBackwardDashComponent::StartAuthoritativeDash()
{
	const double Now = GetSynchronizedTime();
	DashState.bActive = true;
	DashState.ServerStartTime = Now;
	++DashState.Sequence;
	bLocalActivationPending = false;
	InitialFlightRotation = GetOwner()->GetActorQuat().GetNormalized();
	DashState.InitialFlightRotation = InitialFlightRotation.Rotator();
	const FVector InitialVelocity = CachedFlightComponent
		? CachedFlightComponent->GetCurrentVelocity() : FVector::ZeroVector;
	const FVector InitialForward = InitialFlightRotation.GetForwardVector();
	CurrentLateralVelocity = InitialVelocity
		- InitialForward * FVector::DotProduct(InitialVelocity, InitialForward);
	PreviousTravelDistance = 0.0f;
	NextAllowedServerTime = Now + BackwardDashTuning::Cooldown;
	MulticastPlayBackwardDashSound();
	GetOwner()->ForceNetUpdate();
}

void UBackwardDashComponent::MulticastPlayBackwardDashSound_Implementation()

{
	PlayPredictedActivationEffect();
}

void UBackwardDashComponent::PlayPredictedActivationEffect()
{
	if (BackwardDashSound && GetNetMode() != NM_DedicatedServer && GetOwner()
		&& GetOwner()->GetRootComponent())
	{
		UGameplayStatics::SpawnSoundAttached(
			BackwardDashSound, GetOwner()->GetRootComponent());
	}
}

bool UBackwardDashComponent::IsDashActive() const
{
	return CachedFlightComponent && CachedFlightComponent->IsBackwardDashActive();
}

bool UBackwardDashComponent::GetAuthoritativeFlightRotation(FQuat& OutRotation) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DashState.bActive)
	{
		return false;
	}
	OutRotation = InitialFlightRotation;
	return true;
}

bool UBackwardDashComponent::ConsumeAuthoritativeMovement(
	const float DeltaTime, const float NormalForwardSpeed,
	const FVector& DesiredLateralVelocity,
	FVector& OutVelocity, float& OutSignedForwardSpeed)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DashState.bActive)
	{
		return false;
	}

	const double Elapsed = FMath::Max(
		0.0, GetSynchronizedTime() - DashState.ServerStartTime);
	const float BackwardProgress = FMath::Clamp(static_cast<float>(
		Elapsed / BackwardDashTuning::BackwardTravelDuration), 0.0f, 1.0f);
	const float TravelAlpha = FMath::SmoothStep(0.0f, 1.0f, BackwardProgress);
	const float TravelDistance = BackwardDashTuning::TravelDistance * TravelAlpha;
	const float DistanceThisFrame = TravelDistance - PreviousTravelDistance;
	PreviousTravelDistance = TravelDistance;
	const FVector BackwardDirection = -InitialFlightRotation.GetForwardVector();
	const FVector DashVelocity = DeltaTime > UE_SMALL_NUMBER
		? BackwardDirection * (DistanceThisFrame / DeltaTime)
		: FVector::ZeroVector;
	CurrentLateralVelocity = FMath::VInterpTo(
		CurrentLateralVelocity, DesiredLateralVelocity, DeltaTime,
		BackwardDashTuning::LateralVelocityResponse);
	const float RecoveryProgress = FMath::Clamp(static_cast<float>(
		(Elapsed - BackwardDashTuning::BackwardTravelDuration)
		/ BackwardDashTuning::ForwardRecoveryDuration), 0.0f, 1.0f);
	const float RecoverySpeed = NormalForwardSpeed
		* FMath::SmoothStep(0.0f, 1.0f, RecoveryProgress);
	OutVelocity = DashVelocity
		+ InitialFlightRotation.GetForwardVector() * RecoverySpeed
		+ CurrentLateralVelocity;
	OutSignedForwardSpeed = FVector::DotProduct(
		OutVelocity, InitialFlightRotation.GetForwardVector());

	if (Elapsed >= BackwardDashTuning::Duration)
	{
		DashState.bActive = false;
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

void UBackwardDashComponent::OnRep_DashState()
{
	bLocalActivationPending = false;
	if (DashState.bActive)
	{
		InitialFlightRotation =
			DashState.InitialFlightRotation.Quaternion().GetNormalized();
		const double Elapsed = FMath::Max(
			0.0, GetSynchronizedTime() - DashState.ServerStartTime);
		const float BackwardProgress = FMath::Clamp(static_cast<float>(
			Elapsed / BackwardDashTuning::BackwardTravelDuration), 0.0f, 1.0f);
		PreviousTravelDistance = BackwardDashTuning::TravelDistance
			* FMath::SmoothStep(0.0f, 1.0f, BackwardProgress);
		LocalNextAllowedTime = FMath::Max(
			LocalNextAllowedTime,
			GetLocalTime() + BackwardDashTuning::Cooldown - Elapsed);
	}
}

float UBackwardDashComponent::GetCooldownRemaining() const
{
	return CachedFlightComponent
		? CachedFlightComponent->GetBackwardDashCooldownRemaining() : 0.0f;
}

float UBackwardDashComponent::GetManeuverProgress() const
{
	return CachedFlightComponent && CachedFlightComponent->IsBackwardDashActive()
		? CachedFlightComponent->GetMobilityProgress() : 0.0f;
}

double UBackwardDashComponent::GetSynchronizedTime() const
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

double UBackwardDashComponent::GetLocalTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UBackwardDashComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBackwardDashComponent, DashState);
}
