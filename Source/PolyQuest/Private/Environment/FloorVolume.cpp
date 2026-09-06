#include "Environment/FloorVolume.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Info.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

AFloorVolume::AFloorVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	RootComponent = BoundsBox;

	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundsBox->SetBoxExtent(FVector(1000.0f, 1000.0f, 300.0f));

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> PlayerGlobalsMPCObj(
		TEXT("/Game/_Materials/SeeThrough/MPC_PlayerGlobals.MPC_PlayerGlobals"));
	if (PlayerGlobalsMPCObj.Succeeded())
	{
		PlayerGlobalsMPC = PlayerGlobalsMPCObj.Object;
	}
}

void AFloorVolume::BeginPlay()
{
	Super::BeginPlay();

	GatherContainedActors();

	// Explicitly apply initial active state instantly to synchronize interior props and MPC
	SetFloorActive(bIsFloorActive, true);
}

void AFloorVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PlayerGlobalsMPC)
	{
		// Reset cutoff to fully visible on teardown so editor viewport and successor states remain clean
		PushCutoffZToMPC(10000.0f);
	}

	Super::EndPlay(EndPlayReason);
}

void AFloorVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsInterpolating)
	{
		UpdateCutoffZ(DeltaSeconds);
	}
}

void AFloorVolume::GatherContainedActors()
{
	ManagedInteriorActors.Reset();
	ManagedStructuralActors.Reset();

	if (!BoundsBox)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FBox VolumeBox = BoundsBox->Bounds.GetBox();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == this)
		{
			continue;
		}

		// Strictly exclude player, controllers, global managers, lights, volumes, and ignored actors
		if (Candidate->IsA<APlayerCharacter>() ||
			Candidate->IsA<AController>() ||
			Candidate->IsA<AInfo>() ||
			Candidate->IsA<ALight>() ||
			Candidate->IsA<APostProcessVolume>() ||
			Candidate->IsA<AFloorVolume>() ||
			Candidate->ActorHasTag(FName(TEXT("Floor.Ignore"))))
		{
			continue;
		}

		// Evaluate spatial inclusion via actor bounding box and center
		const FBox CandidateBounds = Candidate->GetComponentsBoundingBox(true);
		const FVector CandidateLocation = Candidate->GetActorLocation();
		const FVector CandidateCenter = CandidateBounds.GetCenter();

		// Anti-bleeding: If the actor's vertical center is below the volume box floor, it belongs to the lower level
		if (CandidateCenter.Z < VolumeBox.Min.Z)
		{
			continue;
		}

		// Anti-spanning: If the actor's bottom extends significantly below this volume (e.g. a tall wall starting from Floor 1),
		// strictly exclude it from actor-level hiding to prevent creating sky/void holes on Floor 1!
		if (CandidateBounds.Min.Z < VolumeBox.Min.Z - 50.0f && !Candidate->ActorHasTag(FName(TEXT("Floor.Structural"))))
		{
			continue;
		}

		// Must be contained within horizontal bounds and within floor height
		const bool bCenterInside = VolumeBox.IsInside(CandidateCenter);
		const bool bOriginInside = VolumeBox.IsInside(CandidateLocation) && (CandidateLocation.Z >= VolumeBox.Min.Z);

		if (bCenterInside || bOriginInside)
		{
			if (IsStructuralActor(Candidate))
			{
				ManagedStructuralActors.Add(Candidate);
			}
			else
			{
				ManagedInteriorActors.Add(Candidate);
			}
		}

	}
}

