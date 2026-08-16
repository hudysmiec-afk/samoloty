#include "FlightBoundsVolume.h"

#include "Components/BoxComponent.h"

AFlightBoundsVolume::AFlightBoundsVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	RootComponent = BoundsBox;
	BoundsBox->InitBoxExtent(FVector(100000.0f, 100000.0f, 50000.0f));
	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsBox->SetGenerateOverlapEvents(false);
	BoundsBox->SetHiddenInGame(true);
}

FVector AFlightBoundsVolume::GetLocalExtent() const
{
	return BoundsBox ? BoundsBox->GetUnscaledBoxExtent() : FVector::ZeroVector;
}

FVector AFlightBoundsVolume::GetAxisWorldScale() const
{
	if (!BoundsBox)
	{
		return FVector::OneVector;
	}
	const FVector Scale = BoundsBox->GetComponentTransform().GetScale3D().GetAbs();
	return FVector(
		FMath::Max(Scale.X, UE_SMALL_NUMBER),
		FMath::Max(Scale.Y, UE_SMALL_NUMBER),
		FMath::Max(Scale.Z, UE_SMALL_NUMBER));
}

void AFlightBoundsVolume::GetBoundaryInfo(const FVector& WorldLocation,
	float& OutWarningAlpha, bool& bOutOutside) const
{
	OutWarningAlpha = 0.0f;
	bOutOutside = false;
	if (!BoundsBox)
	{
		return;
	}

	const FVector Local = BoundsBox->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = GetLocalExtent();
	const FVector Scale = GetAxisWorldScale();
	const float Clearances[] = {
		(Extent.X - FMath::Abs(Local.X)) * Scale.X,
		(Extent.Y - FMath::Abs(Local.Y)) * Scale.Y,
		(Extent.Z - FMath::Abs(Local.Z)) * Scale.Z
	};
	const int32 AxisCount = bLimitAltitude ? 3 : 2;
	float MinimumClearance = TNumericLimits<float>::Max();
	for (int32 Axis = 0; Axis < AxisCount; ++Axis)
	{
		MinimumClearance = FMath::Min(MinimumClearance, Clearances[Axis]);
		bOutOutside |= Clearances[Axis] < 0.0f;
	}
	if (WarningDistance > UE_SMALL_NUMBER)
	{
		OutWarningAlpha = 1.0f - FMath::Clamp(
			MinimumClearance / WarningDistance, 0.0f, 1.0f);
	}
}

bool AFlightBoundsVolume::IsSafelyInside(const FVector& WorldLocation) const
{
	if (!BoundsBox)
	{
		return true;
	}
	const FVector Local = BoundsBox->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = GetLocalExtent();
	const FVector Scale = GetAxisWorldScale();
	const FVector LocalInset(
		ReleaseInset / Scale.X,
		ReleaseInset / Scale.Y,
		ReleaseInset / Scale.Z);
	const bool bInsideHorizontal = FMath::Abs(Local.X) <= FMath::Max(0.0f, Extent.X - LocalInset.X)
		&& FMath::Abs(Local.Y) <= FMath::Max(0.0f, Extent.Y - LocalInset.Y);
	const bool bInsideVertical = !bLimitAltitude
		|| FMath::Abs(Local.Z) <= FMath::Max(0.0f, Extent.Z - LocalInset.Z);
	return bInsideHorizontal && bInsideVertical;
}

FVector AFlightBoundsVolume::GetReturnTarget(const FVector& WorldLocation) const
{
	if (!BoundsBox)
	{
		return GetActorLocation();
	}
	FVector Local = BoundsBox->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = GetLocalExtent();
	const FVector Scale = GetAxisWorldScale();
	const FVector LocalDepth(
		ReturnTargetDepth / Scale.X,
		ReturnTargetDepth / Scale.Y,
		ReturnTargetDepth / Scale.Z);

	if (FMath::Abs(Local.X) > Extent.X)
	{
		Local.X = FMath::Sign(Local.X) * FMath::Max(0.0f, Extent.X - LocalDepth.X);
	}
	if (FMath::Abs(Local.Y) > Extent.Y)
	{
		Local.Y = FMath::Sign(Local.Y) * FMath::Max(0.0f, Extent.Y - LocalDepth.Y);
	}
	if (bLimitAltitude && FMath::Abs(Local.Z) > Extent.Z)
	{
		Local.Z = FMath::Sign(Local.Z) * FMath::Max(0.0f, Extent.Z - LocalDepth.Z);
	}

	// Make the target sufficiently far ahead of the aircraft on the inward path.
	// This avoids a twitchy near-point correction and produces a readable broad arc.
	const FVector LocalCenter = FVector::ZeroVector;
	Local = FMath::Lerp(Local, LocalCenter, 0.35f);
	if (!bLimitAltitude)
	{
		const FVector OriginalLocal =
			BoundsBox->GetComponentTransform().InverseTransformPosition(WorldLocation);
		Local.Z = OriginalLocal.Z;
	}
	return BoundsBox->GetComponentTransform().TransformPosition(Local);
}
