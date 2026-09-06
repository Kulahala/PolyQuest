#include "Environment/FloorTriggerVolume.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Environment/FloorVolume.h"
#include "PolyQuest.h"

AFloorTriggerVolume::AFloorTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
}

void AFloorTriggerVolume::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AFloorTriggerVolume::HandleTriggerBeginOverlap);
	}

	// If no TargetFloorVolume is set manually, find the best matching AFloorVolume:
	// 1. Exact FloorIndex match
	// 2. Nearest in spatial distance
	if (!TargetFloorVolume)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			AFloorVolume* BestVolume = nullptr;
			float MinDistanceSq = TNumericLimits<float>::Max();
			const FVector MyLoc = GetActorLocation();

			for (TActorIterator<AFloorVolume> It(World); It; ++It)
			{
				AFloorVolume* Candidate = *It;
				if (!Candidate)
				{
					continue;
				}

				if (Candidate->FloorIndex == TargetFloorIndex)
				{
					BestVolume = Candidate;
					break;
				}

				const float DistSq = FVector::DistSquared(MyLoc, Candidate->GetActorLocation());
				if (DistSq < MinDistanceSq)
				{
					MinDistanceSq = DistSq;
					BestVolume = Candidate;
				}
			}

			TargetFloorVolume = BestVolume;
		}

		if (!TargetFloorVolume)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' failed to resolve a TargetFloorVolume in level for TargetFloorIndex %d."), *GetNameSafe(this), TargetFloorIndex);
		}
	}
}

void AFloorTriggerVolume::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->IsA<APlayerCharacter>())
	{
		return;
	}

	if (TargetFloorVolume)
	{
		// If the player's intended target floor is at or above the volume's floor, activate that floor;
		// otherwise, deactivate that higher floor so the player sees the lower floor clearly.
		const bool bActivate = (TargetFloorIndex >= TargetFloorVolume->FloorIndex);
		TargetFloorVolume->SetFloorActive(bActivate);
	}
}
