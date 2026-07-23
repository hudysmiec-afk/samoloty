#include "RifleTracerVisual.h"

#include "Components/StaticMeshComponent.h"

ARifleTracerVisual::ARifleTracerVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetActorEnableCollision(false);

	TracerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracerMesh"));
	TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TracerMesh->SetGenerateOverlapEvents(false);
	RootComponent = TracerMesh;
}

void ARifleTracerVisual::InitializeTracer(const FVector& Start, const FVector& End,
	const float Speed, const float Length, const float Width)
{
	TraceStart = Start;
	TraceEnd = End;
	const FVector Delta = End - Start;
	TotalDistance = Delta.Size();
	TraceDirection = Delta.GetSafeNormal();
	VisualSpeed = FMath::Max(1.0f, Speed);
	VisualLength = FMath::Max(1.0f, Length);
	VisualWidth = FMath::Max(0.1f, Width);
	HeadDistance = 0.0f;
	bInitialized = TotalDistance > KINDA_SMALL_NUMBER;
	UpdateVisual();
	if (!bInitialized)
	{
		Destroy();
	}
}

void ARifleTracerVisual::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized)
	{
		return;
	}
	HeadDistance = FMath::Min(TotalDistance, HeadDistance + VisualSpeed * DeltaSeconds);
	UpdateVisual();
	if (HeadDistance >= TotalDistance)
	{
		Destroy();
	}
}

void ARifleTracerVisual::UpdateVisual()
{
	if (!bInitialized || !TracerMesh)
	{
		return;
	}
	const float TailDistance = FMath::Max(0.0f, HeadDistance - VisualLength);
	const float VisibleLength = FMath::Max(1.0f, HeadDistance - TailDistance);
	const FVector Center = TraceStart + TraceDirection * ((HeadDistance + TailDistance) * 0.5f);
	SetActorLocationAndRotation(Center, TraceDirection.Rotation());
	// Engine cube is 100 cm long on X. Blueprint meshes may override this scale.
	TracerMesh->SetRelativeScale3D(FVector(VisibleLength / 100.0f, VisualWidth / 100.0f, VisualWidth / 100.0f));
}
