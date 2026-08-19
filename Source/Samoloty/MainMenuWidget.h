#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UGraphicsSettingsWidget;
class UTextBlock;

/** C++ logic base for the visual WBP_MainMenu widget. */
UCLASS(Blueprintable)
class SAMOLOTY_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

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

	/** Opens the reusable settings overlay. Blueprints may override presentation. */
	UFUNCTION(BlueprintNativeEvent, Category="Menu")
	void OpenSettingsPanel();
	virtual void OpenSettingsPanel_Implementation();

	UPROPERTY(EditDefaultsOnly, Category="Menu")
	TSoftClassPtr<UGraphicsSettingsWidget> SettingsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UGraphicsSettingsWidget> SettingsWidget;

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
