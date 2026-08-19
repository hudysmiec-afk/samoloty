#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class APawn;

/**
 * Lightweight server-authoritative population spawner.
 * Each instance manages only the enemies it created and never needs Tick.
 */
UCLASS(Blueprintable)
class SAMOLOTY_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Enemy Spawner")
	bool SpawnEnemy();

	UFUNCTION(BlueprintPure, Category="Enemy Spawner")
	int32 GetAliveEnemyCount() const;

protected:
	/** Enemy Blueprint class spawned by this point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner")
	TSubclassOf<APawn> EnemyClass;

	/** Population this spawner will keep alive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta=(ClampMin="1", UIMin="1"))
	int32 MaxAliveEnemies = 5;

	/** Enemies created immediately when the server starts the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta=(ClampMin="0", UIMin="0"))
	int32 InitialSpawnCount = 5;

	/** Delay before replacing a destroyed enemy. Missing enemies are restored one by one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta=(ClampMin="0.1", UIMin="0.1", Units="s"))
	float RespawnDelay = 5.0f;

	/** Horizontal spawn radius around this actor, in Unreal units (100 = 1 metre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float SpawnRadius = 3000.0f;

	/** Added to the final spawn height. Useful for flying enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta=(Units="cm"))
	float SpawnHeightOffset = 0.0f;

	/** Minimum horizontal distance between enemies managed by this spawner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner",
		meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float MinimumSpawnSeparation = 800.0f;

	/** Random positions tested before postponing this spawn until the next retry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner",
		meta=(ClampMin="1", ClampMax="100", UIMin="1", UIMax="100"))
	int32 MaxSpawnAttempts = 20;

	/** Small clearance above projected ground, added on top of capsule half-height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner|Navigation",
		meta=(EditCondition="bProjectToNavigation", ClampMin="0", Units="cm"))
	float GroundSpawnClearance = 5.0f;

	/** Enable for walking enemies so their spawn position is placed on NavMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner|Navigation")
	bool bProjectToNavigation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner|Navigation", meta=(EditCondition="bProjectToNavigation", Units="cm"))
	FVector NavigationProjectionExtent = FVector(500.0f, 500.0f, 2000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner")
	bool bRandomizeYaw = true;

private:
	UFUNCTION()
	void HandleSpawnedEnemyDestroyed(AActor* DestroyedActor);

	void MaintainPopulation();
	void RemoveInvalidEnemies();
	bool FindSpawnTransform(FTransform& OutTransform) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APawn>> SpawnedEnemies;

	FTimerHandle PopulationTimer;
	double NextSpawnTime = -1.0;
};
