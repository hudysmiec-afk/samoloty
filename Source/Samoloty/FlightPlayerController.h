#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FlightPlayerController.generated.h"

class UPauseMenuWidget;

/** Gameplay controller that safely transfers the locally stored name to PlayerState. */
UCLASS()
class SAMOLOTY_API AFlightPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFlightPlayerController();

	/** Closes the local menu overlay and restores flight input. */
	void CloseFlightMenu();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Defaults to /Game/Main/UI/WBP_PauseMenu and can be overridden in a BP controller. */
	UPROPERTY(EditDefaultsOnly, Category="Menu")
	TSoftClassPtr<UPauseMenuWidget> PauseMenuWidgetClass;

private:
	void ToggleFlightMenu();
	bool CanOpenFlightMenu() const;

	UFUNCTION(Server, Reliable)
	void ServerApplyPlayerName(const FString& RequestedName);

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenuWidget;
};
