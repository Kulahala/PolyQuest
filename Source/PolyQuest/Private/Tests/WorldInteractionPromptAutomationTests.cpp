// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Equipment/WorldWeaponPickup.h"
#include "Components/SphereComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameplayTagContainer.h"
#include "UI/WorldInteractionPromptWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldInteractionPromptAutomationTest, "PolyQuest.UI.WorldInteractionPrompt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldInteractionPromptAutomationTest::RunTest(const FString&)
{
	// -------------------------------------------------------------------------
	// SECTION 1: Native Widget Safe Setters, Test Injection & Localization
	// -------------------------------------------------------------------------
	{
		// 1.1 Headless Null-BindWidget Safety
		{
			UWorldInteractionPromptWidget* HeadlessWidget = NewObject<UWorldInteractionPromptWidget>(GetTransientPackage());
			TestNotNull(TEXT("Headless Prompt Widget created successfully"), HeadlessWidget);
			if (HeadlessWidget)
			{
				HeadlessWidget->SetPromptText(FText::FromString(TEXT("Test Prompt")));
			}
		}

		// 1.2 Injected TextBlock and Text Format Assertions
		{
			UWorldInteractionPromptWidget* Widget = NewObject<UWorldInteractionPromptWidget>(GetTransientPackage());
			UTextBlock* InjectedTextBlock = NewObject<UTextBlock>(Widget);
			Widget->SetTestPromptTextBlock(InjectedTextBlock);

			TestEqual(TEXT("Injected TextBlock matches getter"), Widget->GetTestPromptTextBlock(), InjectedTextBlock);

			// Format with weapon name
			const FText WeaponName = FText::FromString(TEXT("精钢剑"));
			const FText FormattedWithWeapon = FText::Format(
				NSLOCTEXT("PolyQuest", "PromptWithWeapon", "拾取 {0}"),
				WeaponName);
			Widget->SetPromptText(FormattedWithWeapon);
			TestEqual(TEXT("Prompt text with weapon matches format"), InjectedTextBlock->GetText().ToString(), TEXT("拾取 精钢剑"));

			// Fallback without weapon name
			const FText FallbackText = NSLOCTEXT("PolyQuest", "PromptFallback", "拾取");
			Widget->SetPromptText(FallbackText);
			TestEqual(TEXT("Prompt text fallback matches constant"), InjectedTextBlock->GetText().ToString(), TEXT("拾取"));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: World Setup for Player, Controller & Pickup Integration
	// -------------------------------------------------------------------------
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("InteractionPromptTestWorld"));
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

	APolyQuestPlayerController* PC = World->SpawnActor<APolyQuestPlayerController>();
	APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>();
	TestNotNull(TEXT("PlayerController spawned"), PC);
	TestNotNull(TEXT("PlayerCharacter spawned"), Player);

	if (!PC || !Player)
	{
		return false;
	}

	PC->SetTestInteractionPromptClass(UWorldInteractionPromptWidget::StaticClass());
	PC->Possess(Player);

	// -------------------------------------------------------------------------
	// SECTION 3: Controller Prompt Creation, Idempotency & Visibility Lifecycle
	// -------------------------------------------------------------------------
	{
		PC->TriggerTestEnsureInteractionPromptCreated();
		UWorldInteractionPromptWidget* PromptInst1 = PC->GetTestInteractionPromptInstance();
		TestNotNull(TEXT("Interaction prompt widget created"), PromptInst1);

		if (PromptInst1)
		{
			TestEqual(TEXT("Initial prompt visibility is Collapsed"), PromptInst1->GetVisibility(), ESlateVisibility::Collapsed);
		}

		// Idempotent creation
		PC->TriggerTestEnsureInteractionPromptCreated();
		UWorldInteractionPromptWidget* PromptInst2 = PC->GetTestInteractionPromptInstance();
		TestEqual(TEXT("Prompt instance is idempotent"), PromptInst1, PromptInst2);

		// Show and Hide
		PC->ShowInteractionPrompt(FText::FromString(TEXT("Test E")));
		if (PromptInst1)
		{
			TestEqual(TEXT("Show prompt sets visibility to HitTestInvisible"), PromptInst1->GetVisibility(), ESlateVisibility::HitTestInvisible);
		}

		PC->HideInteractionPrompt();
		if (PromptInst1)
		{
			TestEqual(TEXT("Hide prompt sets visibility to Collapsed"), PromptInst1->GetVisibility(), ESlateVisibility::Collapsed);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: Candidate Registration, Distance Arbitration & Tie-Break
	// -------------------------------------------------------------------------
	{
		UMeleeWeaponDefinition* WeaponDefA = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		WeaponDefA->InteractionDisplayName = FText::FromString(TEXT("武器A"));

		UMeleeWeaponDefinition* WeaponDefB = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		WeaponDefB->InteractionDisplayName = FText::FromString(TEXT("武器B"));

		AWorldWeaponPickup* PickupNear = World->SpawnActor<AWorldWeaponPickup>();
		PickupNear->SetWeaponDefinition(WeaponDefA);
		PickupNear->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));

		AWorldWeaponPickup* PickupFar = World->SpawnActor<AWorldWeaponPickup>();
		PickupFar->SetWeaponDefinition(WeaponDefB);
		PickupFar->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));

		Player->SetActorLocation(FVector::ZeroVector);

		// Register both
		Player->RegisterWorldPickupCandidate(PickupFar);
		Player->RegisterWorldPickupCandidate(PickupNear);

		TestEqual(TEXT("Candidate count is 2"), Player->GetTestWorldPickupCandidateCount(), 2);
		TestEqual(TEXT("Nearest pickup arbitrated as current candidate"), Player->GetCurrentWorldPickupCandidate(), PickupNear);

		UWorldInteractionPromptWidget* PromptInstance = PC->GetTestInteractionPromptInstance();
		if (PromptInstance)
		{
			TestEqual(TEXT("Prompt visible when candidate exists"), PromptInstance->GetVisibility(), ESlateVisibility::HitTestInvisible);
		}

		// Unregister nearest, next nearest should become candidate
		Player->UnregisterWorldPickupCandidate(PickupNear);
		TestEqual(TEXT("Far pickup becomes candidate after unregistering near"), Player->GetCurrentWorldPickupCandidate(), PickupFar);

		// Unregister far, prompt should collapse
		Player->UnregisterWorldPickupCandidate(PickupFar);
		TestNull(TEXT("Candidate cleared when all unregister"), Player->GetCurrentWorldPickupCandidate());
		if (PromptInstance)
		{
			TestEqual(TEXT("Prompt hidden when candidates empty"), PromptInstance->GetVisibility(), ESlateVisibility::Collapsed);
		}

		PickupNear->Destroy();
		PickupFar->Destroy();
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Validity Gates & FormerOwner Cooldown
	// -------------------------------------------------------------------------
	{
		UMeleeWeaponDefinition* WeaponDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		WeaponDef->InteractionDisplayName = FText::FromString(TEXT("测试剑"));

		AWorldWeaponPickup* Pickup = World->SpawnActor<AWorldWeaponPickup>();
		Pickup->SetWeaponDefinition(WeaponDef);
		Pickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));

		Pickup->AddTestOverlappingPlayer(Player);
		TestEqual(TEXT("Valid pickup is current candidate"), Player->GetCurrentWorldPickupCandidate(), Pickup);

		// Gate 1: No Definition
		Pickup->SetWeaponDefinition(nullptr);
		TestNull(TEXT("Pickup with null definition rejected"), Player->GetCurrentWorldPickupCandidate());

		// Restore definition
		Pickup->SetWeaponDefinition(WeaponDef);
		TestEqual(TEXT("Pickup restored when definition set"), Player->GetCurrentWorldPickupCandidate(), Pickup);

		// Gate 2: Collision Disabled
		Pickup->SetInteractionEnabled(false);
		TestNull(TEXT("Pickup with disabled collision rejected"), Player->GetCurrentWorldPickupCandidate());

		Pickup->SetInteractionEnabled(true);
		TestEqual(TEXT("Pickup restored when collision enabled"), Player->GetCurrentWorldPickupCandidate(), Pickup);

		// Gate 3: Interaction in progress
		Pickup->BeginInteraction();
		TestNull(TEXT("Pickup with interaction in progress rejected"), Player->GetCurrentWorldPickupCandidate());

		Pickup->EndInteraction();
		TestEqual(TEXT("Pickup restored when interaction ends"), Player->GetCurrentWorldPickupCandidate(), Pickup);

		// Gate 4: FormerOwner cooldown
		Pickup->InitializeDroppedPickup(WeaponDef, Player, 10.0f);
		TestFalse(TEXT("Cannot interact during FormerOwner cooldown"), Pickup->CanInteract(Player));
		TestNull(TEXT("FormerOwner cooling-down pickup rejected from candidate"), Player->GetCurrentWorldPickupCandidate());
		TestTrue(TEXT("FormerOwner cooldown timer active"), Player->HasTestFormerOwnerInteractionTimer());

		// Initialize with zero cooldown
		Pickup->InitializeDroppedPickup(WeaponDef, Player, 0.0f);
		TestTrue(TEXT("Can interact after cooldown expired"), Pickup->CanInteract(Player));
		TestEqual(TEXT("Pickup becomes candidate when cooldown is zero"), Player->GetCurrentWorldPickupCandidate(), Pickup);
		TestFalse(TEXT("Timer cleared when no cooldowns waiting"), Player->HasTestFormerOwnerInteractionTimer());

		Pickup->Destroy();
		Player->ClearWorldPickupInteractionState();
	}

	// -------------------------------------------------------------------------
	// SECTION 6: Movement-Driven Arbitration & Input Integrity
	// -------------------------------------------------------------------------
	{
		UMeleeWeaponDefinition* WeaponDefA = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		WeaponDefA->InteractionDisplayName = FText::FromString(TEXT("左武器"));

		UMeleeWeaponDefinition* WeaponDefB = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		WeaponDefB->InteractionDisplayName = FText::FromString(TEXT("右武器"));

		AWorldWeaponPickup* PickupLeft = World->SpawnActor<AWorldWeaponPickup>();
		PickupLeft->SetWeaponDefinition(WeaponDefA);
		PickupLeft->SetActorLocation(FVector(0.0f, -100.0f, 0.0f));

		AWorldWeaponPickup* PickupRight = World->SpawnActor<AWorldWeaponPickup>();
		PickupRight->SetWeaponDefinition(WeaponDefB);
		PickupRight->SetActorLocation(FVector(0.0f, 100.0f, 0.0f));

		// Player near Left
		Player->SetActorLocation(FVector(0.0f, -50.0f, 0.0f));
		Player->RegisterWorldPickupCandidate(PickupLeft);
		Player->RegisterWorldPickupCandidate(PickupRight);

		TestEqual(TEXT("PickupLeft is closest candidate"), Player->GetCurrentWorldPickupCandidate(), PickupLeft);

		// Movement update without location change: should not change
		Player->HandleCharacterMovementUpdated(0.016f, Player->GetActorLocation(), FVector::ZeroVector);
		TestEqual(TEXT("Candidate unchanged when position unchanged"), Player->GetCurrentWorldPickupCandidate(), PickupLeft);

		// Move player closer to Right
		const FVector OldPos = Player->GetActorLocation();
		Player->SetActorLocation(FVector(0.0f, 80.0f, 0.0f));
		Player->HandleCharacterMovementUpdated(0.016f, OldPos, FVector(0.0f, 100.0f, 0.0f));

		TestEqual(TEXT("PickupRight becomes candidate after moving closer"), Player->GetCurrentWorldPickupCandidate(), PickupRight);

		// Input snapshot integrity: if candidate becomes invalid before interaction
		PickupRight->SetInteractionEnabled(false);
		// Trigger E interaction directly via HandleInteractStarted test seam
		Player->TriggerTestHandleInteractStarted();
		TestEqual(TEXT("PickupLeft becomes candidate after right disabled and E pressed"), Player->GetCurrentWorldPickupCandidate(), PickupLeft);

		PickupLeft->Destroy();
		PickupRight->Destroy();
		Player->ClearWorldPickupInteractionState();
	}

	// -------------------------------------------------------------------------
	// SECTION 7: UnPossess, Re-Possess and Teardown Cleanup
	// -------------------------------------------------------------------------
	{
		PC->UnPossess();
		UWorldInteractionPromptWidget* PromptInstance = PC->GetTestInteractionPromptInstance();
		if (PromptInstance)
		{
			TestEqual(TEXT("Prompt hidden on UnPossess"), PromptInstance->GetVisibility(), ESlateVisibility::Collapsed);
		}
		TestEqual(TEXT("Player interaction state cleared on UnPossess"), Player->GetTestWorldPickupCandidateCount(), 0);

		// Re-Possess and verify movement delegate restoration
		PC->Possess(Player);
		UMeleeWeaponDefinition* WeaponDefRepossess = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		WeaponDefRepossess->InteractionDisplayName = FText::FromString(TEXT("重连武器"));

		AWorldWeaponPickup* PickupRepossess = World->SpawnActor<AWorldWeaponPickup>();
		PickupRepossess->SetWeaponDefinition(WeaponDefRepossess);
		PickupRepossess->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
		PickupRepossess->AddTestOverlappingPlayer(Player);

		const FVector OldLocation = Player->GetActorLocation();
		Player->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		Player->HandleCharacterMovementUpdated(0.016f, OldLocation, FVector(50.0f, 0.0f, 0.0f));
		TestEqual(TEXT("Pickup remains candidate after re-possess and movement update"), Player->GetCurrentWorldPickupCandidate(), PickupRepossess);

		PickupRepossess->Destroy();
		Player->ClearWorldPickupInteractionState();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
