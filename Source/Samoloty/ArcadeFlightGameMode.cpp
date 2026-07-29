#include "ArcadeFlightGameMode.h"
#include "ArcadeFlightHUD.h"
#include "ArcadeJetPawn.h"

AArcadeFlightGameMode::AArcadeFlightGameMode()
{
	DefaultPawnClass = AArcadeJetPawn::StaticClass();
	HUDClass = AArcadeFlightHUD::StaticClass();
}
