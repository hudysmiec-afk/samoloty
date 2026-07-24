#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JetEngineAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;

/** Local presentation component that crossfades slow, cruise and boost engine loops. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UJetEngineAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJetEngineAudioComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Engine")
	TObjectPtr<USoundBase> SlowEngineLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Engine")
	TObjectPtr<USoundBase> NormalEngineLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Engine")
	TObjectPtr<USoundBase> BoostEngineLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Engine", meta=(ClampMin="0"))
	float MasterVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Audio|Engine", meta=(ClampMin="0.1"))
	float CrossfadeSpeed = 6.0f;

private:
	UAudioComponent* CreateLoopComponent(USoundBase* Sound, FName ComponentName);
	void StopAndDestroyAudioComponent(UAudioComponent*& Component);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SlowAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> NormalAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BoostAudioComponent;

	float SlowVolume = 0.0f;
	float NormalVolume = 0.0f;
	float BoostVolume = 0.0f;
};
