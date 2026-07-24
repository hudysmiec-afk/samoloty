#include "JetEngineAudioComponent.h"

#include "ArcadeFlightComponent.h"
#include "JetBoostComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"

UJetEngineAudioComponent::UJetEngineAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UJetEngineAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	SlowAudioComponent = CreateLoopComponent(SlowEngineLoop, TEXT("SlowEngineAudio"));
	NormalAudioComponent = CreateLoopComponent(NormalEngineLoop, TEXT("NormalEngineAudio"));
	BoostAudioComponent = CreateLoopComponent(BoostEngineLoop, TEXT("BoostEngineAudio"));
	NormalVolume = NormalAudioComponent ? MasterVolume : 0.0f;
	if (NormalAudioComponent)
	{
		NormalAudioComponent->SetVolumeMultiplier(NormalVolume);
	}
}

void UJetEngineAudioComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UArcadeFlightComponent* Flight = GetOwner()->FindComponentByClass<UArcadeFlightComponent>();
	const UJetBoostComponent* Boost = GetOwner()->FindComponentByClass<UJetBoostComponent>();
	if (!Flight || !Boost)
	{
		return;
	}

	const float BoostWeight = FMath::Clamp(Boost->GetBoostAlpha(), 0.0f, 1.0f);
	const float BrakeWeight = FMath::Clamp(Flight->GetSmoothedBrake(), 0.0f, 1.0f);
	const float SlowWeight = BrakeWeight * (1.0f - BoostWeight);
	const float NormalWeight = (1.0f - BrakeWeight) * (1.0f - BoostWeight);

	SlowVolume = FMath::FInterpTo(SlowVolume, SlowWeight * MasterVolume, DeltaTime, CrossfadeSpeed);
	NormalVolume = FMath::FInterpTo(NormalVolume, NormalWeight * MasterVolume, DeltaTime, CrossfadeSpeed);
	BoostVolume = FMath::FInterpTo(BoostVolume, BoostWeight * MasterVolume, DeltaTime, CrossfadeSpeed);
	if (SlowAudioComponent)
	{
		SlowAudioComponent->SetVolumeMultiplier(SlowVolume);
	}
	if (NormalAudioComponent)
	{
		NormalAudioComponent->SetVolumeMultiplier(NormalVolume);
	}
	if (BoostAudioComponent)
	{
		BoostAudioComponent->SetVolumeMultiplier(BoostVolume);
	}
}

UAudioComponent* UJetEngineAudioComponent::CreateLoopComponent(USoundBase* Sound,
	const FName ComponentName)
{
	if (!Sound || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return nullptr;
	}
	UAudioComponent* AudioComponent = NewObject<UAudioComponent>(GetOwner(), ComponentName);
	AudioComponent->bAutoActivate = false;
	AudioComponent->bAutoDestroy = false;
	AudioComponent->SetSound(Sound);
	AudioComponent->SetupAttachment(GetOwner()->GetRootComponent());
	AudioComponent->RegisterComponent();
	AudioComponent->SetVolumeMultiplier(0.0f);
	AudioComponent->Play();
	return AudioComponent;
}

void UJetEngineAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UAudioComponent* SlowComponent = SlowAudioComponent.Get();
	UAudioComponent* NormalComponent = NormalAudioComponent.Get();
	UAudioComponent* BoostComponent = BoostAudioComponent.Get();
	StopAndDestroyAudioComponent(SlowComponent);
	StopAndDestroyAudioComponent(NormalComponent);
	StopAndDestroyAudioComponent(BoostComponent);
	SlowAudioComponent = nullptr;
	NormalAudioComponent = nullptr;
	BoostAudioComponent = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UJetEngineAudioComponent::StopAndDestroyAudioComponent(UAudioComponent*& Component)
{
	if (Component)
	{
		Component->Stop();
		Component->DestroyComponent();
		Component = nullptr;
	}
}
