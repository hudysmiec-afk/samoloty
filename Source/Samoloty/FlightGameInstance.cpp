#include "FlightGameInstance.h"

#include "Engine/World.h"
#include "PlayerProfileSaveGame.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

const FString UFlightGameInstance::ProfileSlotName = TEXT("LocalPlayerProfile");

void UFlightGameInstance::Init()
{
	Super::Init();
	LoadLocalProfile();
}

FString UFlightGameInstance::SanitizePlayerName(const FString& InName)
{
	FString Result = InName.TrimStartAndEnd().Left(24);
	for (const TCHAR Character : {TEXT('?'), TEXT('&'), TEXT('#'), TEXT('/'), TEXT('\\')})
	{
		Result.ReplaceCharInline(Character, TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("Player") : Result;
}

void UFlightGameInstance::SetPlayerName(const FString& NewPlayerName)
{
	const FString SanitizedName = SanitizePlayerName(NewPlayerName);
	if (PlayerName == SanitizedName)
	{
		return;
	}
	PlayerName = SanitizedName;
	SaveLocalProfile();
}

bool UFlightGameInstance::PlayOffline()
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || GameplayMap.IsNone())
	{
		return false;
	}
	UGameplayStatics::OpenLevel(this, GameplayMap, true, BuildTravelOptions(false));
	return true;
}

bool UFlightGameInstance::HostGame()
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || GameplayMap.IsNone())
	{
		return false;
	}
	UGameplayStatics::OpenLevel(this, GameplayMap, true, BuildTravelOptions(true));
	return true;
}

bool UFlightGameInstance::JoinGame(const FString& ServerAddress)
{
	FString Address = ServerAddress.TrimStartAndEnd();
	if (Address.IsEmpty())
	{
		return false;
	}
	APlayerController* LocalController = GetFirstLocalPlayerController();
	if (!LocalController)
	{
		return false;
	}
	Address += Address.Contains(TEXT("?")) ? TEXT("&Name=") : TEXT("?Name=");
	Address += PlayerName;
	LocalController->ClientTravel(Address, TRAVEL_Absolute);
	return true;
}

void UFlightGameInstance::LoadLocalProfile()
{
	if (const UPlayerProfileSaveGame* Save = Cast<UPlayerProfileSaveGame>(
		UGameplayStatics::LoadGameFromSlot(ProfileSlotName, 0)))
	{
		PlayerName = SanitizePlayerName(Save->PlayerName);
	}
}

void UFlightGameInstance::SaveLocalProfile() const
{
	UPlayerProfileSaveGame* Save = Cast<UPlayerProfileSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UPlayerProfileSaveGame::StaticClass()));
	if (Save)
	{
		Save->PlayerName = PlayerName;
		UGameplayStatics::SaveGameToSlot(Save, ProfileSlotName, 0);
	}
}

FString UFlightGameInstance::BuildTravelOptions(const bool bListenServer) const
{
	return FString::Printf(TEXT("%sName=%s"),
		bListenServer ? TEXT("listen?") : TEXT(""), *PlayerName);
}