bool AFloorVolume::IsStructuralActor(const AActor* CandidateActor)
{
	if (!CandidateActor)
	{
		return false;
	}

	// Manual tag overrides take highest precedence
	if (CandidateActor->ActorHasTag(FName(TEXT("Floor.Structural"))))
	{
		return true;
	}
	if (CandidateActor->ActorHasTag(FName(TEXT("Floor.Interior"))))
	{
		return false;
	}

	// 1. Negative interior keywords: props, wall-attached fixtures, furniture, lights must never be classified as structural
	static const TCHAR* InteriorKeywords[] = {
		TEXT("Torch"),
		TEXT("Light"),
		TEXT("Lamp"),
		TEXT("Candle"),
		TEXT("Fire"),
		TEXT("Flame"),
		TEXT("Chandelier"),
		TEXT("Banner"),
		TEXT("Shelf"),
		TEXT("Furniture"),
		TEXT("Prop"),
		TEXT("Barrel"),
		TEXT("Chair"),
		TEXT("Table"),
		TEXT("Chest"),
		TEXT("Box"),
		TEXT("Crate"),
		TEXT("Pot"),
		TEXT("Urn"),
		TEXT("Item"),
		TEXT("Pickup"),
		TEXT("Weapon")
	};

	const FString ActorName = CandidateActor->GetName();
	for (const TCHAR* Keyword : InteriorKeywords)
	{
		if (ActorName.Contains(Keyword, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	// Actors owning point/spot lights or particles are interior fixtures
	if (CandidateActor->FindComponentByClass<ULightComponent>())
	{
		return false;
	}

	// 2. Positive structural keywords indicating architecture participating in MPC FloorCutoffZ clipping
	static const TCHAR* StructuralKeywords[] = {
		TEXT("Wall"),
		TEXT("Floor"),
		TEXT("Arch"),
		TEXT("Ceiling"),
		TEXT("Roof"),
		TEXT("Stair"),
		TEXT("Pillar"),
		TEXT("Column"),
		TEXT("Bridge")
	};

	for (const TCHAR* Keyword : StructuralKeywords)
	{
		if (ActorName.Contains(Keyword, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// Inspect mesh component asset names and material master names
	TArray<UStaticMeshComponent*> MeshComps;
	CandidateActor->GetComponents<UStaticMeshComponent>(MeshComps);

	for (const UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp)
		{
			continue;
		}

		if (const UStaticMesh* Mesh = MeshComp->GetStaticMesh())
		{
			const FString MeshName = Mesh->GetName();

			// Negative check on mesh name
			for (const TCHAR* Keyword : InteriorKeywords)
			{
				if (MeshName.Contains(Keyword, ESearchCase::IgnoreCase))
				{
					return false;
				}
			}

			// Positive check on mesh name
			for (const TCHAR* Keyword : StructuralKeywords)
			{
				if (MeshName.Contains(Keyword, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}

		const int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
		{
			if (const UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx))
			{
				const FString MatName = Mat->GetName();
				if (MatName.Contains(TEXT("M_Tiling_Master"), ESearchCase::IgnoreCase) ||
					MatName.Contains(TEXT("M_StoneWall"), ESearchCase::IgnoreCase) ||
					MatName.Contains(TEXT("M_Decorative_Arches"), ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void AFloorVolume::SetFloorActive(bool bActive, bool bInstant)
{
	if (bActive == bIsFloorActive && !bInstant)
	{
		return;
	}

	bIsFloorActive = bActive;
	TargetCutoffZ = bActive ? ActiveCutoffZ : InactiveCutoffZ;

	// Toggle interior actors visibility: collision is NEVER disabled to prevent physics falling / AI drop-through
	for (TWeakObjectPtr<AActor>& ActorPtr : ManagedInteriorActors)
	{
		if (AActor* Actor = ActorPtr.Get())
		{
			Actor->SetActorHiddenInGame(!bActive);
		}
	}

	// If reactivating floor, immediately make structural actors visible
	if (bActive && bHideStructuralActorsWhenInactive)
	{
		for (TWeakObjectPtr<AActor>& ActorPtr : ManagedStructuralActors)
		{
			if (AActor* Actor = ActorPtr.Get())
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}

	if (bInstant || FadeDuration <= 0.0f)
	{
		CurrentCutoffZ = TargetCutoffZ;
		PushCutoffZToMPC(CurrentCutoffZ);
		bIsInterpolating = false;
		SetActorTickEnabled(false);

		// If deactivating instantly, hide structural actors now (collision strictly preserved)
		if (!bActive && bHideStructuralActorsWhenInactive)
		{
			for (TWeakObjectPtr<AActor>& ActorPtr : ManagedStructuralActors)
			{
				if (AActor* Actor = ActorPtr.Get())
				{
					Actor->SetActorHiddenInGame(true);
				}
			}
		}
	}
	else
	{
		bIsInterpolating = true;
		SetActorTickEnabled(true);
	}
}

void AFloorVolume::UpdateCutoffZ(float DeltaSeconds)
{
	const float InterpSpeed = FMath::Abs(ActiveCutoffZ - InactiveCutoffZ) / FMath::Max(0.01f, FadeDuration);
	CurrentCutoffZ = FMath::FInterpConstantTo(CurrentCutoffZ, TargetCutoffZ, DeltaSeconds, InterpSpeed);

	PushCutoffZToMPC(CurrentCutoffZ);

	if (FMath::IsNearlyEqual(CurrentCutoffZ, TargetCutoffZ, 0.5f))
	{
		CurrentCutoffZ = TargetCutoffZ;
		PushCutoffZToMPC(CurrentCutoffZ);
		bIsInterpolating = false;
		SetActorTickEnabled(false);

		// When fade-out interpolation completes for inactive floor, hide structural actors (collision strictly preserved)
		if (!bIsFloorActive && bHideStructuralActorsWhenInactive)
		{
			for (TWeakObjectPtr<AActor>& ActorPtr : ManagedStructuralActors)
			{
				if (AActor* Actor = ActorPtr.Get())
				{
					Actor->SetActorHiddenInGame(true);
				}
			}
		}
	}
}

void AFloorVolume::PushCutoffZToMPC(float InCutoffZ)
{
	UWorld* World = GetWorld();
	if (!World || !PlayerGlobalsMPC)
	{
		return;
	}

	UKismetMaterialLibrary::SetScalarParameterValue(
		World,
		PlayerGlobalsMPC,
		FName(TEXT("FloorCutoffZ")),
		InCutoffZ
	);
}
