#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerProfileSaveGame.generated.h"

/** Local profile data. It contains no password or authentication secret. */
UCLASS()
class SAMOLOTY_API UPlayerProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FString PlayerName = TEXT("Player");
};
