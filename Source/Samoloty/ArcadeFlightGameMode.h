#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArcadeFlightGameMode.generated.h"

class AController;

UCLASS()
class SAMOLOTY_API AArcadeFlightGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArcadeFlightGameMode();

	/** Called on the server when a player-controlled pawn is destroyed by death. */
	void SchedulePlayerRespawn(AController* Controller);

protected:
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Respawn", meta=(ClampMin="0.0", Units="s"))
	float PlayerRespawnDelay = 15.0f;

private:
	void RespawnPlayer(TWeakObjectPtr<AController> Controller);

	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
};
