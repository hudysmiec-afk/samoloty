#include "QuickReversalComponent.h"

#include "ArcadeFlightComponent.h"
#include "EvasiveRollComponent.h"
#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace QuickReversalTuning
{
	constexpr double Duration = 1.10;
	constexpr double Cooldown = 4.0;
	constexpr float NeutralDirectionThreshold = 0.05f;
	constexpr float FlipPhaseEnd = 0.40f;
	constexpr float DirectionTurnEnd = 0.76f;
	constexpr float ThrustStart = 0.48f;
	constexpr float ThrustFull = 0.82f;
	constexpr float BoostFadeStart = 0.92f;
	constexpr float EngineFadeTime = 0.08f;
	constexpr float CoastSpeedAtThrust = 0.18f;
	constexpr float LateralVelocityResponse = 5.0f;
	constexpr float FlipCorrectionStart = 0.55f;
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
		|| (CachedEvasiveRollComponent && CachedEvasiveRollComponent->IsRolling()))
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
	return !CachedEvasiveRollComponent || !CachedEvasiveRollComponent->IsRolling();
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
		TargetForward, FVector::UpVector).ToQuat().GetNormalized();
	const FVector InitialVelocity = CachedFlightComponent
		? CachedFlightComponent->GetCurrentVelocity() : FVector::ZeroVector;
	const FVector InitialForward = GetOwner()->GetActorForwardVector();
	InitialForwardVelocity = InitialForward
		* FVector::DotProduct(InitialVelocity, InitialForward);
	CurrentLateralVelocity = InitialVelocity - InitialForwardVelocity;
	if (InitialForwardVelocity.IsNearlyZero() && GetOwner())
	{
		const float InitialSpeed = CachedFlightComponent
			? CachedFlightComponent->GetCurrentForwardSpeed() : 0.0f;
		InitialForwardVelocity = InitialForward * InitialSpeed;
	}
	NextAllowedServerTime = Now + QuickReversalTuning::Cooldown;
	GetOwner()->ForceNetUpdate();
}

bool UQuickReversalComponent::GetAuthoritativeFlightRotation(FQuat& OutRotation) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ReversalState.bActive)
	{
		return false;
	}

	const float Progress = GetManeuverProgress();
	const float TurnProgress = FMath::Clamp(
		(Progress - QuickReversalTuning::FlipPhaseEnd)
		/ (QuickReversalTuning::DirectionTurnEnd - QuickReversalTuning::FlipPhaseEnd),
		0.0f, 1.0f);
	const float SmoothedTurnProgress =
		TurnProgress * TurnProgress * (3.0f - 2.0f * TurnProgress);
	OutRotation = FQuat::Slerp(
		InitialFlightRotation, TargetFlightRotation, SmoothedTurnProgress).GetNormalized();
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
	const float CoastProgress = FMath::Clamp(
		Progress / QuickReversalTuning::ThrustStart, 0.0f, 1.0f);
	const float SmoothedCoastProgress = CoastProgress * CoastProgress
		* (3.0f - 2.0f * CoastProgress);
	const float ThrustProgress = FMath::Clamp(
		(Progress - QuickReversalTuning::ThrustStart)
		/ (QuickReversalTuning::ThrustFull - QuickReversalTuning::ThrustStart),
		0.0f, 1.0f);
	const float SmoothedThrustProgress = ThrustProgress * ThrustProgress
		* (3.0f - 2.0f * ThrustProgress);

	const FVector ExitDirection = TargetFlightRotation.GetForwardVector().GetSafeNormal();

	float CoastWeight = FMath::Lerp(
		1.0f, QuickReversalTuning::CoastSpeedAtThrust, SmoothedCoastProgress);
	if (Progress >= QuickReversalTuning::ThrustStart)
	{
		CoastWeight = FMath::Lerp(
			QuickReversalTuning::CoastSpeedAtThrust, 0.0f, SmoothedThrustProgress);
	}
	const float SkillTargetSpeed = FMath::Max(NormalSpeed, BoostedSpeed);
	CurrentLateralVelocity = FMath::VInterpTo(
		CurrentLateralVelocity, DesiredLateralVelocity, DeltaTime,
		QuickReversalTuning::LateralVelocityResponse);
	OutVelocity = InitialForwardVelocity * CoastWeight
		+ ExitDirection * SkillTargetSpeed * SmoothedThrustProgress
		+ CurrentLateralVelocity;
	OutSignedForwardSpeed = FVector::DotProduct(
		OutVelocity, GetOwner()->GetActorForwardVector());

	if (Progress >= 1.0f)
	{
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
			TargetForward, FVector::UpVector).ToQuat().GetNormalized();
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

	const float Progress = GetManeuverProgress();
	FQuat DesiredWorldRotation = TargetFlightRotation;
	if (Progress < QuickReversalTuning::FlipPhaseEnd)
	{
		const float FlipProgress = FMath::Clamp(
			Progress / QuickReversalTuning::FlipPhaseEnd, 0.0f, 1.0f);
		const float SmoothedFlipProgress = FlipProgress * FlipProgress
			* (3.0f - 2.0f * FlipProgress);
		const FQuat NoseDownFlip(
			FVector::RightVector,
			FMath::DegreesToRadians(-180.0f * SmoothedFlipProgress));
		const FQuat FlipOnlyRotation =
			(InitialFlightRotation * NoseDownFlip).GetNormalized();
		const float CorrectionAlpha = FMath::SmoothStep(
			QuickReversalTuning::FlipCorrectionStart, 1.0f, FlipProgress);
		DesiredWorldRotation = FQuat::Slerp(
			FlipOnlyRotation, TargetFlightRotation, CorrectionAlpha).GetNormalized();
	}
	return (FlightRootRotation.Inverse() * DesiredWorldRotation).GetNormalized();
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
	if (Progress < QuickReversalTuning::FlipPhaseEnd)
	{
		// Preserve the exact bank from the frame before activation, then remove it
		// gradually while the nose-down flip takes over.
		return 1.0f - FMath::SmoothStep(0.0f, 0.16f, Progress);
	}
	return FMath::SmoothStep(
		QuickReversalTuning::FlipPhaseEnd,
		QuickReversalTuning::DirectionTurnEnd, Progress);
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
