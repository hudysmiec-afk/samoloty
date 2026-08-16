#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldNameComponent.generated.h"

/** Reusable replicated nameplate data for NPCs and other world actors. */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UWorldNameComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldNameComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="World Name")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category="World Name")
	FLinearColor GetNameColor() const { return NameColor; }

	UFUNCTION(BlueprintPure, Category="World Name")
	bool ShouldShowNameplate() const { return bShowNameplate; }

	void SetDefaultDisplayName(const FText& InDisplayName) { DisplayName = InDisplayName; }
	void SetDefaultNameColor(const FLinearColor& InColor) { NameColor = InColor; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="World Name")
	FText DisplayName = NSLOCTEXT("WorldNames", "Enemy", "Enemy");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="World Name")
	FLinearColor NameColor = FLinearColor(1.0f, 0.25f, 0.12f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="World Name")
	bool bShowNameplate = true;
};
