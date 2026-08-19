#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArcadeFlightHUD.generated.h"

class UFlightHUDWidget;
class UHealthComponent;
class UTexture2D;
class APawn;

UCLASS()
class SAMOLOTY_API AArcadeFlightHUD : public AHUD
{
	GENERATED_BODY()

public:
	AArcadeFlightHUD();
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	struct FActiveDamageNumber
	{
		TWeakObjectPtr<AActor> Target;
		FVector LastWorldLocation = FVector::ZeroVector;
		float Damage = 0.0f;
		double StartTime = 0.0;
		float HorizontalOffset = 0.0f;
	};

	UFUNCTION()
	void HandleDamageReceived(AActor* AttackerActor);

	UFUNCTION()
	void HandleDamageDealt(AActor* DamagedActor, float Damage, FVector WorldLocation);

	void UpdateHealthBinding(APawn* OwnerPawn);
	void DrawGroundBarrageMarker(APawn* OwnerPawn, bool bCombatElementsVisible);
	void RefreshNameplateActors(APawn* OwnerPawn);
	void DrawWorldNameplates(APawn* OwnerPawn);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Flight HUD")
	TSubclassOf<UFlightHUDWidget> FlightHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UFlightHUDWidget> FlightHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|Targeting",
		meta=(ClampMin="0.1", Units="s"))
	float AttackerIndicatorDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|Damage Numbers",
		meta=(ClampMin="0.1", Units="s"))
	float DamageNumberDuration = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|Damage Numbers",
		meta=(ClampMin="0", Units="s"))
	float DamageNumberMergeWindow = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names",
		meta=(ClampMin="100", Units="cm"))
	float NameplateMaxDistance = 50000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names",
		meta=(ClampMin="0", Units="cm"))
	float NameplateHeightOffset = 55.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names",
		meta=(ClampMin="0.05", Units="s"))
	float NameplateActorRefreshInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names")
	FLinearColor PlayerNameColor = FLinearColor(0.35f, 0.85f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names")
	TObjectPtr<UTexture2D> AttackerOnScreenTexture;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names",
		meta=(ClampMin="1"))
	float AttackerOnScreenSize = 32.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names",
		meta=(ClampMin="0"))
	float AttackerOnScreenGap = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flight HUD|World Names")
	FLinearColor AttackerOnScreenColor = FLinearColor(1.0f, 0.04f, 0.02f, 1.0f);

	TWeakObjectPtr<UHealthComponent> BoundHealthComponent;
	TMap<TWeakObjectPtr<AActor>, double> RecentAttackersUntil;
	TArray<FActiveDamageNumber> ActiveDamageNumbers;
	TArray<TWeakObjectPtr<APawn>> CachedNameplateActors;
	double NextNameplateActorRefreshTime = 0.0;
	double NextPerformanceStatsUpdateTime = 0.0;
	double NextAbilityCooldownUpdateTime = 0.0;
	float SmoothedFPS = 0.0f;
};
