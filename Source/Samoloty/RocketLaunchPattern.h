#pragma once

#include "CoreMinimal.h"

struct FRocketSeparationSettings
{
	float SpreadAngleDegrees = 0.0f;
	float ForwardDistance = 0.0f;
	float Radius = 0.0f;
	float CurveStrength = 0.35f;
	/** False: left/right launchers share opposite halves. True: each launcher uses a full disk. */
	bool bUseFullDisk = false;
};

struct FRocketSeparationPath
{
	FVector StartLocation = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	FVector ControlPoint1 = FVector::ZeroVector;
	FVector ControlPoint2 = FVector::ZeroVector;
	FVector EndPoint = FVector::ZeroVector;
	bool bUseCurve = false;
};

namespace RocketLaunchPattern
{
	/** Builds the shared distance-driven launch path used by straight and homing rockets. */
	SAMOLOTY_API FRocketSeparationPath BuildSeparationPath(
		const FVector& StartLocation,
		const FVector& CommonFormationCenter,
		const FVector& Forward,
		const FVector& Right,
		const FVector& Up,
		bool bLeftSide,
		int32 SideIndex,
		int32 SideCount,
		FRandomStream& RandomStream,
		const FRocketSeparationSettings& Settings);
}
