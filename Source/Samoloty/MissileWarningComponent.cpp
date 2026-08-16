#include "MissileWarningComponent.h"

#include "RocketProjectile.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"

UMissileWarningComponent::UMissileWarningComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UMissileWarningComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalPlayerOwner())
	{
		SetComponentTickEnabled(false);
	}
}

void UMissileWarningComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	for (auto It = IncomingMissiles.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	UpdateWarningAudio();
}

void UMissileWarningComponent::AddIncomingMissile(ARocketProjectile* Missile)
{
	if (!IsLocalPlayerOwner() || !IsValid(Missile))
	{
		return;
	}
	IncomingMissiles.Add(Missile);
	UpdateWarningAudio();
}

void UMissileWarningComponent::RemoveIncomingMissile(ARocketProjectile* Missile)
{
	IncomingMissiles.Remove(Missile);
	UpdateWarningAudio();
}

bool UMissileWarningComponent::IsLocalPlayerOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

void UMissileWarningComponent::UpdateWarningAudio()
{
	const bool bShouldPlay = IncomingMissiles.Num() > 0 && IncomingMissileWarningSound;
	if (bShouldPlay && !WarningAudioComponent)
	{
		WarningAudioComponent = NewObject<UAudioComponent>(GetOwner(), TEXT("MissileWarningAudio"));
		WarningAudioComponent->bAutoActivate = false;
		WarningAudioComponent->bAutoDestroy = false;
		WarningAudioComponent->bIsUISound = true;
		WarningAudioComponent->SetSound(IncomingMissileWarningSound);
		WarningAudioComponent->SetVolumeMultiplier(WarningVolume);
		WarningAudioComponent->SetupAttachment(GetOwner()->GetRootComponent());
		WarningAudioComponent->RegisterComponent();
	}
	if (bShouldPlay)
	{
		if (!WarningAudioComponent->IsPlaying())
		{
			WarningAudioComponent->Play();
		}
	}
	else if (WarningAudioComponent && WarningAudioComponent->IsPlaying())
	{
		WarningAudioComponent->Stop();
	}
}

void UMissileWarningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	IncomingMissiles.Reset();
	if (WarningAudioComponent)
	{
		WarningAudioComponent->Stop();
		WarningAudioComponent->DestroyComponent();
		WarningAudioComponent = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
