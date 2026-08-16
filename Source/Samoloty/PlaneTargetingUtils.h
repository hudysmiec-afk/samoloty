#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace PlaneTargeting
{
	/** Uses the real query hitbox instead of cameras, Niagara and other visual bounds. */
	inline void GetTargetBounds(const AActor* Target, FVector& OutCenter, FVector& OutExtent)
	{
		OutCenter = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		OutExtent = FVector::ZeroVector;
		if (!Target)
		{
			return;
		}

		FBox DamageBounds(ForceInit);
		bool bHasDamageBounds = false;
		const UPrimitiveComponent* BestPrimitive = nullptr;
		int32 BestScore = TNumericLimits<int32>::Lowest();
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Target->GetComponents(Primitives);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive || !Primitive->IsRegistered()
				|| !Primitive->IsQueryCollisionEnabled())
			{
				continue;
			}
			if (Primitive->ComponentHasTag(TEXT("DamageHitbox")))
			{
				DamageBounds += Primitive->Bounds.GetBox();
				bHasDamageBounds = true;
				continue;
			}
			int32 Score = 0;
			if (Primitive->GetCollisionObjectType() == ECC_Pawn)
			{
				Score += 100;
			}
			if (Primitive->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block)
			{
				Score += 10;
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				BestPrimitive = Primitive;
			}
		}

		if (bHasDamageBounds)
		{
			OutCenter = DamageBounds.GetCenter();
			OutExtent = DamageBounds.GetExtent();
			return;
		}

		if (BestPrimitive)
		{
			OutCenter = BestPrimitive->Bounds.Origin;
			OutExtent = BestPrimitive->Bounds.BoxExtent;
			return;
		}
		Target->GetActorBounds(false, OutCenter, OutExtent);
	}
}
