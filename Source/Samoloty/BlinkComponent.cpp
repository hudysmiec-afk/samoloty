#include "BlinkComponent.h"

#include "ArcadeFlightComponent.h"
#include "ArcadeJetPawn.h"
#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"

namespace BlinkTuning
{
	constexpr float Distance = 10000.0f; // 100 m in Unreal units.
	constexpr double Cooldown = 8.0;
	constexpr double CameraRecoveryDuration = 0.35;
}

UBlinkComponent::UBlinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UBlinkComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedFlightComponent = Owner->FindComponentByClass<UArcadeFlightComponent>();
		CachedHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
	}
}

bool UBlinkComponent::TryActivateFromAbilityQueue(const FVector2D& DirectionInput)
{
	if (!CanActivate())
	{
		return false;
	}
	StartAuthoritativeBlink(DirectionInput);
	return true;
}

bool UBlinkComponent::CanActivate() const
{
	return GetOwner() && GetOwner()->HasAuthority()
		&& CachedFlightComponent && CachedFlightComponent->IsCombatFlightEnabled()
		&& (!CachedHealthComponent || !CachedHealthComponent->IsDead())
		&& GetSynchronizedTime() >= NextAllowedServerTime;
}

void UBlinkComponent::StartAuthoritativeBlink(const FVector2D& DirectionInput)
{
	AArcadeJetPawn* Plane = Cast<AArcadeJetPawn>(GetOwner());
	if (!Plane)
	{
		return;
	}

	FVector2D SafeInput(
		FMath::Clamp(DirectionInput.X, -1.0f, 1.0f),
		FMath::Clamp(DirectionInput.Y, -1.0f, 1.0f));
	if (SafeInput.IsNearlyZero())
	{
		SafeInput.X = 1.0f;
	}
	SafeInput.Normalize();

	const FVector DepartureLocation = Plane->GetActorLocation();
	const FVector Direction = (
		Plane->GetActorForwardVector() * SafeInput.X
		+ Plane->GetActorRightVector() * SafeInput.Y).GetSafeNormal();
	Plane->MovePlaneWithCollision(
		Direction * BlinkTuning::Distance,
		Direction * CachedFlightComponent->GetCurrentForwardSpeed(),
		false);
	const FVector ArrivalLocation = Plane->GetActorLocation();

	const double Now = GetSynchronizedTime();
	BlinkState.ServerActivationTime = Now;
	++BlinkState.Sequence;
	NextAllowedServerTime = Now + BlinkTuning::Cooldown;
	CachedFlightComponent->NotifyAuthoritativeTeleport();
	MulticastPlayBlinkEffects(DepartureLocation, ArrivalLocation);
	Plane->ForceNetUpdate();
}

void UBlinkComponent::MulticastPlayBlinkEffects_Implementation(
	const FVector_NetQuantize DepartureLocation, const FVector_NetQuantize ArrivalLocation)

{
	PlayPredictedActivationEffect(DepartureLocation, ArrivalLocation);
}

void UBlinkComponent::PlayPredictedActivationEffect(
	const FVector& DepartureLocation, const FVector& ArrivalLocation)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (BlinkDepartureEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, BlinkDepartureEffect, DepartureLocation);
	}
	if (BlinkArrivalEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, BlinkArrivalEffect, ArrivalLocation);
	}
	if (BlinkSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BlinkSound, ArrivalLocation);
	}
}

float UBlinkComponent::GetCooldownRemaining() const
{
	return CachedFlightComponent
		? CachedFlightComponent->GetBlinkCooldownRemaining() : 0.0f;
}

bool UBlinkComponent::IsCameraRecovering() const
{
	return CachedFlightComponent && CachedFlightComponent->IsBlinkCameraRecovering();
}

double UBlinkComponent::GetSynchronizedTime() const
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

void UBlinkComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBlinkComponent, BlinkState);
}
