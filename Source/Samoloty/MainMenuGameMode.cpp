#include "MainMenuGameMode.h"

#include "GameFramework/SpectatorPawn.h"
#include "MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	// A null DefaultPawnClass makes networked PIE try to restart a player with
	// no pawn class, which ends in "Couldn't spawn player". The spectator is
	// invisible and gives the menu connection a valid, harmless pawn.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	bStartPlayersAsSpectators = true;
	HUDClass = nullptr;
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}
