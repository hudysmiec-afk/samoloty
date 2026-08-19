#include "FlightPlayerController.h"

#include "ArcadeFlightComponent.h"
#include "FlightGameInstance.h"
#include "PauseMenuWidget.h"
#include "GameFramework/PlayerState.h"

AFlightPlayerController::AFlightPlayerController()
{
	PauseMenuWidgetClass = TSoftClassPtr<UPauseMenuWidget>(FSoftObjectPath(
		TEXT("/Game/Main/UI/WBP_PauseMenu.WBP_PauseMenu_C")));
}

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

void AFlightPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	check(InputComponent);
	InputComponent->BindAction(
		TEXT("PlaneFlightMenu"), IE_Pressed, this, &AFlightPlayerController::ToggleFlightMenu);
}

bool AFlightPlayerController::CanOpenFlightMenu() const
{
	const APawn* ControlledPawn = GetPawn();
	const UArcadeFlightComponent* Flight = ControlledPawn
		? ControlledPawn->FindComponentByClass<UArcadeFlightComponent>() : nullptr;
	return IsLocalController() && Flight
		&& Flight->GetHoverState() == EArcadeHoverState::Hovering;
}

void AFlightPlayerController::ToggleFlightMenu()
{
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport())
	{
		CloseFlightMenu();
		return;
	}

	if (!CanOpenFlightMenu())
	{
		return;
	}

	const TSubclassOf<UPauseMenuWidget> LoadedClass = PauseMenuWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Pause menu widget is missing. Create /Game/Main/UI/WBP_PauseMenu based on PauseMenuWidget."));
		return;
	}

	PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this, LoadedClass);
	if (!PauseMenuWidget)
	{
		return;
	}

	PauseMenuWidget->AddToViewport(100);
	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	PauseMenuWidget->SetKeyboardFocus();
}

void AFlightPlayerController::CloseFlightMenu()
{
	if (PauseMenuWidget)
	{
		PauseMenuWidget->CloseSettingsOverlay();
		PauseMenuWidget->RemoveFromParent();
		PauseMenuWidget = nullptr;
	}

	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(InputMode);
}

void AFlightPlayerController::ServerApplyPlayerName_Implementation(
	const FString& RequestedName)
{
	if (PlayerState)
	{
		PlayerState->SetPlayerName(UFlightGameInstance::SanitizePlayerName(RequestedName));
	}
}
