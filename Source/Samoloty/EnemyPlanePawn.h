#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "EnemyPlanePawn.generated.h"

class UBoxComponent;
class UEnemyPlaneAIComponent;
class UHealthComponent;
class UJetStatsComponent;
class URifleGunComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class SAMOLOTY_API AEnemyPlanePawn : public APawn
{
	GENERATED_BODY()

public:
	AEnemyPlanePawn();
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<UStaticMeshComponent> PlaneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<UJetStatsComponent> JetStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<URifleGunComponent> RifleGun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<UEnemyPlaneAIComponent> EnemyAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<USceneComponent> GunMuzzleLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<USceneComponent> GunMuzzleRight;
};
