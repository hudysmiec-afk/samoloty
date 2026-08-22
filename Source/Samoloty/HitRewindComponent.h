#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HitRewindComponent.generated.h"

class UPrimitiveComponent;

/** Records a short server-only transform history for tagged damage hitboxes. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SAMOLOTY_API UHitRewindComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitRewindComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Estimates the server timestamp represented by the remote aircraft currently
	 * visible to this client. The cap must cover the normal interpolation delay plus
	 * one-way latency, otherwise high-ping shooters have to lead a visible hitscan target.
	 */
	static double EstimateClientViewServerTime(
		const AActor* Viewer, double CurrentEstimatedServerTime);

	/** Sanitizes an untrusted client timestamp against the server's rewind policy. */
	static double ClampRequestedServerTime(double ServerNow, double RequestedServerTime);

	static constexpr double InterpolationBufferSeconds = 0.050;
	/** Maximum age of the world shown to a shooter that receives full compensation. */
	static constexpr double MaxCompensatedViewAgeSeconds = 0.150;
	/** Also includes the RPC's trip from the shooter back to the server. */
	static constexpr double MaxServerRewindSeconds = 0.250;

private:
	friend class FScopedHitboxRewind;

	struct FSnapshot
	{
		double ServerTime = 0.0;
		TArray<FTransform> HitboxTransforms;
	};

	bool BuildSnapshotAt(double ServerTime, TArray<FTransform>& OutTransforms) const;
	void RecordSnapshot();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> DamageHitboxes;

	TArray<FSnapshot> History;
	static TArray<TWeakObjectPtr<UHitRewindComponent>> Registry;
	/** Keep a small sampling margin beyond the largest accepted rewind request. */
	static constexpr double HistoryDuration = MaxServerRewindSeconds + 0.050;
};

/**
 * Temporarily moves all relevant server hitboxes to a historical timestamp.
 * Destruction restores every transform before gameplay continues.
 */
class SAMOLOTY_API FScopedHitboxRewind
{
public:
	FScopedHitboxRewind(UWorld* World, double ServerTime, const AActor* IgnoredActor);
	~FScopedHitboxRewind();

	FScopedHitboxRewind(const FScopedHitboxRewind&) = delete;
	FScopedHitboxRewind& operator=(const FScopedHitboxRewind&) = delete;

private:
	struct FRestoreEntry
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		FTransform Transform;
	};

	TArray<FRestoreEntry> RestoreEntries;
};
