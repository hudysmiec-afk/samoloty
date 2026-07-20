#include "JetStatsComponent.h"

UJetStatsComponent::UJetStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	EffectiveFlightStats = BaseFlightStats;
}

void UJetStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	RecalculateStats();
}

void UJetStatsComponent::RecalculateStats()
{
	// Item, perk and passive-tree modifiers will be aggregated here later.
	EffectiveFlightStats = BaseFlightStats;
}
