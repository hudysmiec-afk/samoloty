#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FlightPlayerController.generated.h"

/** Gameplay controller that safely transfers the locally stored name to PlayerState. */
UCLASS()
class SAMOLOTY_API AFlightPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerApplyPlayerName(const FString& RequestedName);
};
