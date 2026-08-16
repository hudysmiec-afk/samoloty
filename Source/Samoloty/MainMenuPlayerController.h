#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

UCLASS()
class SAMOLOTY_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

protected:
	virtual void BeginPlay() override;

	/** Defaults to /Game/Main/UI/WBP_MainMenu and can be overridden in a BP controller. */
	UPROPERTY(EditDefaultsOnly, Category="Main Menu")
	TSoftClassPtr<UMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MainMenuWidget;
};
