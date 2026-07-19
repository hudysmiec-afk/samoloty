#include "ArcadeFlightGameMode.h"
#include "ArcadeJetPawn.h"

AArcadeFlightGameMode::AArcadeFlightGameMode()
{
	DefaultPawnClass = AArcadeJetPawn::StaticClass();
}
