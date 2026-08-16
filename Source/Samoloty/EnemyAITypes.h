#pragma once

#include "CoreMinimal.h"
#include "EnemyAITypes.generated.h"

UENUM(BlueprintType)
enum class EEnemyAggressionMode : uint8
{
	Aggressive UMETA(DisplayName="Aggressive"),
	Defensive UMETA(DisplayName="Defensive - retaliates when attacked"),
	DefensiveUntilLowHealth UMETA(DisplayName="Defensive until low health"),
	Passive UMETA(DisplayName="Passive")
};

UENUM(BlueprintType)
enum class EEnemyTargetPriority : uint8
{
	Closest UMETA(DisplayName="Closest"),
	FirstAttacker UMETA(DisplayName="First acquired / attacker"),
	LowestHealth UMETA(DisplayName="Lowest health"),
	HighestHealth UMETA(DisplayName="Highest health"),
	Random UMETA(DisplayName="Random"),
	HighestAggro UMETA(DisplayName="Highest damage aggro")
};
