#include "ArcadeFlightGameMode.h"
#include "ArcadeFlightHUD.h"
#include "ArcadeJetPawn.h"
#include "FlightPlayerController.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

AArcadeFlightGameMode::AArcadeFlightGameMode()
{
	DefaultPawnClass = AArcadeJetPawn::StaticClass();
	HUDClass = AArcadeFlightHUD::StaticClass();
	PlayerControllerClass = AFlightPlayerController::StaticClass();
}

void AArcadeFlightGameMode::SchedulePlayerRespawn(AController* Controller)
{
	if (!HasAuthority() || !IsValid(Controller))
	{
		return;
	}

	const TWeakObjectPtr<AController> ControllerKey(Controller);
	if (const FTimerHandle* ExistingTimer = RespawnTimers.Find(ControllerKey);
		ExistingTimer && GetWorldTimerManager().IsTimerActive(*ExistingTimer))
	{
		return;
	}

	FTimerHandle& RespawnTimer = RespawnTimers.FindOrAdd(ControllerKey);
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AArcadeFlightGameMode::RespawnPlayer, ControllerKey);
	GetWorldTimerManager().SetTimer(
		RespawnTimer, RespawnDelegate, FMath::Max(0.01f, PlayerRespawnDelay), false);
}

void AArcadeFlightGameMode::RespawnPlayer(const TWeakObjectPtr<AController> Controller)
{
	RespawnTimers.Remove(Controller);
	if (!Controller.IsValid() || Controller->GetPawn())
	{
		return;
	}

	RestartPlayer(Controller.Get());
}

void AArcadeFlightGameMode::Logout(AController* Exiting)
{
	const TWeakObjectPtr<AController> ControllerKey(Exiting);
	if (FTimerHandle* RespawnTimer = RespawnTimers.Find(ControllerKey))
	{
		GetWorldTimerManager().ClearTimer(*RespawnTimer);
		RespawnTimers.Remove(ControllerKey);
	}

	Super::Logout(Exiting);
}
