#pragma once

#include "CoreMinimal.h"
#include "RocketWeaponComponent.h"
#include "GroundBarrageWeaponComponent.generated.h"

/** Barrage variant that sends the shared rocket salvos toward a fixed area on the ground. */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UGroundBarrageWeaponComponent : public URocketWeaponComponent
{
	GENERATED_BODY()

public:
	UGroundBarrageWeaponComponent();
	virtual void BeginPlay() override;

	/** Local preview and authoritative firing both use this exact ground prediction. */
	bool GetPredictedImpactPoint(FVector& OutImpactPoint) const;
	float GetImpactSpreadRadius() const { return ImpactSpreadRadius; }

protected:
	virtual void PrepareSalvo(const FRocketBarrageStats& Stats) override;
	virtual void ConfigureRocketLaunchData(FRocketLaunchData& LaunchData,
		const FRocketSeparationPath& SeparationPath, FRandomStream& RandomStream) const override;
	virtual bool UsesDataOnlyRocketSimulation() const override { return false; }

	/** Random radius around the shared ground aim point used by rockets in a salvo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plane|Weapons|Ground Barrage",
		meta=(ClampMin="0", Units="cm"))
	float ImpactSpreadRadius = 1000.0f;

private:
	bool CalculateGroundAimPoint(float WeaponRange, FVector& OutImpactPoint) const;

	FVector CachedGroundAimPoint = FVector::ZeroVector;
};
