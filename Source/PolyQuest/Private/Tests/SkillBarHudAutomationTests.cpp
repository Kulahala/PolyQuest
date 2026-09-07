// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestPreparedSkillCooldownFixtures.h"
#include "UI/PlayerSkillBarHUDWidget.h"
#include "UI/PlayerSkillSlotWidget.h"

namespace
{
	void TickSkillBarTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	void AdvanceSkillBarTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.1f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickSkillBarTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}

	UMeleeWeaponDefinition* CreateTestSkillWeapon(UObject* Outer, TSubclassOf<UGameplayAbility> PreparedSkillClass)
	{
		UMeleeWeaponDefinition* Def = NewObject<UMeleeWeaponDefinition>(Outer, NAME_None);
		Def->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		Def->LocomotionMode = EWeaponLocomotionMode::Default;
		Def->AttachSocketName = TEXT("Weapon_R");
		Def->WeaponMesh = nullptr;
		Def->bUseOwnerMeshSocketForTrace = true;
		Def->BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 5.0f);
		Def->BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 20.0f);
		Def->TraceRadius = 8.0f;
		Def->BladeSubdivisions = 2;
		Def->DisplayScale = FVector(1.0f);
		Def->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		Def->PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack.Primary"));
		if (PreparedSkillClass)
		{
			Def->ExclusiveCombatActions.Add(PreparedSkillClass);
			Def->DefaultPreparedActions.Add(PreparedSkillClass);
		}
		return Def;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillBarHudAutomationTest, "PolyQuest.UI.SkillBarHUD", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillBarHudAutomationTest::RunTest(const FString&)
{
	// -------------------------------------------------------------------------
	// SECTION 1: HeadlessDefense
	// No BindWidget, null weak references, unbind and simulate tick without crash
	// -------------------------------------------------------------------------
	{
		// 1.1 Headless Slot Widget
		UPlayerSkillSlotWidget* HeadlessSlot = NewObject<UPlayerSkillSlotWidget>(GetTransientPackage());
		TestNotNull(TEXT("Headless PlayerSkillSlotWidget created successfully"), HeadlessSlot);
		if (HeadlessSlot)
		{
			HeadlessSlot->InitializeSlot(0);
			TestEqual(TEXT("Headless slot records slot index 0"), HeadlessSlot->GetSlotIndex(), 0);

			HeadlessSlot->UpdateSlotState(EPlayerSkillSlotDisplayState::Ready, 0.0f);
			TestEqual(TEXT("Headless slot records Ready state"), HeadlessSlot->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);

			HeadlessSlot->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, 0.5f);
			TestEqual(TEXT("Headless slot records Cooldown state"), HeadlessSlot->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Cooldown);
			TestEqual(TEXT("Headless slot records cooldown percent 0.5"), HeadlessSlot->GetCurrentCooldownPercent(), 0.5f);

			HeadlessSlot->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			TestEqual(TEXT("Headless slot records Invalid state"), HeadlessSlot->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Invalid);
		}

		// 1.2 Headless Bar HUD Widget
		UPlayerSkillBarHUDWidget* HeadlessBar = NewObject<UPlayerSkillBarHUDWidget>(GetTransientPackage());
		TestNotNull(TEXT("Headless PlayerSkillBarHUDWidget created successfully"), HeadlessBar);
		if (HeadlessBar)
		{
			HeadlessBar->BindToEquipmentAndASC(nullptr, nullptr);
			TestNull(TEXT("Headless bar has null EquipmentComponent"), HeadlessBar->GetBoundEquipmentComponent());
			TestNull(TEXT("Headless bar has null ASC"), HeadlessBar->GetBoundAbilitySystemComponent());

			HeadlessBar->RefreshAllSlots();
			HeadlessBar->SimulateTickForTesting(0.1f);
			HeadlessBar->Unbind();

			TestNull(TEXT("Headless bar slot 0 is safely null"), HeadlessBar->GetTestSlot(0));
			TestNull(TEXT("Headless bar slot 3 is safely null"), HeadlessBar->GetTestSlot(3));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: SlotStateTransitions
	// Four-state machine transitions and visual component visibility assertions
	// -------------------------------------------------------------------------
	{
		UPlayerSkillSlotWidget* SlotWidget = NewObject<UPlayerSkillSlotWidget>(GetTransientPackage());
		TestNotNull(TEXT("SlotWidget created for visual state assertions"), SlotWidget);

		UImage* Background = NewObject<UImage>(SlotWidget);
		UTextBlock* SlotNumText = NewObject<UTextBlock>(SlotWidget);
		UBorder* CooldownOverlay = NewObject<UBorder>(SlotWidget);
		UImage* CooldownSweep = NewObject<UImage>(SlotWidget);

		SlotWidget->SetTestControls(Background, SlotNumText, CooldownOverlay, CooldownSweep);
		SlotWidget->InitializeSlot(1);
		TestEqual(TEXT("SlotNumberText shows '2' for index 1"), SlotNumText->GetText().ToString(), TEXT("2"));

		// 2.1 Empty State
		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Empty, 0.0f);
		TestEqual(TEXT("Slot state is Empty"), SlotWidget->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);
		TestFalse(TEXT("Empty state: CooldownOverlay is not visible"), SlotWidget->IsTestOverlayVisible());
		TestFalse(TEXT("Empty state: CooldownSweep is not visible"), SlotWidget->IsTestSweepVisible());

		// 2.2 Ready State
		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Ready, 0.0f);
		TestEqual(TEXT("Slot state is Ready"), SlotWidget->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);
		TestFalse(TEXT("Ready state: CooldownOverlay is not visible"), SlotWidget->IsTestOverlayVisible());
		TestFalse(TEXT("Ready state: CooldownSweep is not visible"), SlotWidget->IsTestSweepVisible());

		// 2.3 Cooldown State
		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, 0.6f);
		TestEqual(TEXT("Slot state is Cooldown"), SlotWidget->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Cooldown);
		TestTrue(TEXT("Cooldown state: CooldownOverlay is visible"), SlotWidget->IsTestOverlayVisible());
		TestEqual(TEXT("Cooldown state: Cooldown percent is 0.6"), SlotWidget->GetCurrentCooldownPercent(), 0.6f);

		// 2.4 Invalid State
		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
		TestEqual(TEXT("Slot state is Invalid"), SlotWidget->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Invalid);
		TestTrue(TEXT("Invalid state: CooldownOverlay is visible"), SlotWidget->IsTestOverlayVisible());
		TestFalse(TEXT("Invalid state: CooldownSweep is not visible"), SlotWidget->IsTestSweepVisible());
	}

	// -------------------------------------------------------------------------
	// SECTION 3: InvalidAndFiniteInputDefense
	// NaN, Infinity, negative duration/remaining, zero duration defense
	// -------------------------------------------------------------------------
	{
		UPlayerSkillSlotWidget* SlotWidget = NewObject<UPlayerSkillSlotWidget>(GetTransientPackage());
		UImage* Background = NewObject<UImage>(SlotWidget);
		UTextBlock* SlotNumText = NewObject<UTextBlock>(SlotWidget);
		UBorder* CooldownOverlay = NewObject<UBorder>(SlotWidget);
		UImage* CooldownSweep = NewObject<UImage>(SlotWidget);
		SlotWidget->SetTestControls(Background, SlotNumText, CooldownOverlay, CooldownSweep);

		constexpr float TestNaN = std::numeric_limits<float>::quiet_NaN();
		constexpr float TestInf = std::numeric_limits<float>::infinity();

		// NaN / Inf inputs fallback to 0.0
		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, TestNaN);
		TestEqual(TEXT("NaN cooldown percent falls back to 0.0"), SlotWidget->GetCurrentCooldownPercent(), 0.0f);

		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, TestInf);
		TestEqual(TEXT("Inf cooldown percent falls back to 0.0"), SlotWidget->GetCurrentCooldownPercent(), 0.0f);

		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, -0.5f);
		TestEqual(TEXT("Negative cooldown percent clamped to 0.0"), SlotWidget->GetCurrentCooldownPercent(), 0.0f);

		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, 1.5f);
		TestEqual(TEXT("Over-max cooldown percent clamped to 1.0"), SlotWidget->GetCurrentCooldownPercent(), 1.0f);

		SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, 0.72f);
		TestEqual(TEXT("Valid cooldown percent sets exact value 0.72"), SlotWidget->GetCurrentCooldownPercent(), 0.72f);
	}

	// -------------------------------------------------------------------------
	// Setup Test World for Runtime GAS & Equipment Tests (Sections 4 - 8)
	// -------------------------------------------------------------------------
	if (!TestNotNull(TEXT("GEngine is available"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SkillBarHudTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	const FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

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

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
	TestNotNull(TEXT("Player spawned in test world"), Player);
	if (!Player)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
	TestNotNull(TEXT("Player ASC is valid"), ASC);
	TestNotNull(TEXT("Player WeaponEquipmentComponent is valid"), EquipComp);
	if (!ASC || !EquipComp)
	{
		return false;
	}

	// Create and inject 4 test slots into BarWidget
	UPlayerSkillBarHUDWidget* BarWidget = NewObject<UPlayerSkillBarHUDWidget>(World);
	UPlayerSkillSlotWidget* S0 = NewObject<UPlayerSkillSlotWidget>(BarWidget);
	UPlayerSkillSlotWidget* S1 = NewObject<UPlayerSkillSlotWidget>(BarWidget);
	UPlayerSkillSlotWidget* S2 = NewObject<UPlayerSkillSlotWidget>(BarWidget);
	UPlayerSkillSlotWidget* S3 = NewObject<UPlayerSkillSlotWidget>(BarWidget);

	S0->SetTestControls(NewObject<UImage>(S0), NewObject<UTextBlock>(S0), NewObject<UBorder>(S0), NewObject<UImage>(S0));
	S1->SetTestControls(NewObject<UImage>(S1), NewObject<UTextBlock>(S1), NewObject<UBorder>(S1), NewObject<UImage>(S1));
	S2->SetTestControls(NewObject<UImage>(S2), NewObject<UTextBlock>(S2), NewObject<UBorder>(S2), NewObject<UImage>(S2));
	S3->SetTestControls(NewObject<UImage>(S3), NewObject<UTextBlock>(S3), NewObject<UBorder>(S3), NewObject<UImage>(S3));

	BarWidget->SetTestSlots(S0, S1, S2, S3);
	BarWidget->BindToEquipmentAndASC(EquipComp, ASC);

	// -------------------------------------------------------------------------
	// SECTION 4: CooldownQueryStartRemainingExpiry
	// 4.0s CD start (1.0) -> mid-ratio (0.5) -> expiration (Ready, 0.0)
	// -------------------------------------------------------------------------
	{
		UMeleeWeaponDefinition* SkillWeapon = CreateTestSkillWeapon(GetTransientPackage(), UTestPreparedSkillCooldownAbility::StaticClass());
		const bool bEquipped = EquipComp->EquipWeapon(SkillWeapon);
		TestTrue(TEXT("SkillWeapon equipped successfully"), bEquipped);

		// Before activation: Slot 0 is Ready, Slots 1-3 are Empty
		TestEqual(TEXT("Slot 0 is Ready after equip"), BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);
		TestEqual(TEXT("Slot 0 initial percent is 0.0"), BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent(), 0.0f);
		TestEqual(TEXT("Slot 1 is Empty"), BarWidget->GetTestSlot(1)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);
		TestEqual(TEXT("Slot 2 is Empty"), BarWidget->GetTestSlot(2)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);
		TestEqual(TEXT("Slot 3 is Empty"), BarWidget->GetTestSlot(3)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);

		// Activate Slot 0: commits 4.0s Cooldown GE
		const bool bActivated = EquipComp->TryActivatePreparedSlot(0);
		TestTrue(TEXT("TryActivatePreparedSlot(0) succeeded"), bActivated);

		BarWidget->RefreshAllSlots();
		TestEqual(TEXT("Slot 0 enters Cooldown state on activation"), BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Cooldown);
		TestTrue(TEXT("Slot 0 cooldown percent starts near 1.0"), BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent() >= 0.95f);

		// Advance 2.0s (midpoint of 4.0s duration)
		AdvanceSkillBarTimer(World, 2.0f);
		BarWidget->RefreshAllSlots();
		TestEqual(TEXT("Slot 0 remains in Cooldown state at midpoint"), BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Cooldown);
		TestTrue(TEXT("Slot 0 cooldown percent is near 0.5 at 2.0s"),
			BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent() >= 0.40f && BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent() <= 0.60f);

		// Advance another 2.5s (total 4.5s > 4.0s duration): cooldown expired
		AdvanceSkillBarTimer(World, 2.5f);
		BarWidget->RefreshAllSlots();
		TestEqual(TEXT("Slot 0 recovers to Ready after cooldown expires"), BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);
		TestEqual(TEXT("Slot 0 cooldown percent resets to 0.0"), BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent(), 0.0f);
	}

	// -------------------------------------------------------------------------
	// SECTION 5: ActorInfoAndPendingRemoveDefense
	// Missing ActorInfo or PendingRemove specs fail-closed safely
	// -------------------------------------------------------------------------
	{
		// 5.1 Missing ActorInfo defense: clear actor info on ASC
		ASC->ClearActorInfo();
		BarWidget->RefreshAllSlots();

		// Configured Slot 0 safely degrades to Invalid when ActorInfo is missing
		TestEqual(TEXT("Configured Slot 0 safely degrades to Invalid with missing ActorInfo"),
			BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Invalid);
		TestEqual(TEXT("Configured Slot 0 percent is 0.0 with missing ActorInfo"),
			BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent(), 0.0f);

		// Unconfigured Slots 1-3 remain Empty
		for (int32 SlotIdx = 1; SlotIdx < 4; ++SlotIdx)
		{
			TestEqual(FString::Printf(TEXT("Unconfigured Slot %d remains Empty with missing ActorInfo"), SlotIdx),
				BarWidget->GetTestSlot(SlotIdx)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);
			TestEqual(FString::Printf(TEXT("Unconfigured Slot %d percent is 0.0"), SlotIdx),
				BarWidget->GetTestSlot(SlotIdx)->GetCurrentCooldownPercent(), 0.0f);
		}

		// Restore ActorInfo on ASC
		ASC->InitAbilityActorInfo(Player, Player);
		BarWidget->RefreshAllSlots();
		TestEqual(TEXT("Slot 0 restored to Ready after valid ActorInfo restore"), BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);

		// 5.2 PendingRemove spec defense
		TSubclassOf<UGameplayAbility> SlotClass;
		FGameplayAbilitySpecHandle SlotHandle;
			const bool bFoundBinding = EquipComp->TryGetPreparedSlotBinding(0, SlotClass, SlotHandle);
			TestTrue(TEXT("Found binding for Slot 0"), bFoundBinding);
			TestFalse(TEXT("Configured Slot 0 is not structurally empty"), EquipComp->IsPreparedSlotEmpty(0));
			TestTrue(TEXT("Unconfigured Slot 1 is structurally empty"), EquipComp->IsPreparedSlotEmpty(1));

		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(SlotHandle);
		TestNotNull(TEXT("Slot 0 Spec found on ASC"), Spec);
		if (Spec)
		{
			Spec->PendingRemove = true;

			TSubclassOf<UGameplayAbility> DeadClass;
			FGameplayAbilitySpecHandle DeadHandle;
			const bool bBindingWithPendingRemove = EquipComp->TryGetPreparedSlotBinding(0, DeadClass, DeadHandle);
			TestFalse(TEXT("TryGetPreparedSlotBinding returns false when spec has PendingRemove=true"), bBindingWithPendingRemove);

				BarWidget->RefreshAllSlots();
				TestEqual(TEXT("Slot 0 safely degrades to Invalid when spec is PendingRemove"),
					BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Invalid);

			// Restore PendingRemove
			Spec->PendingRemove = false;
			BarWidget->RefreshAllSlots();
			TestEqual(TEXT("Slot 0 recovers to Ready when PendingRemove is cleared"),
				BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: EquipmentTransactionAndLifecycle
	// Commit broadcasts once, rollback does not broadcast, clean teardown
	// -------------------------------------------------------------------------
	{
		int32 BroadcastCount = 0;
		FDelegateHandle BroadcastHandle = EquipComp->OnPreparedSlotsChanged().AddLambda([&BroadcastCount]()
		{
			++BroadcastCount;
		});

		// 6.1 Successful equip broadcasts exactly once
		UMeleeWeaponDefinition* AnotherSkillWeapon = CreateTestSkillWeapon(GetTransientPackage(), UTestPreparedSkillCooldownAbility::StaticClass());
		const bool bEquipSuccess = EquipComp->EquipWeapon(AnotherSkillWeapon);
		TestTrue(TEXT("AnotherSkillWeapon equipped successfully"), bEquipSuccess);
		TestEqual(TEXT("Successful equip broadcasts OnPreparedSlotsChanged exactly once"), BroadcastCount, 1);

		// 6.2 Failed equip (null definition) does NOT broadcast (flicker defense)
		const bool bEquipFailed = EquipComp->EquipWeapon(nullptr);
		TestFalse(TEXT("EquipWeapon(nullptr) correctly rejected"), bEquipFailed);
		TestEqual(TEXT("Failed equip does NOT broadcast OnPreparedSlotsChanged"), BroadcastCount, 1);

		// 6.3 Cleanup delegate
		EquipComp->OnPreparedSlotsChanged().Remove(BroadcastHandle);
	}

	// -------------------------------------------------------------------------
	// SECTION 7: AbilityCancelCooldownRetention
	// Cancelling an active ability does NOT clear active Cooldown GE from ASC
	// -------------------------------------------------------------------------
	{
		TSubclassOf<UGameplayAbility> SlotClass;
		FGameplayAbilitySpecHandle SlotHandle;
		const bool bFoundSlot = EquipComp->TryGetPreparedSlotBinding(0, SlotClass, SlotHandle);
		TestTrue(TEXT("Found binding for Slot 0 before cancel test"), bFoundSlot);

		UTestPreparedSkillCooldownAbility* AbilityCDO = SlotClass ? Cast<UTestPreparedSkillCooldownAbility>(SlotClass->GetDefaultObject()) : nullptr;
		if (AbilityCDO)
		{
			AbilityCDO->bAutoEndAbility = false;
		}

		// Activate Slot 0 while non-auto-ending to apply 4.0s cooldown and remain active
		const bool bActivated = EquipComp->TryActivatePreparedSlot(0);
		TestTrue(TEXT("Slot 0 activated for cancel retention test"), bActivated);

		// Cancel the active ability handle on ASC
		ASC->CancelAbilityHandle(SlotHandle);

		if (AbilityCDO)
		{
			AbilityCDO->bAutoEndAbility = true;
		}

		// Cooldown GE remains active on ASC
		BarWidget->RefreshAllSlots();
		TestEqual(TEXT("HUD retains Cooldown state after ability cancellation"),
			BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Cooldown);
		TestTrue(TEXT("HUD cooldown percent remains active after cancellation"),
			BarWidget->GetTestSlot(0)->GetCurrentCooldownPercent() > 0.0f);

		// Advance world past 4.0s cooldown duration
		AdvanceSkillBarTimer(World, 4.5f);
		BarWidget->RefreshAllSlots();
		TestEqual(TEXT("HUD returns to Ready once retained cooldown duration elapses"),
			BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);
	}

	// -------------------------------------------------------------------------
	// SECTION 8: DeadAndPawnLifecycle
	// Dead tag triggers Invalid on configured slots; PC handles pawn repossess
	// -------------------------------------------------------------------------
	{
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Status.Dead"));
		TestTrue(TEXT("State.Status.Dead tag is registered"), DeadTag.IsValid());

		// 8.1 Adding Dead tag switches configured slots to Invalid, empty slots stay Empty
		ASC->AddLooseGameplayTag(DeadTag);
		TestEqual(TEXT("Configured Slot 0 becomes Invalid on death"),
			BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Invalid);
		TestEqual(TEXT("Unconfigured Slot 1 remains Empty on death"),
			BarWidget->GetTestSlot(1)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);
		TestEqual(TEXT("Unconfigured Slot 2 remains Empty on death"),
			BarWidget->GetTestSlot(2)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);
		TestEqual(TEXT("Unconfigured Slot 3 remains Empty on death"),
			BarWidget->GetTestSlot(3)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);

		// 8.2 Removing Dead tag restores configured slot to Ready
		ASC->RemoveLooseGameplayTag(DeadTag);
		TestEqual(TEXT("Configured Slot 0 restores to Ready when revived"),
			BarWidget->GetTestSlot(0)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Ready);
		TestEqual(TEXT("Slot 1 remains Empty when revived"),
			BarWidget->GetTestSlot(1)->GetCurrentDisplayState(), EPlayerSkillSlotDisplayState::Empty);

		// 8.3 PlayerController Lifecycle, Repossess & Teardown
		APolyQuestPlayerController* PC = World->SpawnActor<APolyQuestPlayerController>();
		TestNotNull(TEXT("PC spawned successfully"), PC);
		if (PC)
		{
			PC->DispatchBeginPlay();
			PC->SetTestSkillBarHUDClass(UPlayerSkillBarHUDWidget::StaticClass());

			PC->TriggerTestEnsureSkillBarHUDCreated();
			UPlayerSkillBarHUDWidget* PCBar = PC->GetTestSkillBarHUDInstance();
			TestNotNull(TEXT("PC creates SkillBarHUDInstance"), PCBar);

			// Idempotency check: repeated EnsureSkillBarHUDCreated does not recreate
			PC->TriggerTestEnsureSkillBarHUDCreated();
			TestEqual(TEXT("Repeated EnsureSkillBarHUDCreated preserves existing instance"),
				PC->GetTestSkillBarHUDInstance(), PCBar);

			// Bind to Pawn 1
			PC->TriggerTestBindToPawn(Player);
			TestEqual(TEXT("PCBar bound to Player EquipComp"), PCBar->GetBoundEquipmentComponent(), EquipComp);
			TestEqual(TEXT("PCBar bound to Player ASC"), PCBar->GetBoundAbilitySystemComponent(), ASC);

			// Spawn Pawn 2
			APlayerCharacter* Player2 = FCombatAutomationFixture::SpawnPlayer(World);
			TestNotNull(TEXT("Player 2 spawned in test world"), Player2);
			if (Player2)
			{
				UWeaponEquipmentComponent* EquipComp2 = Player2->FindComponentByClass<UWeaponEquipmentComponent>();
				UAbilitySystemComponent* ASC2 = Player2->GetAbilitySystemComponent();

				// Unbind Pawn 1
				PC->TriggerTestUnbindCurrentPawn();
				TestNull(TEXT("PCBar unbinds EquipComp on UnbindCurrentPawn"), PCBar->GetBoundEquipmentComponent());
				TestNull(TEXT("PCBar unbinds ASC on UnbindCurrentPawn"), PCBar->GetBoundAbilitySystemComponent());

				// Bind to Pawn 2
				PC->TriggerTestBindToPawn(Player2);
				TestEqual(TEXT("PCBar rebinds to Player 2 EquipComp"), PCBar->GetBoundEquipmentComponent(), EquipComp2);
				TestEqual(TEXT("PCBar rebinds to Player 2 ASC"), PCBar->GetBoundAbilitySystemComponent(), ASC2);

				Player2->Destroy();
			}

			PC->Destroy();
		}
	}

	Player->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
