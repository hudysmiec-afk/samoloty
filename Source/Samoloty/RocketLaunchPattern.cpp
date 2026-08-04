#include "RocketLaunchPattern.h"

FRocketSeparationPath RocketLaunchPattern::BuildSeparationPath(
	const FVector& StartLocation,
	const FVector& CommonFormationCenter,
	const FVector& Forward,
	const FVector& Right,
	const FVector& Up,
	const bool bLeftSide,
	const int32 SideIndex,
	const int32 SideCount,
	FRandomStream& RandomStream,
	const FRocketSeparationSettings& Settings)
{
	FRocketSeparationPath Path;
	Path.StartLocation = StartLocation;

	const int32 SafeSideCount = FMath::Max(1, SideCount);
	const float SectorAlpha =
		(static_cast<float>(FMath::Max(0, SideIndex)) + RandomStream.FRand()) / SafeSideCount;
	const float StartAngle = Settings.bUseFullDisk ? 0.0f : (bLeftSide ? HALF_PI : -HALF_PI);
	const float AngularRange = Settings.bUseFullDisk ? 2.0f * PI : PI;
	const float DiskAngle = StartAngle + SectorAlpha * AngularRange;
	const float RadiusAlpha = FMath::Sqrt(RandomStream.FRand());
	const FVector DiskDirection = Right * FMath::Cos(DiskAngle) + Up * FMath::Sin(DiskAngle);
	const float SpreadTangent = FMath::Tan(FMath::DegreesToRadians(Settings.SpreadAngleDegrees));

	Path.Direction = (Forward + DiskDirection * SpreadTangent * RadiusAlpha)
		.GetSafeNormal(SMALL_NUMBER, Forward);
	const float ForwardDistance = FMath::Max(0.0f, Settings.ForwardDistance);
	const FVector SeparationOffset = DiskDirection * FMath::Max(0.0f, Settings.Radius) * RadiusAlpha;
	Path.EndPoint = CommonFormationCenter + Forward * ForwardDistance + SeparationOffset;

	const FVector LateralTravel =
		Path.EndPoint - (StartLocation + Forward * ForwardDistance);
	const float ControlHandleLength = ForwardDistance * FMath::Max(0.0f, Settings.CurveStrength);
	Path.ControlPoint1 = StartLocation + Forward * (ControlHandleLength * 0.35f)
		+ LateralTravel * 0.35f;
	Path.ControlPoint2 = Path.EndPoint - Path.Direction * ControlHandleLength;
	Path.bUseCurve = ForwardDistance > KINDA_SMALL_NUMBER;
	return Path;
}
