#include "ArcadeFlightHUD.h"

#include "Engine/Canvas.h"

void AArcadeFlightHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	DrawLine(Center.X - CrosshairGap - CrosshairLineLength, Center.Y,
		Center.X - CrosshairGap, Center.Y, CrosshairColor, CrosshairThickness);
	DrawLine(Center.X + CrosshairGap, Center.Y,
		Center.X + CrosshairGap + CrosshairLineLength, Center.Y, CrosshairColor, CrosshairThickness);
	DrawLine(Center.X, Center.Y - CrosshairGap - CrosshairLineLength,
		Center.X, Center.Y - CrosshairGap, CrosshairColor, CrosshairThickness);
	DrawLine(Center.X, Center.Y + CrosshairGap,
		Center.X, Center.Y + CrosshairGap + CrosshairLineLength, CrosshairColor, CrosshairThickness);
}
