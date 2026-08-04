#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RadarComponent.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ERadarAltitudeRelation : uint8
{
	Level,
	Above,
	Below
};

/**
 * Local contact sensor used by the HUD and targeting components. Contact lists are
 * presentation data only; authoritative weapon requests are checked independently.
 */
UCLASS(ClassGroup=(Plane), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API URadarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URadarComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	const TArray<TWeakObjectPtr<AActor>>& GetDetectedContacts() const { return DetectedContacts; }
	bool IsDetected(const AActor* Candidate) const;
	bool IsDetectableTarget(const AActor* Candidate, float RangeTolerance = 1.0f) const;
	bool IsWithinMissileTargetingRange(const AActor* Candidate, float WeaponRange,
		float RangeTolerance = 1.0f) const;
	ERadarAltitudeRelation GetAltitudeRelation(const AActor* Candidate) const;

	UFUNCTION(BlueprintPure, Category="Plane|Radar")
	float GetDetectionRange() const;

	UFUNCTION(BlueprintPure, Category="Plane|Radar")
	float GetGunTargetingRange() const;

	UFUNCTION(BlueprintPure, Category="Plane|Radar")
	float GetMissileTargetingRange() const;

	/** Contact membership is refreshed slowly; HUD positions use live actor transforms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Radar",
		meta=(ClampMin="0.05", Units="s"))
	float ScanInterval = 0.2f;

	/** Vertical difference below which a contact uses the level icon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Radar",
		meta=(ClampMin="0", Units="cm"))
	float AltitudeThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plane|Radar|Debug")
	bool bShowRadarDebug = false;

private:
	void ScanContacts();
	void DrawRadarDebug() const;

	TArray<TWeakObjectPtr<AActor>> DetectedContacts;
	float ScanAccumulator = 0.0f;
};
