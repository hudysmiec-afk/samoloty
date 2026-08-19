#include "PauseMenuWidget.h"

#include "FlightPlayerController.h"
#include "GraphicsSettingsWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SettingsWidgetClass = TSoftClassPtr<UGraphicsSettingsWidget>(FSoftObjectPath(
		TEXT("/Game/Main/UI/WBP_Settings.WBP_Settings_C")));
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BTN_Resume->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::HandleResume);
	BTN_Settings->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::HandleSettings);
	BTN_MainMenu->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::HandleMainMenu);
	BTN_Quit->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::HandleQuit);

	BTN_Resume->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResume);
	BTN_Settings->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleSettings);
	BTN_MainMenu->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleMainMenu);
	BTN_Quit->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleQuit);
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleResume();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::CloseSettingsOverlay()
{
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}
}

void UPauseMenuWidget::HandleResume()
{
	if (AFlightPlayerController* Controller = GetOwningPlayer<AFlightPlayerController>())
	{
		Controller->CloseFlightMenu();
	}
}

void UPauseMenuWidget::HandleSettings()
{
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		return;
	}

	const TSubclassOf<UGraphicsSettingsWidget> LoadedClass = SettingsWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Settings widget is missing. Assign WBP_Settings in WBP_PauseMenu Class Defaults."));
		return;
	}

	SettingsWidget = CreateWidget<UGraphicsSettingsWidget>(GetOwningPlayer(), LoadedClass);
	if (SettingsWidget)
	{
		SettingsWidget->AddToViewport(200);
		SettingsWidget->SetKeyboardFocus();
	}
}

void UPauseMenuWidget::HandleMainMenu()
{
	CloseSettingsOverlay();
	UGameplayStatics::OpenLevel(this, MainMenuMapName, true);
}

void UPauseMenuWidget::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(
		this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
