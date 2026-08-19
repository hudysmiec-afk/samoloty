#include "MainMenuWidget.h"

#include "Engine/World.h"
#include "FlightGameInstance.h"
#include "GraphicsSettingsWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SettingsWidgetClass = TSoftClassPtr<UGraphicsSettingsWidget>(FSoftObjectPath(
		TEXT("/Game/Main/UI/WBP_Settings.WBP_Settings_C")));
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	auto BindButton = [](UButton* Button, UObject* Object, const FName FunctionName)
	{
		if (Button)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Object, FunctionName);
			Button->OnClicked.Remove(Delegate);
			Button->OnClicked.Add(Delegate);
		}
	};
	BindButton(BTN_PlayOffline, this, GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, HandlePlayOffline));
	BindButton(BTN_HostGame, this, GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, HandleHostGame));
	BindButton(BTN_JoinGame, this, GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, HandleJoinGame));
	BindButton(BTN_Settings, this, GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, HandleSettings));
	BindButton(BTN_Quit, this, GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, HandleQuit));

	if (TXT_PlayerName)
	{
		TXT_PlayerName->OnTextCommitted.RemoveDynamic(
			this, &UMainMenuWidget::HandlePlayerNameCommitted);
		TXT_PlayerName->OnTextCommitted.AddDynamic(
			this, &UMainMenuWidget::HandlePlayerNameCommitted);
		if (const UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>())
		{
			TXT_PlayerName->SetText(FText::FromString(GameInstance->GetPlayerName()));
		}
	}
	if (TXT_Status)
	{
		TXT_Status->SetText(FText::GetEmpty());
	}
}

void UMainMenuWidget::HandlePlayOffline()
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		SetStatus(NSLOCTEXT("MainMenu", "OfflineRequiresStandalone",
			"Play Offline requires a Standalone menu window. This PIE window is already a network client."), true);
		return;
	}

	StoreEnteredPlayerName();
	UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>();
	if (!GameInstance || !GameInstance->PlayOffline())
	{
		SetStatus(NSLOCTEXT("MainMenu", "OfflineFailed", "Could not open the gameplay map."), true);
	}
}

void UMainMenuWidget::HandleHostGame()
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		SetStatus(NSLOCTEXT("MainMenu", "HostRequiresStandalone",
			"Host Game requires a Standalone menu window. This PIE window is already a network client."), true);
		return;
	}

	StoreEnteredPlayerName();
	UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>();
	if (!GameInstance || !GameInstance->HostGame())
	{
		SetStatus(NSLOCTEXT("MainMenu", "HostFailed", "Could not start the server."), true);
	}
}

void UMainMenuWidget::HandleJoinGame()
{
	StoreEnteredPlayerName();
	const FString Address = TXT_ServerAddress ? TXT_ServerAddress->GetText().ToString() : FString();
	UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>();
	if (!GameInstance || !GameInstance->JoinGame(Address))
	{
		SetStatus(NSLOCTEXT("MainMenu", "AddressRequired", "Enter a server IP address."), true);
	}
	else
	{
		SetStatus(NSLOCTEXT("MainMenu", "Connecting", "Connecting..."), false);
	}
}

void UMainMenuWidget::HandleSettings()
{
	OpenSettingsPanel();
}

void UMainMenuWidget::OpenSettingsPanel_Implementation()
{
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		return;
	}

	const TSubclassOf<UGraphicsSettingsWidget> LoadedClass = SettingsWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		SetStatus(NSLOCTEXT("MainMenu", "SettingsWidgetMissing",
			"Settings widget is missing or has the wrong parent class."), true);
		return;
	}
	SettingsWidget = CreateWidget<UGraphicsSettingsWidget>(GetOwningPlayer(), LoadedClass);
	if (SettingsWidget)
	{
		SettingsWidget->AddToViewport(10);
	}
}

void UMainMenuWidget::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(
		this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::HandlePlayerNameCommitted(
	const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>())
	{
		GameInstance->SetPlayerName(Text.ToString());
		if (TXT_PlayerName)
		{
			TXT_PlayerName->SetText(FText::FromString(GameInstance->GetPlayerName()));
		}
	}
}

void UMainMenuWidget::StoreEnteredPlayerName()
{
	if (UFlightGameInstance* GameInstance = GetGameInstance<UFlightGameInstance>())
	{
		GameInstance->SetPlayerName(
			TXT_PlayerName ? TXT_PlayerName->GetText().ToString() : GameInstance->GetPlayerName());
	}
}

void UMainMenuWidget::SetStatus(const FText& Text, const bool bError)
{
	if (TXT_Status)
	{
		TXT_Status->SetText(Text);
		TXT_Status->SetColorAndOpacity(FSlateColor(
			bError ? FLinearColor(1.0f, 0.15f, 0.08f) : FLinearColor(0.25f, 0.8f, 1.0f)));
	}
}
