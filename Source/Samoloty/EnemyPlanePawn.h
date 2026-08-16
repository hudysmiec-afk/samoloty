#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "EnemyPlanePawn.generated.h"

class UBoxComponent;
class UAircraftCollisionComponent;
class UCapsuleComponent;
class UEnemyPlaneAIComponent;
class UHealthComponent;
class UJetStatsComponent;
class URifleGunComponent;
class USceneComponent;
class UStaticMeshComponent;
class UWorldNameComponent;

UCLASS()
class SAMOLOTY_API AEnemyPlanePawn : public APawn
{
	GENERATED_BODY()

public:
	AEnemyPlanePawn();
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Rounded capsule used only by the authoritative movement sweep. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Collision")
	TObjectPtr<UCapsuleComponent> MovementCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<USceneComponent> VisualRoot;

	/** Query-only hitboxes. They can be fitted to every enemy Blueprint independently. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Collision")
	TObjectPtr<UBoxComponent> BodyHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Collision")
	TObjectPtr<UBoxComponent> LeftWingHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Collision")
	TObjectPtr<UBoxComponent> RightWingHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<UAircraftCollisionComponent> AircraftCollision;

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
	TObjectPtr<UWorldNameComponent> WorldName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<USceneComponent> GunMuzzleLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Plane|Components")
	TObjectPtr<USceneComponent> GunMuzzleRight;
};
