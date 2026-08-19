#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UGraphicsSettingsWidget;

/** C++ logic base for the visual WBP_PauseMenu widget. Does not pause the game world. */
UCLASS(Blueprintable)
class SAMOLOTY_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Removes any settings overlay opened by this menu. */
	void CloseSettingsOverlay();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_Resume;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_Settings;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_MainMenu;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> BTN_Quit;

	/** Defaults to the existing /Game/Main/UI/WBP_Settings widget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Menu")
	TSoftClassPtr<UGraphicsSettingsWidget> SettingsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Menu")
	FName MainMenuMapName = TEXT("/Game/Main/Maps/MainMenu");

private:
	UFUNCTION()
	void HandleResume();

	UFUNCTION()
	void HandleSettings();

	UFUNCTION()
	void HandleMainMenu();

	UFUNCTION()
	void HandleQuit();

	UPROPERTY(Transient)
	TObjectPtr<UGraphicsSettingsWidget> SettingsWidget;
};
