#include "TargetSelectionComponent.h"

#include "RadarComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UTargetSelectionComponent::UTargetSelectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UTargetSelectionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator >= FMath::Max(0.02f, SelectionRefreshInterval))
	{
		RefreshAccumulator = 0.0f;
		RefreshSelection();
	}
	DrawSelectionDebug();
}

void UTargetSelectionComponent::CycleSelectedTarget()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GetWorld())
	{
		return;
	}

	if (SelectedTarget.IsValid())
	{
		RejectedTargetsUntil.Add(SelectedTarget,
			GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, PreviousTargetRejectDuration));
	}
	SetSelectedTarget(nullptr);
	RefreshAccumulator = 0.0f;
	RefreshSelection();
}

void UTargetSelectionComponent::RefreshSelection()
{
	const URadarComponent* Radar = GetOwner()
		? GetOwner()->FindComponentByClass<URadarComponent>() : nullptr;
	if (!Radar || !GetWorld())
	{
		SetSelectedTarget(nullptr);
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	CleanupRejectedTargets(Now);
	if (SelectedTarget.IsValid() && IsSelectable(SelectedTarget.Get(), Now))
	{
		return;
	}

	AActor* BestTarget = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	FVector ViewLocation;
	FVector ViewDirection;
	GetSelectionViewPoint(ViewLocation, ViewDirection);
	const float RadarRange = FMath::Max(1.0f, Radar->GetDetectionRange());
	for (const TWeakObjectPtr<AActor>& Contact : Radar->GetDetectedContacts())
	{
		AActor* Candidate = Contact.Get();
		if (!IsSelectable(Candidate, Now))
		{
			continue;
		}

		float AimError = 0.0f;
		if (!IsUnderCrosshair(Candidate, ViewLocation, ViewDirection, AimError))
		{
			continue;
		}

		const float Distance = FVector::Distance(ViewLocation, Candidate->GetActorLocation());
		// Aim error wins; distance only resolves overlapping targets.
		const float Score = AimError * 10.0f + Distance / RadarRange;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}
	SetSelectedTarget(BestTarget);
}

void UTargetSelectionComponent::CleanupRejectedTargets(const double Now)
{
	for (auto It = RejectedTargetsUntil.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= Now)
		{
			It.RemoveCurrent();
		}
	}
}

void UTargetSelectionComponent::SetSelectedTarget(AActor* NewTarget)
{
	AActor* PreviousTarget = SelectedTarget.Get();
	if (PreviousTarget == NewTarget)
	{
		return;
	}
	SelectedTarget = NewTarget;
	OnSelectedTargetChanged.Broadcast(PreviousTarget, NewTarget);
}

bool UTargetSelectionComponent::IsSelectable(const AActor* Candidate, const double Now) const
{
	if (!Candidate)
	{
		return false;
	}
	const URadarComponent* Radar = GetOwner()
		? GetOwner()->FindComponentByClass<URadarComponent>() : nullptr;
	if (!Radar || !Radar->IsDetected(Candidate))
	{
		return false;
	}
	const double* RejectedUntil = RejectedTargetsUntil.Find(
		TWeakObjectPtr<AActor>(const_cast<AActor*>(Candidate)));
	return !RejectedUntil || *RejectedUntil <= Now;
}

bool UTargetSelectionComponent::IsUnderCrosshair(const AActor* Candidate,
	const FVector& ViewLocation, const FVector& ViewDirection, float& OutAimError) const
{
	OutAimError = TNumericLimits<float>::Max();
	if (!Candidate)
	{
		return false;
	}

	FVector TargetCenter;
	FVector TargetExtent;
	Candidate->GetActorBounds(true, TargetCenter, TargetExtent);
	const FVector ToTarget = TargetCenter - ViewLocation;
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Alignment = FVector::DotProduct(ViewDirection, ToTarget / Distance);
	if (Alignment <= 0.0f)
	{
		return false;
	}
	const float AimAngleDegrees = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(Alignment, -1.0f, 1.0f)));
	const float BoundsAngleDegrees = FMath::RadiansToDegrees(FMath::Asin(
		FMath::Clamp(TargetExtent.Size() / Distance, 0.0f, 1.0f)));
	const float AllowedAngleDegrees = FMath::Max(0.01f,
		BoundsAngleDegrees + FMath::Max(0.0f, CrosshairToleranceDegrees));
	OutAimError = AimAngleDegrees / AllowedAngleDegrees;
	return AimAngleDegrees <= AllowedAngleDegrees;
}

void UTargetSelectionComponent::GetSelectionViewPoint(FVector& OutLocation,
	FVector& OutDirection) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (PlayerController)
	{
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(OutLocation, ViewRotation);
		OutDirection = ViewRotation.Vector();
		return;
	}
	OutLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	OutDirection = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
}

void UTargetSelectionComponent::DrawSelectionDebug() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bShowSelectionDebug || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GEngine)
	{
		return;
	}
	const AActor* Target = SelectedTarget.Get();
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow,
		Target ? FString::Printf(TEXT("SELECTED: %s | Shift: clear"), *Target->GetName())
			: TEXT("SELECTED: aim crosshair at a radar contact"));
}
