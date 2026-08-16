#include "QuickReversalComponent.h"

#include "ArcadeFlightComponent.h"
#include "BackwardDashComponent.h"
#include "EvasiveRollComponent.h"
#include "ForwardDashComponent.h"
#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace QuickReversalTuning
{
	constexpr double Duration = 1.20;
	constexpr double Cooldown = 4.0;
	constexpr float NeutralDirectionThreshold = 0.05f;
	constexpr float RotationStart = 0.02f;
	constexpr float RotationEnd = 0.62f;
	constexpr float UprightCorrectionStart = 0.62f;
	constexpr float UprightCorrectionEnd = 0.88f;
	constexpr float CoastEnd = 0.58f;
	constexpr float ThrustStart = 0.54f;
	constexpr float ThrustFull = 0.94f;
	constexpr float BoostFadeStart = 0.94f;
	constexpr float EngineFadeTime = 0.08f;
	constexpr float LateralVelocityResponse = 5.0f;
	constexpr float ArcRadius = 300.0f;
	constexpr float BankFadeEnd = 0.20f;
	constexpr float BankRestoreStart = 0.88f;
}

UQuickReversalComponent::UQuickReversalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQuickReversalComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
		CachedEvasiveRollComponent = Owner->FindComponentByClass<UEvasiveRollComponent>();
		CachedBackwardDashComponent = Owner->FindComponentByClass<UBackwardDashComponent>();
		CachedForwardDashComponent = Owner->FindComponentByClass<UForwardDashComponent>();
		CachedHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
	}
}

void UQuickReversalComponent::RequestActivation(const float PreferredDirection)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()
		|| GetLocalTime() < LocalNextAllowedTime)
	{
		return;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead())
		|| (CachedBackwardDashComponent && CachedBackwardDashComponent->IsDashActive())
		|| (CachedForwardDashComponent && CachedForwardDashComponent->IsDashActive()))
	{
		return;
	}

	const EQuickReversalDirection Direction =
		PreferredDirection < -QuickReversalTuning::NeutralDirectionThreshold
			? EQuickReversalDirection::Left : EQuickReversalDirection::Right;
	const double Now = GetLocalTime();
	LocalNextAllowedTime = Now + QuickReversalTuning::Cooldown;
	bLocalActivationPending = true;
	if (GetOwner()->HasAuthority())
	{
		if (CanActivate())
		{
			StartAuthoritativeReversal(Direction);
		}
		else
		{
			LocalNextAllowedTime = Now;
			bLocalActivationPending = false;
		}
		return;
	}
	ServerRequestActivation(Direction);
}

bool UQuickReversalComponent::TryActivateFromAbilityQueue(const float PreferredDirection)
{
	if (!CanActivate())
	{
		return false;
	}
	const EQuickReversalDirection Direction =
		PreferredDirection < -QuickReversalTuning::NeutralDirectionThreshold
			? EQuickReversalDirection::Left : EQuickReversalDirection::Right;
	StartAuthoritativeReversal(Direction);
	return true;
}

void UQuickReversalComponent::ServerRequestActivation_Implementation(
	const EQuickReversalDirection Direction)
{
	if (!CanActivate())
	{
		ClientRejectActivation();
		return;
	}
	StartAuthoritativeReversal(Direction);
}

void UQuickReversalComponent::ClientRejectActivation_Implementation()
{
	LocalNextAllowedTime = GetLocalTime();
	bLocalActivationPending = false;
}

bool UQuickReversalComponent::CanActivate() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ReversalState.bActive
		|| GetSynchronizedTime() < NextAllowedServerTime)
	{
		return false;
	}
	if (!CachedFlightComponent || !CachedFlightComponent->IsCombatFlightEnabled()
		|| (CachedHealthComponent && CachedHealthComponent->IsDead()))
	{
		return false;
	}
	return (!CachedEvasiveRollComponent
			|| !CachedEvasiveRollComponent->IsRollManeuverInProgress())
		&& (!CachedBackwardDashComponent || !CachedBackwardDashComponent->IsDashActive())
		&& (!CachedForwardDashComponent || !CachedForwardDashComponent->IsDashActive());
}

