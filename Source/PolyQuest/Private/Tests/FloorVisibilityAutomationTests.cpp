// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/Player/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Environment/FloorTriggerVolume.h"
#include "Environment/FloorVolume.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorVisibilityAutomationTest, "PolyQuest.Environment.FloorVisibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFloorVisibilityAutomationTest::RunTest(const FString&)
{
	// -------------------------------------------------------------------------
	// SECTION 1: Static Actor Classification Helper Unit Tests
	// -------------------------------------------------------------------------
	{
		// Null safety
		TestFalse(TEXT("Null actor is not structural"), AFloorVolume::IsStructuralActor(nullptr));

		// Simulated Actor instances with specific names
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FloorVisibilityTestWorld"));
		WorldContext.SetCurrentWorld(World);

		if (!TestNotNull(TEXT("Test World created"), World))
		{
			return false;
		}

		struct FTestScopeCleanup
		{
			UWorld* WorldToDestroy;
			~FTestScopeCleanup()
			{
				if (WorldToDestroy)
				{
					GEngine->DestroyWorldContext(WorldToDestroy);
					WorldToDestroy->DestroyWorld(false);
				}
			}
		} ScopeCleanup{ World };

		auto SetupActorRoot = [](AActor* Actor, const FVector& Location)
		{
			if (Actor)
			{
				USceneComponent* RootComp = NewObject<USceneComponent>(Actor, TEXT("RootComponent"));
				Actor->SetRootComponent(RootComp);
				RootComp->RegisterComponent();
				Actor->SetActorLocation(Location);
			}
		};

		// 1.1 Structural Name Classification (Wall, Floor, Arch, Ceiling)
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(TEXT("SM_Dungeon_StoneWall_01"));
		AActor* WallActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, SpawnParams);
		SetupActorRoot(WallActor, FVector(0.f, 0.f, 500.f));
		TestNotNull(TEXT("WallActor spawned"), WallActor);
		if (WallActor)
		{
			TestTrue(TEXT("Actor named Wall is classified as Structural"), AFloorVolume::IsStructuralActor(WallActor));
		}

		SpawnParams.Name = FName(TEXT("SM_Dungeon_Floor_Tile_02"));
		AActor* FloorTileActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, SpawnParams);
		SetupActorRoot(FloorTileActor, FVector(0.f, 0.f, 500.f));
		TestNotNull(TEXT("FloorTileActor spawned"), FloorTileActor);
		if (FloorTileActor)
		{
			TestTrue(TEXT("Actor named Floor is classified as Structural"), AFloorVolume::IsStructuralActor(FloorTileActor));
		}

		// 1.2 Interior Prop Classification (Chair, Torch, Barrel)
		SpawnParams.Name = FName(TEXT("SM_Wooden_Chair_A"));
		AActor* ChairActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, SpawnParams);
		SetupActorRoot(ChairActor, FVector(0.f, 0.f, 500.f));
		TestNotNull(TEXT("ChairActor spawned"), ChairActor);
		if (ChairActor)
		{
			TestFalse(TEXT("Actor named Chair is classified as Interior (Not Structural)"), AFloorVolume::IsStructuralActor(ChairActor));
		}

		SpawnParams.Name = FName(TEXT("BP_WallTorch_Dungeon"));
		AActor* TorchActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, SpawnParams);
		SetupActorRoot(TorchActor, FVector(0.f, 0.f, 500.f));
		TestNotNull(TEXT("TorchActor spawned"), TorchActor);
		if (TorchActor)
		{
			TestFalse(TEXT("Actor named Torch is classified as Interior"), AFloorVolume::IsStructuralActor(TorchActor));
		}

		// 1.3 Explicit Tag Override
		SpawnParams.Name = FName(TEXT("SM_SpecialProp"));
		AActor* TaggedStructural = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, SpawnParams);
		SetupActorRoot(TaggedStructural, FVector(0.f, 0.f, 500.f));
		if (TaggedStructural)
		{
			TaggedStructural->Tags.Add(FName(TEXT("Floor.Structural")));
			TestTrue(TEXT("Explicit Floor.Structural tag overrides default"), AFloorVolume::IsStructuralActor(TaggedStructural));
		}

		// -------------------------------------------------------------------------
		// SECTION 2: Spatial Gathering & Player Exclusion In AFloorVolume
		// -------------------------------------------------------------------------
		AFloorVolume* FloorVolume = World->SpawnActor<AFloorVolume>(FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator);
		TestNotNull(TEXT("FloorVolume spawned"), FloorVolume);
		if (FloorVolume)
		{
			FloorVolume->BoundsBox->SetBoxExtent(FVector(1000.f, 1000.f, 500.f));

			// Spawn a PlayerCharacter within the bounds
			APlayerCharacter* PlayerChar = World->SpawnActor<APlayerCharacter>(FVector(100.f, 100.f, 500.f), FRotator::ZeroRotator);
			TestNotNull(TEXT("PlayerChar spawned within volume"), PlayerChar);

			// Gather actors
			FloorVolume->GatherContainedActors();

			// Ensure player is strictly excluded
			bool bPlayerContained = false;
			for (const TWeakObjectPtr<AActor>& InteriorPtr : FloorVolume->ManagedInteriorActors)
			{
				if (InteriorPtr.Get() == PlayerChar)
				{
					bPlayerContained = true;
					break;
				}
			}
			for (const TWeakObjectPtr<AActor>& StructuralPtr : FloorVolume->ManagedStructuralActors)
			{
				if (StructuralPtr.Get() == PlayerChar)
				{
					bPlayerContained = true;
					break;
				}
			}
			TestFalse(TEXT("PlayerCharacter is strictly excluded from managed floor actors"), bPlayerContained);

			// Verify contained counts
			TestTrue(TEXT("WallActor captured in Structural list"), FloorVolume->ManagedStructuralActors.Contains(WallActor));
			TestTrue(TEXT("ChairActor captured in Interior list"), FloorVolume->ManagedInteriorActors.Contains(ChairActor));
		}

		// -------------------------------------------------------------------------
		// SECTION 3: Collision Preservation Contract Under Deactivation
		// -------------------------------------------------------------------------
		// Spawn a physics/collision prop to verify collision remains enabled when actor is hidden
		AStaticMeshActor* PhysicsProp = World->SpawnActor<AStaticMeshActor>(FVector(50.f, 50.f, 500.f), FRotator::ZeroRotator);
		TestNotNull(TEXT("PhysicsProp spawned"), PhysicsProp);
		if (PhysicsProp && PhysicsProp->GetStaticMeshComponent())
		{
			UStaticMeshComponent* MeshComp = PhysicsProp->GetStaticMeshComponent();
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MeshComp->SetCollisionObjectType(ECC_WorldDynamic);

			if (FloorVolume)
			{
				FloorVolume->ManagedInteriorActors.Add(PhysicsProp);

				// Deactivate floor: actors should be hidden, but collision must NEVER be modified or disabled!
				FloorVolume->SetFloorActive(false, true);

				TestTrue(TEXT("Interior actor is hidden in game when floor is inactive"), PhysicsProp->IsHidden());
				TestTrue(TEXT("Structural actor is hidden in game when floor is inactive"), WallActor->IsHidden());
				TestEqual(
					TEXT("Interior actor collision remains QueryAndPhysics (NEVER disabled)"),
					MeshComp->GetCollisionEnabled(),
					ECollisionEnabled::QueryAndPhysics
				);

				// Reactivate floor
				FloorVolume->SetFloorActive(true, true);
				TestFalse(TEXT("Interior actor is visible in game when floor is active"), PhysicsProp->IsHidden());
				TestFalse(TEXT("Structural actor is visible in game when floor is active"), WallActor->IsHidden());
				TestEqual(
					TEXT("Interior actor collision remains QueryAndPhysics after reactivation"),
					MeshComp->GetCollisionEnabled(),
					ECollisionEnabled::QueryAndPhysics
				);
			}
		}

		// -------------------------------------------------------------------------
		// SECTION 4: Floor Trigger Hysteresis & Floor Switching
		// -------------------------------------------------------------------------
		AFloorTriggerVolume* BottomTrigger = World->SpawnActor<AFloorTriggerVolume>(FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator);
		AFloorTriggerVolume* TopTrigger = World->SpawnActor<AFloorTriggerVolume>(FVector(0.f, 0.f, 600.f), FRotator::ZeroRotator);
		TestNotNull(TEXT("BottomTrigger spawned"), BottomTrigger);
		TestNotNull(TEXT("TopTrigger spawned"), TopTrigger);

		if (BottomTrigger && TopTrigger && FloorVolume)
		{
			FloorVolume->FloorIndex = 2;
			BottomTrigger->TargetFloorIndex = 1;
			BottomTrigger->TargetFloorVolume = FloorVolume;

			TopTrigger->TargetFloorIndex = 2;
			TopTrigger->TargetFloorVolume = FloorVolume;

			APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator);

			// Initially active
			FloorVolume->SetFloorActive(true, true);
			TestTrue(TEXT("FloorVolume initially active"), FloorVolume->bIsFloorActive);

			// Player overlaps bottom trigger (target floor 1 < volume floor 2): should deactivate floor 2
			BottomTrigger->HandleTriggerBeginOverlap(
				BottomTrigger->TriggerBox,
				Player,
				nullptr,
				0,
				false,
				FHitResult()
			);
			TestFalse(TEXT("Overlapping bottom trigger (Floor 1) deactivates Floor 2"), FloorVolume->bIsFloorActive);

			// Player overlaps bottom trigger again: idempotent, still inactive
			BottomTrigger->HandleTriggerBeginOverlap(
				BottomTrigger->TriggerBox,
				Player,
				nullptr,
				0,
				false,
				FHitResult()
			);
			TestFalse(TEXT("Repeated overlap on bottom trigger remains inactive (idempotent)"), FloorVolume->bIsFloorActive);

			// Player reaches top of stairs (Floor 2): overlaps top trigger -> activates floor 2
			TopTrigger->HandleTriggerBeginOverlap(
				TopTrigger->TriggerBox,
				Player,
				nullptr,
				0,
				false,
				FHitResult()
			);
			TestTrue(TEXT("Overlapping top trigger (Floor 2) reactivates Floor 2"), FloorVolume->bIsFloorActive);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
