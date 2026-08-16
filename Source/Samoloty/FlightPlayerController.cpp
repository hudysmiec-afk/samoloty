#include "FlightPlayerController.h"

#include "FlightGameInstance.h"
#include "GameFramework/PlayerState.h"

void AFlightPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		if (const UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>())
		{
			ServerApplyPlayerName(GameInstance->GetPlayerName());
		}
	}
}

void AFlightPlayerController::ServerApplyPlayerName_Implementation(
	const FString& RequestedName)
{
	if (PlayerState)
	{
		PlayerState->SetPlayerName(UFlightGameInstance::SanitizePlayerName(RequestedName));
	}
}