void UQuickReversalComponent::StartAuthoritativeReversal(
	const EQuickReversalDirection Direction)
{
	const double Now = GetSynchronizedTime();
	ReversalState.bActive = true;
	bLocalActivationPending = false;
	ReversalState.Direction = Direction;
	ReversalState.ServerStartTime = Now;
	++ReversalState.Sequence;
	InitialFlightRotation = GetOwner()->GetActorQuat().GetNormalized();
	ReversalState.InitialFlightRotation = InitialFlightRotation.Rotator();
	const FVector TargetForward = -InitialFlightRotation.GetForwardVector();
	TargetFlightRotation = FRotationMatrix::MakeFromXZ(
		TargetForward, InitialFlightRotation.GetUpVector()).ToQuat().GetNormalized();
	const FVector InitialVelocity = CachedFlightComponent
		? CachedFlightComponent->GetCurrentVelocity() : FVector::ZeroVector;
	const FVector InitialForward = GetOwner()->GetActorForwardVector();
	InitialForwardVelocity = InitialForward
		* FVector::DotProduct(InitialVelocity, InitialForward);
	CurrentLateralVelocity = InitialVelocity - InitialForwardVelocity;
	PreviousArcOffset = FVector::ZeroVector;
	if (InitialForwardVelocity.IsNearlyZero() && GetOwner())
	{
		const float InitialSpeed = CachedFlightComponent
			? CachedFlightComponent->GetCurrentForwardSpeed() : 0.0f;
		InitialForwardVelocity = InitialForward * InitialSpeed;
	}
	NextAllowedServerTime = Now + QuickReversalTuning::Cooldown;
	MulticastPlayQuickReversalSound();
	GetOwner()->ForceNetUpdate();
}

void UQuickReversalComponent::MulticastPlayQuickReversalSound_Implementation()
{
	if (QuickReversalSound && GetNetMode() != NM_DedicatedServer && GetOwner()
		&& GetOwner()->GetRootComponent())
	{
		UGameplayStatics::SpawnSoundAttached(
			QuickReversalSound, GetOwner()->GetRootComponent());
	}
}

bool UQuickReversalComponent::GetAuthoritativeFlightRotation(FQuat& OutRotation) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ReversalState.bActive)
	{
		return false;
	}

	OutRotation = BuildManeuverWorldRotation(GetManeuverProgress());
	return true;
}

bool UQuickReversalComponent::ConsumeAuthoritativeMovement(
	const float DeltaTime, const float NormalSpeed, const float BoostedSpeed,
	const FVector& DesiredLateralVelocity,
	FVector& OutVelocity, float& OutSignedForwardSpeed)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ReversalState.bActive)
	{
		return false;
	}

	const float Progress = GetManeuverProgress();
	const float CoastProgress = FMath::SmoothStep(
		0.0f, QuickReversalTuning::CoastEnd, Progress);
	const float ThrustProgress = FMath::SmoothStep(
		QuickReversalTuning::ThrustStart,
		QuickReversalTuning::ThrustFull,
		Progress);

	const FVector ExitDirection = TargetFlightRotation.GetForwardVector().GetSafeNormal();
	const float SkillTargetSpeed = FMath::Max(NormalSpeed, BoostedSpeed);
	CurrentLateralVelocity = FMath::VInterpTo(
		CurrentLateralVelocity, DesiredLateralVelocity, DeltaTime,
		QuickReversalTuning::LateralVelocityResponse);

	const FVector ArcOffset = BuildManeuverArcOffset(Progress);
	const FVector ArcVelocity = DeltaTime > UE_SMALL_NUMBER
		? (ArcOffset - PreviousArcOffset) / DeltaTime
		: FVector::ZeroVector;
	PreviousArcOffset = ArcOffset;
	OutVelocity = InitialForwardVelocity * (1.0f - CoastProgress)
		+ ArcVelocity
		+ ExitDirection * SkillTargetSpeed * ThrustProgress
		+ CurrentLateralVelocity;
	OutSignedForwardSpeed = FVector::DotProduct(
		OutVelocity, GetOwner()->GetActorForwardVector());

	if (Progress >= 1.0f)
	{
		if (CachedFlightComponent)
		{
			CachedFlightComponent->PrepareStraightManeuverExit();
		}
		ReversalState.bActive = false;
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

void UQuickReversalComponent::OnRep_ReversalState()
{
	bLocalActivationPending = false;
	if (ReversalState.bActive)
	{
		InitialFlightRotation =
			ReversalState.InitialFlightRotation.Quaternion().GetNormalized();
		const FVector TargetForward = -InitialFlightRotation.GetForwardVector();
		TargetFlightRotation = FRotationMatrix::MakeFromXZ(
			TargetForward, InitialFlightRotation.GetUpVector()).ToQuat().GetNormalized();
		PreviousArcOffset = BuildManeuverArcOffset(GetManeuverProgress());
		const double Elapsed = FMath::Max(
			0.0, GetSynchronizedTime() - ReversalState.ServerStartTime);
		LocalNextAllowedTime = FMath::Max(
			LocalNextAllowedTime,
			GetLocalTime() + QuickReversalTuning::Cooldown - Elapsed);
	}
}

float UQuickReversalComponent::GetCooldownRemaining() const
{
	const bool bAuthority = GetOwner() && GetOwner()->HasAuthority();
	const double EndTime = bAuthority ? NextAllowedServerTime : LocalNextAllowedTime;
	const double Now = bAuthority ? GetSynchronizedTime() : GetLocalTime();
	return static_cast<float>(FMath::Max(0.0, EndTime - Now));
}

float UQuickReversalComponent::GetManeuverProgress() const
{
	if (!ReversalState.bActive)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(
		(GetSynchronizedTime() - ReversalState.ServerStartTime) / QuickReversalTuning::Duration),
		0.0f, 1.0f);
}

