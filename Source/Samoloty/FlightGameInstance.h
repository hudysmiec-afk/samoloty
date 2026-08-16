#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FlightGameInstance.generated.h"

/** Persistent local menu/profile state shared between the menu and gameplay maps. */
UCLASS()
class SAMOLOTY_API UFlightGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintPure, Category="Menu|Profile")
	const FString& GetPlayerName() const { return PlayerName; }

	UFUNCTION(BlueprintCallable, Category="Menu|Profile")
	void SetPlayerName(const FString& NewPlayerName);

	UFUNCTION(BlueprintCallable, Category="Menu|Travel")
	bool PlayOffline();

	UFUNCTION(BlueprintCallable, Category="Menu|Travel")
	bool HostGame();

	UFUNCTION(BlueprintCallable, Category="Menu|Travel")
	bool JoinGame(const FString& ServerAddress);

	static FString SanitizePlayerName(const FString& InName);

protected:
	/** Package path of the gameplay map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Menu|Travel")
	FName GameplayMap = TEXT("/Game/Main/Maps/DefaultMap");

private:
	void LoadLocalProfile();
	void SaveLocalProfile() const;
	FString BuildTravelOptions(bool bListenServer) const;

	UPROPERTY(Transient)
	FString PlayerName = TEXT("Player");

	static const FString ProfileSlotName;
};
