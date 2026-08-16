#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/** C++ logic base for the visual WBP_MainMenu widget. */
UCLASS(Blueprintable)
class SAMOLOTY_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> TXT_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> TXT_ServerAddress;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> BTN_PlayOffline;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> BTN_HostGame;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> BTN_JoinGame;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> BTN_Settings;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> BTN_Quit;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Status;

	/** Implement this in WBP_MainMenu when the settings panel is added. */
	UFUNCTION(BlueprintImplementableEvent, Category="Menu")
	void OpenSettingsPanel();

private:
	UFUNCTION()
	void HandlePlayOffline();

	UFUNCTION()
	void HandleHostGame();

	UFUNCTION()
	void HandleJoinGame();

	UFUNCTION()
	void HandleSettings();

	UFUNCTION()
	void HandleQuit();

	UFUNCTION()
	void HandlePlayerNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void StoreEnteredPlayerName();
	void SetStatus(const FText& Text, bool bError);
};