float UQuickReversalComponent::GetDirectionSign() const
{
	return ReversalState.Direction == EQuickReversalDirection::Left ? -1.0f : 1.0f;
}

FQuat UQuickReversalComponent::GetPresentationRelativeRotation(
	const FQuat& FlightRootRotation) const
{
	if (!ReversalState.bActive)
	{
		return FQuat::Identity;
	}

	const FQuat DesiredWorldRotation = BuildManeuverWorldRotation(GetManeuverProgress());
	return (FlightRootRotation.Inverse() * DesiredWorldRotation).GetNormalized();
}

FQuat UQuickReversalComponent::BuildManeuverWorldRotation(const float Progress) const
{
	const float FlipAlpha = FMath::SmoothStep(
		QuickReversalTuning::RotationStart,
		QuickReversalTuning::RotationEnd,
		Progress);
	const FQuat ForwardFlip(
		FVector::RightVector,
		FMath::DegreesToRadians(180.0f * FlipAlpha));

	// A half-loop ends inverted. Correct it with one explicit local-axis half-roll.
	// Building both rotations from fixed local axes avoids interpolating between a
	// moving flip and an exactly opposite quaternion, whose path is ambiguous.
	const float UprightAlpha = FMath::SmoothStep(
		QuickReversalTuning::UprightCorrectionStart,
		QuickReversalTuning::UprightCorrectionEnd,
		Progress);
	const FQuat UprightRoll(
		FVector::ForwardVector,
		FMath::DegreesToRadians(180.0f * GetDirectionSign() * UprightAlpha));
	return (InitialFlightRotation * ForwardFlip * UprightRoll).GetNormalized();
}

FVector UQuickReversalComponent::BuildManeuverArcOffset(const float Progress) const
{
	const float FlipAlpha = FMath::SmoothStep(
		QuickReversalTuning::RotationStart,
		QuickReversalTuning::RotationEnd,
		Progress);
	const FVector InitialForward = InitialFlightRotation.GetForwardVector();
	const FVector InitialRight = InitialFlightRotation.GetRightVector();
	const FVector PivotOffset = InitialForward * QuickReversalTuning::ArcRadius;
	const FVector StartRadial = -PivotOffset;
	const FQuat ArcRotation(
		InitialRight,
		FMath::DegreesToRadians(180.0f * FlipAlpha));
	return PivotOffset + ArcRotation.RotateVector(StartRadial);
}

FVector UQuickReversalComponent::GetManeuverUpAxis() const
{
	return InitialFlightRotation.GetUpVector().GetSafeNormal();
}

float UQuickReversalComponent::GetEngineCutAlpha() const
{
	if (!ReversalState.bActive)
	{
		return 0.0f;
	}
	const float Progress = GetManeuverProgress();
	const float FadeIn = FMath::SmoothStep(
		0.0f, QuickReversalTuning::EngineFadeTime, Progress);
	const float FadeOut = 1.0f - FMath::SmoothStep(
		QuickReversalTuning::ThrustStart - QuickReversalTuning::EngineFadeTime,
		QuickReversalTuning::ThrustStart, Progress);
	return FadeIn * FadeOut;
}

float UQuickReversalComponent::GetSkillBoostAlpha() const
{
	if (!ReversalState.bActive)
	{
		return 0.0f;
	}
	const float Progress = GetManeuverProgress();
	const float FadeIn = FMath::SmoothStep(
		QuickReversalTuning::ThrustStart,
		QuickReversalTuning::ThrustFull, Progress);
	const float FadeOut = 1.0f - FMath::SmoothStep(
		QuickReversalTuning::BoostFadeStart, 1.0f, Progress);
	return FadeIn * FadeOut;
}

float UQuickReversalComponent::GetFlightBankBlendAlpha() const
{
	if (!ReversalState.bActive)
	{
		return 1.0f;
	}
	const float Progress = GetManeuverProgress();
	if (Progress < QuickReversalTuning::BankFadeEnd)
	{
		return 1.0f - FMath::SmoothStep(
			0.0f, QuickReversalTuning::BankFadeEnd, Progress);
	}
	return FMath::SmoothStep(
		QuickReversalTuning::BankRestoreStart, 1.0f, Progress);
}

double UQuickReversalComponent::GetSynchronizedTime() const
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

double UQuickReversalComponent::GetLocalTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UQuickReversalComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQuickReversalComponent, ReversalState);
}
