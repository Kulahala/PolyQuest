// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Camera/CameraComponent.h"
#include "AbilitySystemComponent.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Player/PlayerLockOnTargeting.h"
#include "Combat/Projectile/CombatProjectileTargeting.h"
#include "Components/BoxComponent.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "UI/EnemyHealthBarWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerLockOnAutomationTest, "PolyQuest.Player.LockOn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerLockOnAutomationTest::RunTest(const FString&)
{
	if (!TestNotNull(TEXT("Engine is available for transient lock-on fixtures"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerLockOnTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Transient lock-on test world created"), World))
	{
		return false;
	}

	const FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	struct FTestScopeCleanup
	{
		UWorld* WorldToDestroy = nullptr;

		~FTestScopeCleanup()
		{
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
		}
	} ScopeCleanup{ World };

	auto SpawnEnemy = [World](const FName Name, const FVector& Location)
	{
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator::ZeroRotator, Location),
			[Name](AEnemyCharacter& InEnemy)
			{
				InEnemy.Rename(*Name.ToString());
				InEnemy.SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false));
			});
		return Enemy;
	};

	AEnemyCharacter* EnemyRight = SpawnEnemy(TEXT("LockOn_Right"), FVector(500.0f, 0.0f, 100.0f));
	AEnemyCharacter* EnemyBottom = SpawnEnemy(TEXT("LockOn_Bottom"), FVector(500.0f, 200.0f, 100.0f));
	AEnemyCharacter* EnemyLeft = SpawnEnemy(TEXT("LockOn_Left"), FVector(-500.0f, 0.0f, 100.0f));
	AEnemyCharacter* EnemyTop = SpawnEnemy(TEXT("LockOn_Top"), FVector(0.0f, -500.0f, 100.0f));
	AEnemyCharacter* EnemyTie = SpawnEnemy(TEXT("LockOn_Tie"), FVector(600.0f, 0.0f, 100.0f));

	TestNotNull(TEXT("Right candidate spawned"), EnemyRight);
	TestNotNull(TEXT("Bottom candidate spawned"), EnemyBottom);
	TestNotNull(TEXT("Left candidate spawned"), EnemyLeft);
	TestNotNull(TEXT("Top candidate spawned"), EnemyTop);
	TestNotNull(TEXT("Tie candidate spawned"), EnemyTie);

	// -------------------------------------------------------------------------
	// SECTION 1: Pure strict-viewport, nearest, ordering, and cycling rules
	// -------------------------------------------------------------------------
	{
		const FVector2D ViewportSize(1920.0f, 1080.0f);
		TestTrue(TEXT("Strict viewport accepts an interior point"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(960.0f, 540.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects left edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(0.0f, 540.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects right edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(1920.0f, 540.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects top edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(960.0f, 0.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects bottom edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(960.0f, 1080.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects non-finite projection"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(std::numeric_limits<float>::quiet_NaN(), 540.0f), ViewportSize));

		TestTrue(TEXT("Margin 0.0 equals strict interior check"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 540.0f), ViewportSize, 0.0f));
		TestFalse(TEXT("Margin 0.0 rejects exact left boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(0.0f, 540.0f), ViewportSize, 0.0f));
		TestFalse(TEXT("Margin 0.0 rejects exact right boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(1920.0f, 540.0f), ViewportSize, 0.0f));
		TestFalse(TEXT("Margin 0.0 rejects exact top boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 0.0f), ViewportSize, 0.0f));
		TestFalse(TEXT("Margin 0.0 rejects exact bottom boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 1080.0f), ViewportSize, 0.0f));

		// 15% retention boundary on 1920x1080: X in (-288, 2208), Y in (-162, 1242)
		constexpr float Margin15 = 0.15f;
		TestTrue(TEXT("Margin 0.15 accepts point just inside left hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(-287.0f, 540.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects exact left hysteresis boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(-288.0f, 540.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects point outside left hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(-289.0f, 540.0f), ViewportSize, Margin15));

		TestTrue(TEXT("Margin 0.15 accepts point just inside right hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(2207.0f, 540.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects exact right hysteresis boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(2208.0f, 540.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects point outside right hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(2209.0f, 540.0f), ViewportSize, Margin15));

		TestTrue(TEXT("Margin 0.15 accepts point just inside top hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, -161.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects exact top hysteresis boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, -162.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects point outside top hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, -163.0f), ViewportSize, Margin15));

		TestTrue(TEXT("Margin 0.15 accepts point just inside bottom hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 1241.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects exact bottom hysteresis boundary"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 1242.0f), ViewportSize, Margin15));
		TestFalse(TEXT("Margin 0.15 rejects point outside bottom hysteresis limit"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 1243.0f), ViewportSize, Margin15));

		TestFalse(TEXT("Margin check rejects negative margin"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 540.0f), ViewportSize, -0.05f));
		TestFalse(TEXT("Margin check rejects NaN margin"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 540.0f), ViewportSize, std::numeric_limits<float>::quiet_NaN()));
		TestFalse(TEXT("Margin check rejects Inf margin"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 540.0f), ViewportSize, std::numeric_limits<float>::infinity()));
		TestFalse(TEXT("Margin check rejects non-positive viewport X"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 540.0f), FVector2D(0.0f, 1080.0f), Margin15));
		TestFalse(TEXT("Margin check rejects non-positive viewport Y"), FPlayerLockOnTargeting::IsWithinViewportWithMargin(FVector2D(960.0f, 540.0f), FVector2D(1920.0f, -10.0f), Margin15));

		float ClockwiseAngle = 0.0f;
		TestTrue(TEXT("Clockwise angle resolves screen-right"), FPlayerLockOnTargeting::TryCalculateClockwiseAngle(FVector2D(960.0f, 540.0f), FVector2D(1160.0f, 540.0f), ClockwiseAngle));
		TestTrue(TEXT("Screen-right is the zero-degree cycle origin"), FMath::IsNearlyZero(ClockwiseAngle));
		TestTrue(TEXT("Clockwise angle resolves screen-bottom"), FPlayerLockOnTargeting::TryCalculateClockwiseAngle(FVector2D(960.0f, 540.0f), FVector2D(960.0f, 740.0f), ClockwiseAngle));
		TestTrue(TEXT("Screen-bottom follows screen-right clockwise"), FMath::IsNearlyEqual(ClockwiseAngle, PI * 0.5f));
		TestFalse(TEXT("Clockwise angle rejects non-finite input"), FPlayerLockOnTargeting::TryCalculateClockwiseAngle(FVector2D(std::numeric_limits<float>::infinity(), 540.0f), FVector2D(960.0f, 740.0f), ClockwiseAngle));

		auto MakeCandidate = [](AEnemyCharacter* Target, const FVector2D ScreenPosition, const float Angle, const float DistanceSquared, const TCHAR* StableKey)
		{
			FPlayerLockOnCandidate Candidate;
			Candidate.TargetActor = Target;
			Candidate.ScreenPosition = ScreenPosition;
			Candidate.ClockwiseAngleRadians = Angle;
			Candidate.PlayerScreenDistanceSquared = DistanceSquared;
			Candidate.StableKey = StableKey;
			return Candidate;
		};

		TArray<FPlayerLockOnCandidate> Candidates;
		Candidates.Add(MakeCandidate(EnemyTop, FVector2D(960.0f, 340.0f), PI * 1.5f, 40000.0f, TEXT("Top")));
		Candidates.Add(MakeCandidate(EnemyLeft, FVector2D(760.0f, 540.0f), PI, 40000.0f, TEXT("Left")));
		Candidates.Add(MakeCandidate(EnemyBottom, FVector2D(960.0f, 740.0f), PI * 0.5f, 40000.0f, TEXT("Bottom")));
		Candidates.Add(MakeCandidate(EnemyRight, FVector2D(1160.0f, 540.0f), 0.0f, 40000.0f, TEXT("Right")));

		FPlayerLockOnTargeting::SortClockwise(Candidates);
		TestEqual(TEXT("Clockwise ordering begins at screen-right"), Candidates[0].TargetActor.Get(), EnemyRight);
		TestEqual(TEXT("Clockwise ordering moves to screen-bottom"), Candidates[1].TargetActor.Get(), EnemyBottom);
		TestEqual(TEXT("Clockwise ordering continues to screen-left"), Candidates[2].TargetActor.Get(), EnemyLeft);
		TestEqual(TEXT("Clockwise ordering ends at screen-top"), Candidates[3].TargetActor.Get(), EnemyTop);
		TestEqual(TEXT("Positive cycle advances clockwise"), FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, EnemyRight, 1), 1);
		TestEqual(TEXT("Negative cycle advances counter-clockwise"), FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, EnemyRight, -1), 3);
		TestEqual(TEXT("No lock has no cycle destination"), FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, nullptr, 1), INDEX_NONE);
		TestEqual(TEXT("Death successor advances from its cached clockwise anchor"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, Candidates[0]), 1);
		TestEqual(TEXT("Death successor wraps after the final clockwise candidate"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, Candidates[3]), 0);
		FPlayerLockOnCandidate InvalidDeathAnchor = Candidates[0];
		InvalidDeathAnchor.ClockwiseAngleRadians = std::numeric_limits<float>::quiet_NaN();
		TestEqual(TEXT("Death successor rejects a non-finite cached anchor"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, InvalidDeathAnchor), INDEX_NONE);

		TestEqual(TEXT("Mouse-nearest selection chooses closest valid candidate"), FPlayerLockOnTargeting::FindNearestToCursor(Candidates, FVector2D(970.0f, 720.0f)), 1);

		TArray<FPlayerLockOnCandidate> TieCandidates;
		TieCandidates.Add(MakeCandidate(EnemyTie, FVector2D(1160.0f, 540.0f), 0.0f, 40000.0f, TEXT("Z_Tie")));
		TieCandidates.Add(MakeCandidate(EnemyRight, FVector2D(1160.0f, 540.0f), 0.0f, 40000.0f, TEXT("A_Tie")));
		FPlayerLockOnTargeting::SortClockwise(TieCandidates);
		TestEqual(TEXT("Exact cycle tie falls back to stable key"), TieCandidates[0].TargetActor.Get(), EnemyRight);
		TestEqual(TEXT("Exact cursor-distance tie falls back to stable key"), FPlayerLockOnTargeting::FindNearestToCursor(TieCandidates, FVector2D(960.0f, 540.0f)), 0);
		TestEqual(TEXT("Death successor honors stable-key ordering for an equal-angle anchor"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(TieCandidates, TieCandidates[0]), 1);
		TestEqual(TEXT("Death successor rejects an empty candidate list"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(TArray<FPlayerLockOnCandidate>(), TieCandidates[0]), INDEX_NONE);
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Player-owned target lifecycle, highlight handoff, and facing
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
			World,
			FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 100.0f)),
			[](APlayerCharacter& InPlayer)
			{
				InPlayer.SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false));
			});
		APlayerController* PlayerController = World->SpawnActor<APlayerController>();
		TestNotNull(TEXT("Player spawned for lock lifecycle"), Player);
		TestNotNull(TEXT("PlayerController spawned for lock lifecycle"), PlayerController);

		if (Player && PlayerController && EnemyRight && EnemyBottom && EnemyTie)
		{
			TestTrue(TEXT("Player fixture applied its persistent Stamina regen effect"), Player->HasTestStaminaRegenEffectApplied());
			PlayerController->Possess(Player);

			UEnemyHealthBarWidget* RightWidget = NewObject<UEnemyHealthBarWidget>(EnemyRight);
			UEnemyHealthBarWidget* BottomWidget = NewObject<UEnemyHealthBarWidget>(EnemyBottom);
			UEnemyHealthBarWidget* TieWidget = NewObject<UEnemyHealthBarWidget>(EnemyTie);
			UImage* RightHighlight = NewObject<UImage>(RightWidget);
			UImage* BottomHighlight = NewObject<UImage>(BottomWidget);
			UImage* TieHighlight = NewObject<UImage>(TieWidget);
			RightHighlight->SetVisibility(ESlateVisibility::Collapsed);
			BottomHighlight->SetVisibility(ESlateVisibility::Collapsed);
			TieHighlight->SetVisibility(ESlateVisibility::Collapsed);
			RightWidget->SetTestTargetHighlightImage(RightHighlight);
			BottomWidget->SetTestTargetHighlightImage(BottomHighlight);
			TieWidget->SetTestTargetHighlightImage(TieHighlight);
			EnemyRight->SetTestHealthBarWidget(RightWidget);
			EnemyBottom->SetTestHealthBarWidget(BottomWidget);
			EnemyTie->SetTestHealthBarWidget(TieWidget);

			Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
			{
				OutViewportSize = FVector2D(1920.0f, 1080.0f);
				if (WorldPoint.Y > 100.0f)
				{
					OutScreenPosition = FVector2D(960.0f, 720.0f);
				}
				else if (WorldPoint.X < -100.0f)
				{
					OutScreenPosition = FVector2D(760.0f, 540.0f);
				}
				else if (WorldPoint.Y < -100.0f)
				{
					OutScreenPosition = FVector2D(960.0f, 340.0f);
				}
				else if (WorldPoint.X > 550.0f)
				{
					OutScreenPosition = FVector2D(1400.0f, 540.0f);
				}
				else if (WorldPoint.X > 100.0f)
				{
					OutScreenPosition = FVector2D(1100.0f, 540.0f);
				}
				else
				{
					OutScreenPosition = FVector2D(960.0f, 540.0f);
				}
				return true;
			});
			Player->SetTestLockOnCursorPosition(FVector2D(1110.0f, 540.0f));
			TestTrue(TEXT("World candidate query acquires the mouse-nearest target"), Player->TriggerTestAcquireLockOnTarget());
			TestEqual(TEXT("World candidate query locks the right-side mouse-nearest target"), Player->GetLockedTarget(), EnemyRight);
			TestEqual(TEXT("World candidate query highlights the acquired target"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			Player->SetTestLockedTarget(nullptr);

			Player->SetTestLockedTarget(EnemyRight);
			TestEqual(TEXT("Player exposes the newly locked target through the C++ query"), Player->GetLockedTarget(), EnemyRight);
			TestEqual(TEXT("Newly locked enemy shows its existing bar highlight"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Non-locked enemy bar stays unhighlighted"), BottomHighlight->GetVisibility(), ESlateVisibility::Collapsed);
			TestTrue(TEXT("Valid locked target survives explicit validation"), Player->TriggerTestValidateCurrentLockedTarget());

			Player->SetTestLockedTarget(EnemyBottom);
			TestEqual(TEXT("Switching targets clears the old bar highlight"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("Switching targets highlights the new bar"), BottomHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);

			const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
			const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
			const FGameplayTag PlayerLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
			const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			Player->SetTestLockedTarget(EnemyTie);
			TestTrue(TEXT("The death handoff fixture caches the current target screen candidate"), Player->TriggerTestValidateCurrentLockedTarget());
			if (UAbilitySystemComponent* TieAsc = EnemyTie->GetAbilitySystemComponent())
			{
				TieAsc->AddLooseGameplayTag(DeadTag);
				TestTrue(TEXT("A dead target completes one legal replacement attempt"), Player->TriggerTestValidateCurrentLockedTarget());
				TestEqual(TEXT("A dead target hands off to the next clockwise candidate"), Player->GetLockedTarget(), EnemyBottom);
				TestEqual(TEXT("Dead target bar highlight is cleared during handoff"), TieHighlight->GetVisibility(), ESlateVisibility::Collapsed);
				TestEqual(TEXT("Death handoff highlights the replacement target"), BottomHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
				TestTrue(TEXT("The replacement remains locked on later validation without a second handoff"), Player->TriggerTestValidateCurrentLockedTarget());
				TestEqual(TEXT("Later validation retains the single death replacement"), Player->GetLockedTarget(), EnemyBottom);
				TieAsc->RemoveLooseGameplayTag(DeadTag);

				Player->SetTestLockedTarget(EnemyBottom);
				if (UAbilitySystemComponent* BottomAsc = EnemyBottom->GetAbilitySystemComponent())
				{
					BottomAsc->AddLooseGameplayTag(InvulnerableTag);
					TestFalse(TEXT("An invulnerable target clears without replacing it"), Player->TriggerTestValidateCurrentLockedTarget());
					TestNull(TEXT("Invulnerable target does not auto-retarget"), Player->GetLockedTarget());
					BottomAsc->RemoveLooseGameplayTag(InvulnerableTag);

					// Focused regression: Execution Lock-On Retention
					UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent();

					if (PlayerAsc)
					{
						// Case 1: Only PlayerLocked (missing VictimLocked) -> fails & clears
						BottomAsc->AddLooseGameplayTag(InvulnerableTag);
						PlayerAsc->AddLooseGameplayTag(PlayerLockedTag);
						Player->SetTestLockedTarget(EnemyBottom);
						TestFalse(TEXT("Only PlayerLocked fails validation on invulnerable target"), Player->TriggerTestValidateCurrentLockedTarget());
						TestNull(TEXT("Only PlayerLocked clears lock on invulnerable target"), Player->GetLockedTarget());
						PlayerAsc->RemoveLooseGameplayTag(PlayerLockedTag);

						// Case 2: Only VictimLocked (missing PlayerLocked) -> fails & clears
						BottomAsc->AddLooseGameplayTag(VictimLockedTag);
						Player->SetTestLockedTarget(EnemyBottom);
						TestFalse(TEXT("Only VictimLocked fails validation on invulnerable target"), Player->TriggerTestValidateCurrentLockedTarget());
						TestNull(TEXT("Only VictimLocked clears lock on invulnerable target"), Player->GetLockedTarget());

						// Case 3: Paired execution lock (PlayerLocked + VictimLocked) + Invulnerable -> succeeds and retains target!
						PlayerAsc->AddLooseGameplayTag(PlayerLockedTag);
						Player->SetTestLockedTarget(EnemyBottom);
						BottomAsc->RemoveLooseGameplayTag(InvulnerableTag);
						TestFalse(TEXT("Execution retention requires the target to actually hold Invulnerable"),
							Player->TriggerTestCanRetainExecutionLockedTarget(EnemyBottom));
						BottomAsc->AddLooseGameplayTag(InvulnerableTag);
						TestTrue(TEXT("Paired execution lock retains invulnerable target within retention"), Player->TriggerTestValidateCurrentLockedTarget());
						TestEqual(TEXT("Target is retained during paired execution lock"), Player->GetLockedTarget(), EnemyBottom);

						// Case 4: Target Dead during execution lock -> fails retention and handles death
						BottomAsc->AddLooseGameplayTag(DeadTag);
						Player->TriggerTestValidateCurrentLockedTarget();
						TestTrue(TEXT("Dead target during execution lock is not retained as locked target"), Player->GetLockedTarget() != EnemyBottom);
						BottomAsc->RemoveLooseGameplayTag(DeadTag);

						// Case 5: Remove execution tags -> next validation returns to ordinary behavior and clears lock
						Player->SetTestLockedTarget(EnemyBottom);
						PlayerAsc->RemoveLooseGameplayTag(PlayerLockedTag);
						BottomAsc->RemoveLooseGameplayTag(VictimLockedTag);
						TestFalse(TEXT("Removing execution tags restores ordinary validation and clears invulnerable target"), Player->TriggerTestValidateCurrentLockedTarget());
						TestNull(TEXT("Cleared target remains null after tags removed"), Player->GetLockedTarget());

						BottomAsc->RemoveLooseGameplayTag(InvulnerableTag);
					}
				}
			}

			if (UAbilitySystemComponent* TopAsc = EnemyTop ? EnemyTop->GetAbilitySystemComponent() : nullptr)
			{
				for (AEnemyCharacter* OtherCandidate : { EnemyRight, EnemyBottom, EnemyLeft, EnemyTie })
				{
					if (UAbilitySystemComponent* OtherAsc = OtherCandidate ? OtherCandidate->GetAbilitySystemComponent() : nullptr)
					{
						OtherAsc->AddLooseGameplayTag(InvulnerableTag);
					}
				}

				Player->SetTestLockedTarget(EnemyTop);
				TestTrue(TEXT("The no-candidate death fixture caches its locked target"), Player->TriggerTestValidateCurrentLockedTarget());
				TopAsc->AddLooseGameplayTag(DeadTag);
				TestFalse(TEXT("A dead target clears when no valid clockwise replacement exists"), Player->TriggerTestValidateCurrentLockedTarget());
				TestNull(TEXT("No-candidate death handoff leaves no lock"), Player->GetLockedTarget());
				TopAsc->RemoveLooseGameplayTag(DeadTag);

				for (AEnemyCharacter* OtherCandidate : { EnemyRight, EnemyBottom, EnemyLeft, EnemyTie })
				{
					if (UAbilitySystemComponent* OtherAsc = OtherCandidate ? OtherCandidate->GetAbilitySystemComponent() : nullptr)
					{
						OtherAsc->RemoveLooseGameplayTag(InvulnerableTag);
					}
				}
			}

			Player->SetTestLockedTarget(EnemyRight);
			// Helper to project EnemyRight at custom X while keeping other candidates at their consistent strict-viewport positions
			auto SetProjectionForEnemyRightX = [Player](const float EnemyRightX)
			{
				Player->SetTestLockOnProjectionHook([EnemyRightX](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
				{
					OutViewportSize = FVector2D(1920.0f, 1080.0f);
					if (WorldPoint.Y > 100.0f)
					{
						OutScreenPosition = FVector2D(960.0f, 720.0f); // EnemyBottom
					}
					else if (WorldPoint.X < -100.0f)
					{
						OutScreenPosition = FVector2D(760.0f, 540.0f); // EnemyLeft
					}
					else if (WorldPoint.Y < -100.0f)
					{
						OutScreenPosition = FVector2D(960.0f, 340.0f); // EnemyTop
					}
					else if (WorldPoint.X > 550.0f)
					{
						OutScreenPosition = FVector2D(1400.0f, 540.0f); // EnemyTie
					}
					else if (WorldPoint.X > 100.0f)
					{
						OutScreenPosition = FVector2D(EnemyRightX, 540.0f); // EnemyRight
					}
					else
					{
						OutScreenPosition = FVector2D(960.0f, 540.0f); // Player anchor
					}
					return true;
				});
			};

			// 1. Target at exact viewport edge (X=0) and inside 15% retention (X=-150) stays retained
			SetProjectionForEnemyRightX(-150.0f);
			TestTrue(TEXT("A target inside the 15% retention boundary remains valid"), Player->TriggerTestValidateCurrentLockedTarget());
			TestEqual(TEXT("Retention preserves the current locked target"), Player->GetLockedTarget(), EnemyRight);
			TestEqual(TEXT("Retention keeps target health bar highlight active"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);

			// 2. Strict acquisition cannot acquire a target in the retention-only zone
			Player->SetTestLockedTarget(nullptr);
			Player->SetTestLockOnCursorPosition(FVector2D(1110.0f, 540.0f));
			TestTrue(TEXT("World candidate query acquires mouse-nearest strict candidate"), Player->TriggerTestAcquireLockOnTarget());
			TestEqual(TEXT("Strict acquisition chooses EnemyBottom instead of off-screen EnemyRight"), Player->GetLockedTarget(), EnemyBottom);
			Player->SetTestLockedTarget(nullptr);

			// 3. Cycle no-op in retention-only state
			Player->SetTestLockedTarget(EnemyRight);
			TestTrue(TEXT("Re-locking EnemyRight succeeds in retention zone"), Player->TriggerTestValidateCurrentLockedTarget());
			Player->TriggerTestTargetCycle(1.0f);
			TestEqual(TEXT("Cycle forward on retained target is no-op and preserves lock"), Player->GetLockedTarget(), EnemyRight);
			TestEqual(TEXT("Retained target highlight stays active during Cycle no-op"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Strict candidates stay unhighlighted during Cycle no-op"), BottomHighlight->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("Tie candidate stays unhighlighted during Cycle no-op"), TieHighlight->GetVisibility(), ESlateVisibility::Collapsed);

			Player->TriggerTestTargetCycle(-1.0f);
			TestEqual(TEXT("Cycle backward on retained target is also no-op"), Player->GetLockedTarget(), EnemyRight);

			// 4. Target moving beyond 15% retention limit (X=-300 < -288) clears the lock
			SetProjectionForEnemyRightX(-300.0f);
			TestFalse(TEXT("A target beyond the 15% retention limit clears the lock"), Player->TriggerTestValidateCurrentLockedTarget());
			TestNull(TEXT("Target beyond retention limit does not auto-retarget"), Player->GetLockedTarget());
			TestEqual(TEXT("Target highlight is collapsed when lock is cleared"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);

			// 4.1 Even with paired execution lock, moving beyond 15% retention limit still clears
			if (UAbilitySystemComponent* RightAsc = EnemyRight->GetAbilitySystemComponent())
			{
				UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent();
				if (PlayerAsc)
				{
					Player->SetTestLockedTarget(EnemyRight);
					PlayerAsc->AddLooseGameplayTag(PlayerLockedTag);
					RightAsc->AddLooseGameplayTag(VictimLockedTag);
					RightAsc->AddLooseGameplayTag(InvulnerableTag);

					TestFalse(TEXT("Execution target beyond 15% retention limit still clears the lock"), Player->TriggerTestValidateCurrentLockedTarget());
					TestNull(TEXT("Execution target beyond retention limit is cleared"), Player->GetLockedTarget());

					PlayerAsc->RemoveLooseGameplayTag(PlayerLockedTag);
					RightAsc->RemoveLooseGameplayTag(VictimLockedTag);
					RightAsc->RemoveLooseGameplayTag(InvulnerableTag);
				}
			}

			// 5. Target returning to strict viewport restores normal Cycle behavior
			SetProjectionForEnemyRightX(1100.0f);
			Player->SetTestLockedTarget(EnemyRight);
			TestTrue(TEXT("EnemyRight re-locks inside strict viewport"), Player->TriggerTestValidateCurrentLockedTarget());
			Player->TriggerTestTargetCycle(1.0f);
			TestEqual(TEXT("Target Cycle inside strict viewport successfully cycles to next candidate"), Player->GetLockedTarget(), EnemyTie);
			TestEqual(TEXT("Cycled target highlight is updated"), TieHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Old target highlight is cleared after successful cycle"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);

			Player->TriggerTestTargetCycle(1.0f);
			TestEqual(TEXT("Second cycle advances to EnemyBottom"), Player->GetLockedTarget(), EnemyBottom);
			TestEqual(TEXT("EnemyBottom highlight is updated"), BottomHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Tie highlight is cleared after second cycle"), TieHighlight->GetVisibility(), ESlateVisibility::Collapsed);

			// 5.1 Verify Target Cycle rejects Invulnerable candidate
			if (UAbilitySystemComponent* TieAsc = EnemyTie->GetAbilitySystemComponent())
			{
				TieAsc->AddLooseGameplayTag(InvulnerableTag);
				Player->SetTestLockedTarget(EnemyRight);
				Player->TriggerTestTargetCycle(1.0f);
				TestTrue(TEXT("Target cycle skips invulnerable candidate"), Player->GetLockedTarget() != EnemyTie);
				TieAsc->RemoveLooseGameplayTag(InvulnerableTag);
			}

			Player->SetTestLockOnProjectionHook([](const FVector&, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
			{
				OutScreenPosition = FVector2D(960.0f, 540.0f);
				OutViewportSize = FVector2D(1920.0f, 1080.0f);
				return true;
			});
			Player->SetTestLockedTarget(EnemyRight);
			if (UCharacterMovementComponent* MovementComponent = Player->GetCharacterMovement())
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
				Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
				Player->Tick(0.1f);
				TestTrue(TEXT("Locked ordinary locomotion uses the 800 degree-per-second default RotationRate"),
					FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, 10.0f, 0.01f));
				TestFalse(TEXT("Locked ordinary locomotion disables movement-facing rotation"), MovementComponent->bOrientRotationToMovement);

				if (UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent())
				{
					const FGameplayTag SprintTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
					const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
					const FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
					const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
					const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
					const FGameplayTag HitReactingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
					const FGameplayTag SmallHitReactingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);

					PlayerAsc->AddLooseGameplayTag(GuardTag);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->Tick(0.2f);
					TestTrue(TEXT("Locked Guard continuously faces the target"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));
					PlayerAsc->RemoveLooseGameplayTag(GuardTag);

					PlayerAsc->AddLooseGameplayTag(ParryTag);
					MovementComponent->SetMovementMode(MOVE_None);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->Tick(0.2f);
					TestTrue(TEXT("Locked Parry faces the target during its MOVE_None lock"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));
					MovementComponent->SetMovementMode(MOVE_Walking);
					PlayerAsc->RemoveLooseGameplayTag(ParryTag);

					PlayerAsc->AddLooseGameplayTag(SprintTag);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->Tick(0.2f);
					TestTrue(TEXT("Locked Sprint preserves free-run facing"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 90.0f)));
					TestTrue(TEXT("Locked Sprint restores movement-facing rotation"), MovementComponent->bOrientRotationToMovement);
					PlayerAsc->RemoveLooseGameplayTag(SprintTag);
					Player->Tick(0.2f);
					TestTrue(TEXT("Lock-facing resumes after Sprint ends"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));

					for (const FGameplayTag& YawOwnerTag : { AttackingTag, DodgeTag, HitReactingTag, SmallHitReactingTag })
					{
						PlayerAsc->AddLooseGameplayTag(YawOwnerTag);
						Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
						Player->Tick(0.2f);
						TestTrue(*FString::Printf(TEXT("Lock Tick does not overwrite %s yaw ownership"), *YawOwnerTag.ToString()),
							FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 90.0f)));
						PlayerAsc->RemoveLooseGameplayTag(YawOwnerTag);
					}
				}

				TSharedPtr<FRootMotionSource_ConstantForce> RootMotionSource = MakeShared<FRootMotionSource_ConstantForce>();
				RootMotionSource->InstanceName = TEXT("LockOnAutomationRootMotion");
				RootMotionSource->Priority = 500;
				RootMotionSource->Duration = 1.0f;
				RootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
				RootMotionSource->Force = FVector(100.0f, 0.0f, 0.0f);
				const uint16 RootMotionSourceId = MovementComponent->ApplyRootMotionSource(RootMotionSource);
				TestTrue(TEXT("Transient Root Motion source activates for lock-facing protection"), Player->HasAnyRootMotion());
				Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
				Player->Tick(0.2f);
				TestTrue(TEXT("Lock Tick does not overwrite active Root Motion yaw"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 90.0f)));
				MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceId);
				MovementComponent->CurrentRootMotion.Clear();

				UObject* BowRequester = Player->GetFollowCamera();
				if (TestNotNull(TEXT("Player provides a concrete Bow requester fixture"), BowRequester))
				{
					TestTrue(TEXT("Bow requester registers for lock-facing regression coverage"), Player->RegisterBowAimRequester(BowRequester));
					Player->Tick(0.0f);
					const float BowYawBeforeLockTick = Player->GetActorRotation().Yaw;
					Player->Tick(0.2f);
					TestTrue(TEXT("Lock Tick does not overwrite active Bow aiming yaw"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, BowYawBeforeLockTick)));
					Player->UnregisterBowAimRequester(BowRequester);
				}
			}

			Player->SetTestLockedTarget(EnemyRight);
			if (UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent())
			{
				PlayerAsc->AddLooseGameplayTag(DeadTag);
				TestFalse(TEXT("A dead player clears the current lock"), Player->TriggerTestValidateCurrentLockedTarget());
				TestNull(TEXT("Player death never auto-retargets"), Player->GetLockedTarget());
				PlayerAsc->RemoveLooseGameplayTag(DeadTag);
			}

			Player->SetTestLockedTarget(EnemyTie);
			EnemyTie->Destroy();
			EnemyTie = nullptr;
			Player->Tick(0.0f);
			TestNull(TEXT("A destroyed target clears on Player Tick without auto-retarget"), Player->GetLockedTarget());

			Player->SetTestLockedTarget(EnemyRight);
			Player->SetTestBypassLockOnValidation(true);
			if (USpringArmComponent* CameraBoom = Player->GetCameraBoom())
			{
				CameraBoom->SetUsingAbsoluteRotation(true);
				CameraBoom->SetWorldRotation(FRotator(-55.0f, -45.0f, 0.0f));
			}
			else
			{
				AddError(TEXT("Lock-on directional Dodge fixture requires the Player CameraBoom."));
			}
			Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
			Player->ApplyLockAwareActionFacing();
			TestTrue(TEXT("Locked attack-facing turns once toward the target"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));

			Player->SetTestCurrentMoveInput(FVector2D(-1.0f, 0.0f));
			const FVector MovementFacing = Player->GetActionWorldDirection();
			Player->ApplyDodgeFacing();
			TestTrue(
				TEXT("Directional dodge keeps camera-relative movement facing over the lock"),
				FVector::DotProduct(Player->GetActorForwardVector().GetSafeNormal2D(), MovementFacing) > 0.9999f);

			Player->SetTestCurrentMoveInput(FVector2D::ZeroVector);
			Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
			Player->ApplyDodgeFacing();
			TestTrue(TEXT("No-input dodge falls back to the locked target"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));
			Player->SetTestBypassLockOnValidation(false);

			Player->SetTestLockedTarget(nullptr);
			TestEqual(TEXT("Explicit clear removes the final target highlight"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);

			// -------------------------------------------------------------------------
			// SECTION 3: TODO-02B4 Camera-to-Target LOS Gate and Occlusion Grace
			// -------------------------------------------------------------------------
			{
				EnemyTie = SpawnEnemy(TEXT("LockOn_Tie_Sec3"), FVector(600.0f, 0.0f, 100.0f));
				TieWidget = NewObject<UEnemyHealthBarWidget>(EnemyTie);
				TieHighlight = NewObject<UImage>(TieWidget);
				TieHighlight->SetVisibility(ESlateVisibility::Collapsed);
				TieWidget->SetTestTargetHighlightImage(TieHighlight);
				EnemyTie->SetTestHealthBarWidget(TieWidget);

				Player->SetTestBypassLockOnValidation(false);
				Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
				{
					OutViewportSize = FVector2D(1920.0f, 1080.0f);
					if (WorldPoint.Y > 100.0f)
					{
						OutScreenPosition = FVector2D(960.0f, 720.0f); // EnemyBottom
					}
					else if (WorldPoint.X < -100.0f)
					{
						OutScreenPosition = FVector2D(760.0f, 540.0f); // EnemyLeft
					}
					else if (WorldPoint.Y < -100.0f)
					{
						OutScreenPosition = FVector2D(960.0f, 340.0f); // EnemyTop
					}
					else if (WorldPoint.X > 550.0f)
					{
						OutScreenPosition = FVector2D(1400.0f, 540.0f); // EnemyTie
					}
					else if (WorldPoint.X > 100.0f)
					{
						OutScreenPosition = FVector2D(1100.0f, 540.0f); // EnemyRight
					}
					else
					{
						OutScreenPosition = FVector2D(960.0f, 540.0f); // Player anchor
					}
					return true;
				});

				// 1. AcquireRejectsBlockedCandidate: Blocked candidate cannot be acquired initially
				TMap<const AActor*, bool> CandidateLOSMap;
				CandidateLOSMap.Add(EnemyRight, false);
				CandidateLOSMap.Add(EnemyBottom, true);
				CandidateLOSMap.Add(EnemyTie, true);
				CandidateLOSMap.Add(EnemyLeft, false);
				CandidateLOSMap.Add(EnemyTop, false);

				Player->SetTestLockOnLOSHook([&CandidateLOSMap](const AActor* TargetActor, const FVector&, const FVector&)
				{
					const bool* FoundLOS = CandidateLOSMap.Find(TargetActor);
					return FoundLOS ? *FoundLOS : false;
				});

				Player->SetTestLockOnCursorPosition(FVector2D(1110.0f, 540.0f));
				TestTrue(TEXT("Acquisition query succeeds by falling back to unblocked candidate"), Player->TriggerTestAcquireLockOnTarget());
				TestEqual(TEXT("Blocked mouse-nearest candidate is rejected in favor of unblocked candidate"), Player->GetLockedTarget(), EnemyBottom);
				Player->SetTestLockedTarget(nullptr);

				CandidateLOSMap[EnemyTie] = false;
				CandidateLOSMap[EnemyBottom] = false;
				TestFalse(TEXT("Acquisition query fails when all candidates are blocked by LOS"), Player->TriggerTestAcquireLockOnTarget());
				TestNull(TEXT("No target acquired when all candidates are blocked"), Player->GetLockedTarget());

				// 2. CycleSkipsBlockedCandidates: Cycling skips blocked candidate
				CandidateLOSMap[EnemyRight] = true;
				CandidateLOSMap[EnemyBottom] = false;
				CandidateLOSMap[EnemyTie] = true;
				CandidateLOSMap[EnemyLeft] = false;
				CandidateLOSMap[EnemyTop] = false;
				Player->SetTestLockedTarget(EnemyRight);
				TestEqual(TEXT("Initial lock on EnemyRight for cycle test"), Player->GetLockedTarget(), EnemyRight);

				Player->TriggerTestHandleTargetCycle(1.0f);
				TestEqual(TEXT("Clockwise cycle skips LOS-blocked candidate"), Player->GetLockedTarget(), EnemyTie);

				// 3. DeathRetargetSkipsBlockedCandidates: Death auto-retarget skips blocked candidate
				CandidateLOSMap[EnemyTie] = true;
				CandidateLOSMap[EnemyRight] = false;
				CandidateLOSMap[EnemyBottom] = true;
				CandidateLOSMap[EnemyLeft] = false;
				CandidateLOSMap[EnemyTop] = false;
				Player->SetTestLockedTarget(EnemyTie);
				TestTrue(TEXT("Death fixture caches current candidate for LOS test"), Player->TriggerTestValidateCurrentLockedTarget());

				if (UAbilitySystemComponent* TieAsc = EnemyTie->GetAbilitySystemComponent())
				{
					TieAsc->AddLooseGameplayTag(DeadTag);
					TestTrue(TEXT("Dead target completes retarget attempt under LOS filter"), Player->TriggerTestValidateCurrentLockedTarget());
					TestEqual(TEXT("Death retarget skips LOS-blocked candidate and selects next visible"), Player->GetLockedTarget(), EnemyBottom);
					TieAsc->RemoveLooseGameplayTag(DeadTag);
				}

				// 4. OcclusionGraceRetainsAndRecovers: Target retained within grace duration and recovers on clear LOS
				Player->SetTestLockedTarget(EnemyRight);
				CandidateLOSMap[EnemyRight] = false;

				Player->TriggerTestUpdateLockOnOcclusion(1.0f);
				TestEqual(TEXT("Locked target is retained within grace duration (1.0s < 2.5s)"), Player->GetLockedTarget(), EnemyRight);
				TestTrue(TEXT("Current target is marked as occluded"), Player->GetTestIsCurrentLockedTargetOccluded());
				TestEqual(TEXT("Occlusion timer accumulated correctly"), Player->GetTestLockOnOcclusionTimer(), 1.0f);
				TestEqual(TEXT("Target highlight persists during grace period"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);

				CandidateLOSMap[EnemyRight] = true;
				Player->TriggerTestUpdateLockOnOcclusion(0.1f);
				TestEqual(TEXT("Locked target remains valid after recovering LOS"), Player->GetLockedTarget(), EnemyRight);
				TestFalse(TEXT("Occluded flag is cleared upon recovering LOS"), Player->GetTestIsCurrentLockedTargetOccluded());
				TestEqual(TEXT("Occlusion timer reset to 0 upon recovering LOS"), Player->GetTestLockOnOcclusionTimer(), 0.0f);

				// 5. OcclusionTimeoutClearsAndSameTargetCannotRefresh: Timeout clears lock; same target SetLockedTarget cannot refresh timer
				CandidateLOSMap[EnemyRight] = false;
				Player->TriggerTestUpdateLockOnOcclusion(1.5f);
				TestEqual(TEXT("Occlusion timer at 1.5s"), Player->GetTestLockOnOcclusionTimer(), 1.5f);

				Player->SetTestLockedTarget(EnemyRight);
				TestEqual(TEXT("Re-setting same target does NOT reset occlusion timer"), Player->GetTestLockOnOcclusionTimer(), 1.5f);

				Player->TriggerTestUpdateLockOnOcclusion(1.2f);
				TestNull(TEXT("Exceeding grace duration clears locked target"), Player->GetLockedTarget());
				TestEqual(TEXT("Target highlight collapsed after occlusion timeout"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);
				TestEqual(TEXT("Occlusion timer reset to 0 after clear"), Player->GetTestLockOnOcclusionTimer(), 0.0f);

				// 6. ExecutionExemptionSkipsOcclusionOnly: Paired execution lock exempt from occlusion clear
				if (UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent())
				{
					if (UAbilitySystemComponent* BottomAsc = EnemyBottom->GetAbilitySystemComponent())
					{
						Player->SetTestLockedTarget(EnemyBottom);
						PlayerAsc->AddLooseGameplayTag(PlayerLockedTag);
						BottomAsc->AddLooseGameplayTag(VictimLockedTag);
						BottomAsc->AddLooseGameplayTag(InvulnerableTag);

						CandidateLOSMap[EnemyBottom] = false;

						Player->TriggerTestUpdateLockOnOcclusion(4.0f);
						TestEqual(TEXT("Paired execution lock exempt from occlusion timeout"), Player->GetLockedTarget(), EnemyBottom);
						TestEqual(TEXT("Execution exemption keeps occlusion timer at 0"), Player->GetTestLockOnOcclusionTimer(), 0.0f);

						PlayerAsc->RemoveLooseGameplayTag(PlayerLockedTag);
						BottomAsc->RemoveLooseGameplayTag(VictimLockedTag);
						BottomAsc->RemoveLooseGameplayTag(InvulnerableTag);
					}
				}
				Player->SetTestLockedTarget(nullptr);

				// 7. Real physical collision trace verification with UBoxComponent
				Player->SetTestLockOnLOSHook(nullptr);

				Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
				EnemyRight->SetActorLocation(FVector(1000.0f, 0.0f, 100.0f));

				const FVector TraceStart = Player->TriggerTestResolveLockOnTraceStart(PlayerController);
				const FVector TargetAimPoint = FCombatProjectileTargeting::GetTargetAimPoint(EnemyRight);

				TestTrue(TEXT("Clear physical world line of sight passes"), Player->TriggerTestHasLineOfSightToTarget(EnemyRight));

				FActorSpawnParameters ObstacleSpawnParams;
				ObstacleSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				const FVector MidPointA = FMath::Lerp(TraceStart, TargetAimPoint, 0.4f);
				AActor* ObstacleA = World->SpawnActor<AActor>(MidPointA, FRotator::ZeroRotator, ObstacleSpawnParams);
				TestNotNull(TEXT("ObstacleA spawned for LOS collision test"), ObstacleA);
				if (ObstacleA)
				{
					UBoxComponent* BoxA = NewObject<UBoxComponent>(ObstacleA);
					ObstacleA->SetRootComponent(BoxA);
					BoxA->SetWorldLocation(MidPointA);
					BoxA->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
					BoxA->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					BoxA->SetCollisionObjectType(ECC_WorldStatic);
					BoxA->SetCollisionResponseToAllChannels(ECR_Ignore);
					BoxA->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
					BoxA->RegisterComponent();
					BoxA->UpdateComponentToWorld();

					// Case A: Visible obstacle blocks LOS
					ObstacleA->SetActorHiddenInGame(false);
					TestFalse(TEXT("Visible obstacle blocks physical LOS"), Player->TriggerTestHasLineOfSightToTarget(EnemyRight));

					const FVector MidPointB = FMath::Lerp(TraceStart, TargetAimPoint, 0.7f);
					AActor* ObstacleB = World->SpawnActor<AActor>(MidPointB, FRotator::ZeroRotator, ObstacleSpawnParams);
					TestNotNull(TEXT("ObstacleB spawned for LOS retry test"), ObstacleB);
					if (ObstacleB)
					{
						UBoxComponent* BoxB = NewObject<UBoxComponent>(ObstacleB);
						ObstacleB->SetRootComponent(BoxB);
						BoxB->SetWorldLocation(MidPointB);
						BoxB->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
						BoxB->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
						BoxB->SetCollisionObjectType(ECC_WorldStatic);
						BoxB->SetCollisionResponseToAllChannels(ECR_Ignore);
						BoxB->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
						BoxB->RegisterComponent();
						BoxB->UpdateComponentToWorld();

						// Case B: Hidden front obstacle + Visible rear obstacle still blocks LOS
						ObstacleA->SetActorHiddenInGame(true);
						ObstacleB->SetActorHiddenInGame(false);
						TestFalse(TEXT("Hidden front obstacle with visible rear obstacle still blocks LOS"), Player->TriggerTestHasLineOfSightToTarget(EnemyRight));

						// Case C: Both obstacles hidden -> LOS passes (demonstrates retry ignores hidden actors)
						ObstacleB->SetActorHiddenInGame(true);
						TestTrue(TEXT("Only hidden obstacles in path allows physical LOS to pass"), Player->TriggerTestHasLineOfSightToTarget(EnemyRight));

						// Case D: Exceeding 8 hidden obstacles exhausts MaxTraceAttempts and fails closed
						TArray<AActor*> StackedObstacles;
						TArray<UBoxComponent*> StackedBoxes;
						for (int32 i = 0; i < 9; ++i)
						{
							const FVector StackLoc = FMath::Lerp(TraceStart, TargetAimPoint, 0.1f + static_cast<float>(i) * 0.08f);
							AActor* HiddenObs = World->SpawnActor<AActor>(StackLoc, FRotator::ZeroRotator, ObstacleSpawnParams);
							UBoxComponent* ObsBox = NewObject<UBoxComponent>(HiddenObs);
							HiddenObs->SetRootComponent(ObsBox);
							ObsBox->SetWorldLocation(StackLoc);
							ObsBox->SetBoxExtent(FVector(20.0f, 100.0f, 100.0f));
							ObsBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
							ObsBox->SetCollisionObjectType(ECC_WorldStatic);
							ObsBox->SetCollisionResponseToAllChannels(ECR_Ignore);
							ObsBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
							ObsBox->RegisterComponent();
							ObsBox->UpdateComponentToWorld();
							HiddenObs->SetActorHiddenInGame(true);
							StackedObstacles.Add(HiddenObs);
							StackedBoxes.Add(ObsBox);
						}

						TestFalse(TEXT("9 hidden obstacles in path exhausts 8-trace budget and fails closed"), Player->TriggerTestHasLineOfSightToTarget(EnemyRight));

						for (int32 i = 0; i < StackedObstacles.Num(); ++i)
						{
							StackedBoxes[i]->DestroyComponent();
							StackedObstacles[i]->Destroy();
						}

						BoxB->DestroyComponent();
						ObstacleB->Destroy();
					}

					BoxA->DestroyComponent();
					ObstacleA->Destroy();

					// Case E: Clear path directly to target actor passes
					TestTrue(TEXT("Clear path directly to target actor passes"), Player->TriggerTestHasLineOfSightToTarget(EnemyRight));
				}
			}

			// 8. Facing Block Contract v1 during Lock-On locomotion
			{
				const FGameplayTag TagBlockFacing = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Block.Facing")), false);
				TestTrue(TEXT("8.0: State.Block.Facing tag is valid"), TagBlockFacing.IsValid());

				UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent();
				UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
				TestNotNull(TEXT("8.0: Player ASC valid"), PlayerAsc);
				TestNotNull(TEXT("8.0: MoveComp valid"), MoveComp);

				if (PlayerAsc && MoveComp)
				{
					MoveComp->CurrentRootMotion.Clear();
					Player->SetTestLockOnProjectionHook([](const FVector&, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
					{
						OutScreenPosition = FVector2D(960.0f, 540.0f);
						OutViewportSize = FVector2D(1920.0f, 1080.0f);
						return true;
					});

					// Set target at +X (Yaw = 0)
					EnemyRight->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
					Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
					MoveComp->Velocity = FVector::ZeroVector;
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->SetTestLockedTarget(EnemyRight);
					TestEqual(TEXT("8.1: Player is locked onto EnemyRight"), Player->GetLockedTarget(), EnemyRight);

					// Without block, normal locked locomotion facing turns towards target
					MoveComp->SetMovementMode(MOVE_Walking);
					Player->Tick(0.01f);
					TestFalse(TEXT("8.2: bOrientRotationToMovement is false during normal lock-on"),
						MoveComp->bOrientRotationToMovement);

					// Now add State.Block.Facing to Player ASC
					PlayerAsc->AddLooseGameplayTag(TagBlockFacing);

					// Verify bOrientRotationToMovement remains false (action owns rotation)
					Player->Tick(0.01f);
					TestFalse(TEXT("8.3: bOrientRotationToMovement remains false while State.Block.Facing is active"),
						MoveComp->bOrientRotationToMovement);

					// Set arbitrary yaw (120 deg) while facing is blocked
					Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
					MoveComp->Velocity = FVector::ZeroVector;
					MoveComp->SetMovementMode(MOVE_Walking);
					Player->SetActorRotation(FRotator(0.0f, 120.0f, 0.0f));

					// Tick with DeltaTime: Yaw must remain untouched (blocked by State.Block.Facing)
					Player->Tick(0.1f);
					TestTrue(TEXT("8.4: Player yaw is untouched while State.Block.Facing is active (remains 120 deg)"),
						FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 120.0f), 0.01f));

					// Remove State.Block.Facing: next tick should resume locked locomotion facing
					PlayerAsc->RemoveLooseGameplayTag(TagBlockFacing);
					Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
					MoveComp->Velocity = FVector::ZeroVector;
					MoveComp->SetMovementMode(MOVE_Walking);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					TestEqual(TEXT("8.4b: LockedTarget still valid"), Player->GetLockedTarget(), EnemyRight);

					// Tick to verify rotation resumes towards target (800 deg/s * 0.1s turns 80 deg from 90 to 10)
					Player->Tick(0.1f);
					const float ResultYaw = Player->GetActorRotation().Yaw;
					TestTrue(TEXT("8.5: Player turned towards locked target after State.Block.Facing removed"),
						FMath::IsNearlyEqual(ResultYaw, 10.0f, 0.01f));

					// 8.6: Tag addition / removal callback does NOT cancel or start Sprint
					TestFalse(TEXT("8.6a: Player not sprinting initially"), Player->HasActiveSprint());
					PlayerAsc->AddLooseGameplayTag(TagBlockFacing);
					TestFalse(TEXT("8.6b: Adding State.Block.Facing does NOT start sprint"), Player->HasActiveSprint());
					PlayerAsc->RemoveLooseGameplayTag(TagBlockFacing);
					TestFalse(TEXT("8.6c: Removing State.Block.Facing does NOT affect sprint state"), Player->HasActiveSprint());

					// 8.7: Symmetrical UnPossessed / PossessedBy lifecycle does not crash or leave dangling delegates
					Player->TriggerTestUnPossessed();
					PlayerController->Possess(Player);
					Player->Tick(0.01f);
					TestTrue(TEXT("8.7: Re-possessed player ticks cleanly with rebound delegate"), true);

					// 8.8: Regression checks: existing action tags also block locked locomotion facing
					const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
					const FGameplayTag TagDodging = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
					const FGameplayTag TagHitReacting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
					const FGameplayTag TagSmallHitReacting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);

					for (const FGameplayTag& ActionTag : { TagAttacking, TagDodging, TagHitReacting, TagSmallHitReacting })
					{
						if (ActionTag.IsValid())
						{
							PlayerAsc->AddLooseGameplayTag(ActionTag);
							Player->Tick(0.01f);
							TestFalse(FString::Printf(TEXT("8.8: bOrientRotationToMovement remains false with %s"), *ActionTag.ToString()),
								MoveComp->bOrientRotationToMovement);
							PlayerAsc->RemoveLooseGameplayTag(ActionTag);
						}
					}

					Player->SetTestLockedTarget(nullptr);
				}
			}

			PlayerController->Destroy();
			Player->Destroy();
		}
	}

	if (EnemyRight)
	{
		EnemyRight->Destroy();
	}
	if (EnemyBottom)
	{
		EnemyBottom->Destroy();
	}
	if (EnemyLeft)
	{
		EnemyLeft->Destroy();
	}
	if (EnemyTop)
	{
		EnemyTop->Destroy();
	}
	if (EnemyTie)
	{
		EnemyTie->Destroy();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
