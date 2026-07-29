#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArcadeFlightHUD.generated.h"

UCLASS()
class SAMOLOTY_API AArcadeFlightHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Crosshair")
	FLinearColor CrosshairColor = FLinearColor(0.2f, 0.9f, 1.0f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category="Crosshair", meta=(ClampMin="0"))
	float CrosshairGap = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair", meta=(ClampMin="1"))
	float CrosshairLineLength = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair", meta=(ClampMin="0.5"))
	float CrosshairThickness = 1.5f;
};
