#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArcadeFlightHUD.generated.h"

class UFlightHUDWidget;
class UHealthComponent;

UCLASS()
class SAMOLOTY_API AArcadeFlightHUD : public AHUD
{
	GENERATED_BODY()

public:
	AArcadeFlightHUD();
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	UFUNCTION()
	void HandleDamageReceived(AActor* AttackerActor);

	void UpdateHealthBinding(APawn* OwnerPawn);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Flight HUD")
	TSubclassOf<UFlightHUDWidget> FlightHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UFlightHUDWidget> FlightHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|Targeting",
		meta=(ClampMin="0.1", Units="s"))
	float AttackerIndicatorDuration = 3.0f;

	TWeakObjectPtr<UHealthComponent> BoundHealthComponent;
	TMap<TWeakObjectPtr<AActor>, double> RecentAttackersUntil;
};
