#include "MainMenuPlayerController.h"

#include "MainMenuWidget.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
	MainMenuWidgetClass = TSoftClassPtr<UMainMenuWidget>(FSoftObjectPath(
		TEXT("/Game/Main/UI/WBP_MainMenu.WBP_MainMenu_C")));
}

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	const TSubclassOf<UMainMenuWidget> LoadedClass = MainMenuWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Main menu widget not found. Create /Game/Main/UI/WBP_MainMenu based on MainMenuWidget."));
		return;
	}
	MainMenuWidget = CreateWidget<UMainMenuWidget>(this, LoadedClass);
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport();
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}
