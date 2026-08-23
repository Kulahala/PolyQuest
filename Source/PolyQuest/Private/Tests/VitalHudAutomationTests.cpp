// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "UI/EnemyHealthBarWidget.h"
#include "UI/PlayerVitalHUDWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVitalHudAutomationTest, "PolyQuest.UI.VitalHUD", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVitalHudAutomationTest::RunTest(const FString&)
{
	// -------------------------------------------------------------------------
	// SECTION 1: Widget Safe Setters, Visual Assertions & Clamp Math
	// -------------------------------------------------------------------------
	{
		// 1.1 Headless Null-BindWidget Defense
		{
			UPlayerVitalHUDWidget* HeadlessWidget = NewObject<UPlayerVitalHUDWidget>(GetTransientPackage());
			TestNotNull(TEXT("Headless PlayerWidget created via NewObject"), HeadlessWidget);
			if (HeadlessWidget)
			{
				HeadlessWidget->SetHealth(50.0f, 100.0f);
				HeadlessWidget->SetStamina(30.0f, 100.0f);
			}

			UEnemyHealthBarWidget* HeadlessEnemyWidget = NewObject<UEnemyHealthBarWidget>(GetTransientPackage());
			TestNotNull(TEXT("Headless EnemyWidget created via NewObject"), HeadlessEnemyWidget);
			if (HeadlessEnemyWidget)
			{
				HeadlessEnemyWidget->SetHealth(75.0f, 100.0f);
			}
		}

		// 1.2 PlayerVitalHUDWidget Visual Output Assertions with Injected Transient Controls
		{
			UPlayerVitalHUDWidget* PlayerWidget = NewObject<UPlayerVitalHUDWidget>(GetTransientPackage());
			UProgressBar* HPBar = NewObject<UProgressBar>(PlayerWidget);
			UTextBlock* HPCurr = NewObject<UTextBlock>(PlayerWidget);
			UTextBlock* HPMax = NewObject<UTextBlock>(PlayerWidget);
			UProgressBar* SPBar = NewObject<UProgressBar>(PlayerWidget);
			UTextBlock* SPCurr = NewObject<UTextBlock>(PlayerWidget);
			UTextBlock* SPMax = NewObject<UTextBlock>(PlayerWidget);

			PlayerWidget->SetTestHealthWidgets(HPBar, HPCurr, HPMax);
			PlayerWidget->SetTestStaminaWidgets(SPBar, SPCurr, SPMax);

			// Normal range
			PlayerWidget->SetHealth(50.0f, 100.0f);
			TestEqual(TEXT("Player normal health percent is 0.5"), HPBar->GetPercent(), 0.5f);
			TestEqual(TEXT("Player normal health current text is '50'"), HPCurr->GetText().ToString(), TEXT("50"));
			TestEqual(TEXT("Player normal health max text is '100'"), HPMax->GetText().ToString(), TEXT("100"));

			PlayerWidget->SetStamina(30.0f, 100.0f);
			TestEqual(TEXT("Player normal stamina percent is 0.3"), SPBar->GetPercent(), 0.3f);
			TestEqual(TEXT("Player normal stamina current text is '30'"), SPCurr->GetText().ToString(), TEXT("30"));
			TestEqual(TEXT("Player normal stamina max text is '100'"), SPMax->GetText().ToString(), TEXT("100"));

			// Over-max clamping
			PlayerWidget->SetHealth(150.0f, 100.0f);
			TestEqual(TEXT("Player over-max health percent is clamped to 1.0"), HPBar->GetPercent(), 1.0f);
			TestEqual(TEXT("Player over-max health current text is clamped to '100'"), HPCurr->GetText().ToString(), TEXT("100"));
			TestEqual(TEXT("Player over-max health max text is '100'"), HPMax->GetText().ToString(), TEXT("100"));

			PlayerWidget->SetStamina(200.0f, 100.0f);
			TestEqual(TEXT("Player over-max stamina percent is clamped to 1.0"), SPBar->GetPercent(), 1.0f);
			TestEqual(TEXT("Player over-max stamina current text is clamped to '100'"), SPCurr->GetText().ToString(), TEXT("100"));

			// Negative current clamping
			PlayerWidget->SetHealth(-20.0f, 100.0f);
			TestEqual(TEXT("Player negative health percent is clamped to 0.0"), HPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player negative health current text is clamped to '0'"), HPCurr->GetText().ToString(), TEXT("0"));
			TestEqual(TEXT("Player negative health max text is '100'"), HPMax->GetText().ToString(), TEXT("100"));

			PlayerWidget->SetStamina(-50.0f, 100.0f);
			TestEqual(TEXT("Player negative stamina percent is clamped to 0.0"), SPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player negative stamina current text is clamped to '0'"), SPCurr->GetText().ToString(), TEXT("0"));

			// Zero or negative max fallback
			PlayerWidget->SetHealth(50.0f, 0.0f);
			TestEqual(TEXT("Player zero max health percent falls back to 0.0"), HPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player zero max health current text falls back to '0'"), HPCurr->GetText().ToString(), TEXT("0"));
			TestEqual(TEXT("Player zero max health max text falls back to '0'"), HPMax->GetText().ToString(), TEXT("0"));

			PlayerWidget->SetHealth(50.0f, -100.0f);
			TestEqual(TEXT("Player negative max health percent falls back to 0.0"), HPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player negative max health current text falls back to '0'"), HPCurr->GetText().ToString(), TEXT("0"));

			PlayerWidget->SetStamina(50.0f, 0.0f);
			TestEqual(TEXT("Player zero max stamina percent falls back to 0.0"), SPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player zero max stamina current text falls back to '0'"), SPCurr->GetText().ToString(), TEXT("0"));

			PlayerWidget->SetStamina(50.0f, -100.0f);
			TestEqual(TEXT("Player negative max stamina percent falls back to 0.0"), SPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player negative max stamina current text falls back to '0'"), SPCurr->GetText().ToString(), TEXT("0"));

			// Non-finite values defense (NaN / Inf)
			constexpr float TestNaN = std::numeric_limits<float>::quiet_NaN();
			constexpr float TestInf = std::numeric_limits<float>::infinity();

			PlayerWidget->SetHealth(TestNaN, 100.0f);
			TestEqual(TEXT("Player NaN health percent falls back to 0.0"), HPBar->GetPercent(), 0.0f);
			TestEqual(TEXT("Player NaN health current text falls back to '0'"), HPCurr->GetText().ToString(), TEXT("0"));

			PlayerWidget->SetHealth(50.0f, TestNaN);
			TestEqual(TEXT("Player NaN max health percent falls back to 0.0"), HPBar->GetPercent(), 0.0f);

			PlayerWidget->SetHealth(TestInf, 100.0f);
			TestEqual(TEXT("Player Inf health percent falls back to 0.0"), HPBar->GetPercent(), 0.0f);

			PlayerWidget->SetHealth(50.0f, TestInf);
			TestEqual(TEXT("Player Inf max health percent falls back to 0.0"), HPBar->GetPercent(), 0.0f);

			PlayerWidget->SetStamina(TestNaN, 100.0f);
			TestEqual(TEXT("Player NaN stamina percent falls back to 0.0"), SPBar->GetPercent(), 0.0f);

			PlayerWidget->SetStamina(50.0f, TestInf);
			TestEqual(TEXT("Player Inf max stamina percent falls back to 0.0"), SPBar->GetPercent(), 0.0f);
		}

		// 1.3 EnemyHealthBarWidget Visual Output Assertions
		{
			UEnemyHealthBarWidget* EnemyWidget = NewObject<UEnemyHealthBarWidget>(GetTransientPackage());
			UProgressBar* EnemyHPBar = NewObject<UProgressBar>(EnemyWidget);
			EnemyWidget->SetTestHealthProgressBar(EnemyHPBar);

			EnemyWidget->SetHealth(75.0f, 100.0f);
			TestEqual(TEXT("Enemy normal health percent is 0.75"), EnemyHPBar->GetPercent(), 0.75f);

			EnemyWidget->SetHealth(150.0f, 100.0f);
			TestEqual(TEXT("Enemy over-max health percent is clamped to 1.0"), EnemyHPBar->GetPercent(), 1.0f);

			EnemyWidget->SetHealth(-10.0f, 100.0f);
			TestEqual(TEXT("Enemy negative health percent is clamped to 0.0"), EnemyHPBar->GetPercent(), 0.0f);

			EnemyWidget->SetHealth(50.0f, 0.0f);
			TestEqual(TEXT("Enemy zero max health percent falls back to 0.0"), EnemyHPBar->GetPercent(), 0.0f);

			EnemyWidget->SetHealth(50.0f, -50.0f);
			TestEqual(TEXT("Enemy negative max health percent falls back to 0.0"), EnemyHPBar->GetPercent(), 0.0f);

			constexpr float TestNaN = std::numeric_limits<float>::quiet_NaN();
			constexpr float TestInf = std::numeric_limits<float>::infinity();

			EnemyWidget->SetHealth(TestNaN, 100.0f);
			TestEqual(TEXT("Enemy NaN health percent falls back to 0.0"), EnemyHPBar->GetPercent(), 0.0f);

			EnemyWidget->SetHealth(50.0f, TestInf);
			TestEqual(TEXT("Enemy Inf max health percent falls back to 0.0"), EnemyHPBar->GetPercent(), 0.0f);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Enemy CDO WidgetComponent Defaults
	// -------------------------------------------------------------------------
	{
		const AEnemyCharacter* EnemyCDO = AEnemyCharacter::StaticClass()->GetDefaultObject<AEnemyCharacter>();
		TestNotNull(TEXT("AEnemyCharacter CDO exists"), EnemyCDO);

		if (EnemyCDO)
		{
			const UWidgetComponent* WidgetComp = EnemyCDO->GetTestHealthBarWidgetComponent();
			TestNotNull(TEXT("EnemyHealthBarWidgetComponent exists on CDO"), WidgetComp);

			if (WidgetComp)
			{
				TestEqual(TEXT("Enemy WidgetComponent space is Screen"),
					WidgetComp->GetWidgetSpace(), EWidgetSpace::Screen);
				TestEqual(TEXT("Enemy WidgetComponent collision is NoCollision"),
					WidgetComp->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
				TestFalse(TEXT("Enemy WidgetComponent overlap generation is disabled"),
					WidgetComp->GetGenerateOverlapEvents());
				TestEqual(TEXT("Enemy WidgetComponent pivot is (0.5, 1.0)"),
					WidgetComp->GetPivot(), FVector2D(0.5, 1.0));
				TestEqual(TEXT("Enemy WidgetComponent relative Z is 130.0"),
					WidgetComp->GetRelativeLocation().Z, 130.0);
				TestEqual(TEXT("Enemy WidgetComponent draw size is 160x20"),
					WidgetComp->GetDrawSize(), FVector2D(160.0, 20.0));
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: Transient World Setup & Enemy UI Lifecycle
	// -------------------------------------------------------------------------
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("VitalHudTestWorld"));
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

	// 3.1 Enemy UI binding, visual update and unbinding
	{
		AEnemyCharacter* Enemy = World->SpawnActor<AEnemyCharacter>();
		TestNotNull(TEXT("Enemy character spawned in test world"), Enemy);

		if (Enemy)
		{
			Enemy->DispatchBeginPlay();

			// Headless environment: GetUserWidgetObject() is null, weak reference is safely null
			TestNull(TEXT("Headless enemy widget instance is safely null"), Enemy->GetTestHealthBarWidget());
			TestTrue(TEXT("Enemy bound UI health attribute delegates"), Enemy->HasBoundUIHealthDelegates());

			// Inject test widget with transient progress bar
			UEnemyHealthBarWidget* TestEnemyWidget = NewObject<UEnemyHealthBarWidget>(Enemy);
			UProgressBar* EnemyBar = NewObject<UProgressBar>(TestEnemyWidget);
			TestEnemyWidget->SetTestHealthProgressBar(EnemyBar);
			Enemy->SetTestHealthBarWidget(TestEnemyWidget);
			TestEqual(TEXT("Enemy holds test widget reference"), Enemy->GetTestHealthBarWidget(), TestEnemyWidget);

			// Initial refresh (CDO health 100/100 -> percent 1.0)
			Enemy->TriggerTestRefreshEnemyHealthBar();
			TestEqual(TEXT("Enemy initial health bar percent is 1.0"), EnemyBar->GetPercent(), 1.0f);

			// Modify ASC attributes and verify delegate fires cleanly and updates progress bar
			if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
			{
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 120.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 75.0f);
				TestEqual(TEXT("Enemy health bar percent updated to 75/120 (0.625)"), EnemyBar->GetPercent(), 75.0f / 120.0f);
			}

			// Test clean unbinding and verify old-ASC change is ignored
			Enemy->TriggerTestUnbindUIHealthEvents();
			TestFalse(TEXT("Enemy UI delegates cleanly unbound"), Enemy->HasBoundUIHealthDelegates());

			if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
			{
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 40.0f);
				TestEqual(TEXT("Unbound enemy widget percent is NOT changed by attribute update"), EnemyBar->GetPercent(), 75.0f / 120.0f);
			}

			// Rebind and verify refresh
			Enemy->TriggerTestBindUIHealthEvents();
			TestTrue(TEXT("Enemy UI delegates rebound"), Enemy->HasBoundUIHealthDelegates());
			Enemy->TriggerTestRefreshEnemyHealthBar();
			TestEqual(TEXT("Rebound enemy widget percent reflects current health 40/120"), EnemyBar->GetPercent(), 40.0f / 120.0f);

			// Destroy enemy and verify teardown
			Enemy->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: PlayerController HUD Ownership, Re-Possess & Teardown
	// -------------------------------------------------------------------------
	{
		APolyQuestPlayerController* PC = World->SpawnActor<APolyQuestPlayerController>();
		TestNotNull(TEXT("PlayerController spawned in test world"), PC);
		if (PC)
		{
			PC->DispatchBeginPlay();
		}

		APlayerCharacter* PlayerPawn1 = World->SpawnActor<APlayerCharacter>();
		TestNotNull(TEXT("Player Pawn 1 spawned in test world"), PlayerPawn1);
		if (PlayerPawn1)
		{
			PlayerPawn1->DispatchBeginPlay();
		}

		APlayerCharacter* PlayerPawn2 = World->SpawnActor<APlayerCharacter>();
		TestNotNull(TEXT("Player Pawn 2 spawned in test world"), PlayerPawn2);
		if (PlayerPawn2)
		{
			PlayerPawn2->DispatchBeginPlay();
		}

		if (PC && PlayerPawn1 && PlayerPawn2)
		{
			PC->SetTestPlayerVitalHUDClass(UPlayerVitalHUDWidget::StaticClass());

			// EnsureHUDCreated idempotency
			PC->TriggerTestEnsureHUDCreated();
			UPlayerVitalHUDWidget* HUDInstance1 = PC->GetTestPlayerVitalHUDInstance();
			TestNotNull(TEXT("HUD instance created on first call"), HUDInstance1);

			PC->TriggerTestEnsureHUDCreated();
			UPlayerVitalHUDWidget* HUDInstance2 = PC->GetTestPlayerVitalHUDInstance();
			TestEqual(TEXT("Repeated EnsureHUDCreated reuses existing instance without duplication"), HUDInstance1, HUDInstance2);

			// Inject transient visual controls into HUD instance for observable output verification
			UProgressBar* HUD_HPBar = NewObject<UProgressBar>(HUDInstance1);
			UTextBlock* HUD_HPCurr = NewObject<UTextBlock>(HUDInstance1);
			UTextBlock* HUD_HPMax = NewObject<UTextBlock>(HUDInstance1);
			UProgressBar* HUD_SPBar = NewObject<UProgressBar>(HUDInstance1);
			UTextBlock* HUD_SPCurr = NewObject<UTextBlock>(HUDInstance1);
			UTextBlock* HUD_SPMax = NewObject<UTextBlock>(HUDInstance1);

			HUDInstance1->SetTestHealthWidgets(HUD_HPBar, HUD_HPCurr, HUD_HPMax);
			HUDInstance1->SetTestStaminaWidgets(HUD_SPBar, HUD_SPCurr, HUD_SPMax);

			// Bind to Pawn 1
			PC->TriggerTestBindToPawn(PlayerPawn1);
			TestEqual(TEXT("PC bound to PlayerPawn1 ASC"), PC->GetTestBoundAbilitySystemComponent(), PlayerPawn1->GetAbilitySystemComponent());
			TestTrue(TEXT("PC has all 4 bound attribute delegates for Pawn 1"), PC->HasBoundAttributeDelegates());

			// Attribute change on Pawn 1 triggers observable HUD refresh
			UAbilitySystemComponent* ASC1 = PlayerPawn1->GetAbilitySystemComponent();
			TestNotNull(TEXT("PlayerPawn1 ASC is valid"), ASC1);
			if (ASC1)
			{
				ASC1->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 80.0f);
				ASC1->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 60.0f);

				TestEqual(TEXT("HUD Health percent observed as 0.8"), HUD_HPBar->GetPercent(), 0.8f);
				TestEqual(TEXT("HUD Health current text observed as '80'"), HUD_HPCurr->GetText().ToString(), TEXT("80"));
				TestEqual(TEXT("HUD Stamina percent observed as 0.6"), HUD_SPBar->GetPercent(), 0.6f);
				TestEqual(TEXT("HUD Stamina current text observed as '60'"), HUD_SPCurr->GetText().ToString(), TEXT("60"));

				// No Player healing feature is authored yet, so drive an upward ASC change directly.
				ASC1->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
				ASC1->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

				TestEqual(TEXT("HUD Health percent refreshes to 1.0 after an upward ASC change"), HUD_HPBar->GetPercent(), 1.0f);
				TestEqual(TEXT("HUD Health current text refreshes to '100' after an upward ASC change"), HUD_HPCurr->GetText().ToString(), TEXT("100"));
				TestEqual(TEXT("HUD Stamina percent refreshes to 1.0 after an upward ASC change"), HUD_SPBar->GetPercent(), 1.0f);
				TestEqual(TEXT("HUD Stamina current text refreshes to '100' after an upward ASC change"), HUD_SPCurr->GetText().ToString(), TEXT("100"));
			}

			// Re-bind to Pawn 2 (simulating respawn / re-possess)
			PC->TriggerTestBindToPawn(PlayerPawn2);
			TestEqual(TEXT("PC bound to PlayerPawn2 ASC"), PC->GetTestBoundAbilitySystemComponent(), PlayerPawn2->GetAbilitySystemComponent());
			TestTrue(TEXT("PC has all 4 bound attribute delegates for Pawn 2"), PC->HasBoundAttributeDelegates());

			// Immediately reflects Pawn 2 initial attributes (100 / 100)
			TestEqual(TEXT("HUD Health percent reflects Pawn 2 initial state (1.0)"), HUD_HPBar->GetPercent(), 1.0f);
			TestEqual(TEXT("HUD Health current text reflects Pawn 2 initial state ('100')"), HUD_HPCurr->GetText().ToString(), TEXT("100"));

			// Old Pawn 1 attribute change is completely ignored by controller and HUD
			if (ASC1)
			{
				ASC1->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
				ASC1->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 15.0f);

				TestEqual(TEXT("Old Pawn 1 change does NOT mutate HUD Health percent"), HUD_HPBar->GetPercent(), 1.0f);
				TestEqual(TEXT("Old Pawn 1 change does NOT mutate HUD Health current text"), HUD_HPCurr->GetText().ToString(), TEXT("100"));
			}

			// New Pawn 2 attribute change updates HUD
			if (UAbilitySystemComponent* ASC2 = PlayerPawn2->GetAbilitySystemComponent())
			{
				ASC2->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 45.0f);
				TestEqual(TEXT("New Pawn 2 change updates HUD Health percent to 0.45"), HUD_HPBar->GetPercent(), 0.45f);
				TestEqual(TEXT("New Pawn 2 change updates HUD Health current text to '45'"), HUD_HPCurr->GetText().ToString(), TEXT("45"));
			}

			// Unbind and verify clean state
			PC->TriggerTestUnbindCurrentPawn();
			TestNull(TEXT("PC bound ASC is cleared after UnbindCurrentPawn"), PC->GetTestBoundAbilitySystemComponent());
			TestFalse(TEXT("PC delegate handles are all cleared after UnbindCurrentPawn"), PC->HasBoundAttributeDelegates());

			// Clean up actors
			PC->Destroy();
			PlayerPawn1->Destroy();
			PlayerPawn2->Destroy();
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
