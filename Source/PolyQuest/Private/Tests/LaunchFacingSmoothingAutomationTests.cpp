#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/LaunchFacingSmoothingState.h"
#include "AbilitySystem/Tasks/AbilityTask_TurnToFacing.h"
#include "Character/Player/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestLaunchFacingSmoothingAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLaunchFacingSmoothingAutomationTest,
	"PolyQuest.Combat.LaunchFacingSmoothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FLaunchFacingWorldCleanup
	{
		UWorld* World = nullptr;
		~FLaunchFacingWorldCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

bool FLaunchFacingSmoothingAutomationTest::RunTest(const FString& Parameters)
{
	// -------------------------------------------------------------------------
	// SECTION 1: Pure State Machine Unit Tests (FLaunchFacingSmoothingState)
	// -------------------------------------------------------------------------
	{
		FLaunchFacingSmoothingState State;

		// 1.1 Initial uninitialized state
		TestEqual(TEXT("Initial StartYaw is 0"), State.GetStartYaw(), 0.0f);
		TestEqual(TEXT("Initial TargetYaw is 0"), State.GetTargetYaw(), 0.0f);
		TestTrue(TEXT("Initial LaunchVelocity is ZeroVector"), State.GetLaunchVelocity().IsZero());
		TestFalse(TEXT("Initial bHasFrozenLaunch is false"), State.HasFrozenLaunch());
		TestFalse(TEXT("Initial bCommitReceived is false"), State.IsCommitReceived());
		TestFalse(TEXT("Initial bTurnCompleted is false"), State.IsTurnCompleted());
		TestFalse(TEXT("Initial bLaunchIssued is false"), State.IsLaunchIssued());

		// 1.2 Unfrozen state operations fail-closed
		FVector DummyVelocity = FVector(100.0, 100.0, 100.0);
		TestFalse(TEXT("TryReceiveCommit fails on unfrozen state"), State.TryReceiveCommit());
		TestFalse(TEXT("TryConsumeLaunchVelocity fails on unfrozen state"), State.TryConsumeLaunchVelocity(DummyVelocity));
		TestTrue(TEXT("TryConsumeLaunchVelocity resets out parameter on unfrozen failure"), DummyVelocity.IsZero());

		// 1.3 Valid Freezes: Cardinal directions
		// Front attacker (Target -> Attacker is (1, 0, 0)) at ImpactReferenceYaw = 0:
		// Target should face Attacker (Yaw = 0), LaunchVelocity should be away (-X, +Z)
		TestTrue(TEXT("TryFreeze succeeds for front attacker at Yaw=0"),
			State.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, 450.0f, 550.0f));
		TestTrue(TEXT("Frozen state is valid"), State.HasFrozenLaunch());
		TestEqual(TEXT("StartYaw is 0"), State.GetStartYaw(), 0.0f);
		TestNearlyEqual(TEXT("TargetYaw is 0 for front attacker"), State.GetTargetYaw(), 0.0f, 0.001f);
		TestNearlyEqual(TEXT("LaunchVelocity X is -450"), State.GetLaunchVelocity().X, -450.0, 0.001);
		TestNearlyEqual(TEXT("LaunchVelocity Y is 0"), State.GetLaunchVelocity().Y, 0.0, 0.001);
		TestNearlyEqual(TEXT("LaunchVelocity Z is 550"), State.GetLaunchVelocity().Z, 550.0, 0.001);

		// Back attacker (Target -> Attacker is (-1, 0, 0)) at ImpactReferenceYaw = 0:
		// Target should face Attacker (Yaw = 180), LaunchVelocity should be away (+X, +Z)
		TestTrue(TEXT("TryFreeze succeeds for back attacker at Yaw=0"),
			State.TryFreeze(FVector(-1.0, 0.0, 0.0), 0.0f, 450.0f, 550.0f));
		TestTrue(TEXT("TargetYaw is 180 for back attacker"), FMath::IsNearlyEqual(FMath::Abs(State.GetTargetYaw()), 180.0f, 0.001f));
		TestNearlyEqual(TEXT("LaunchVelocity X is +450"), State.GetLaunchVelocity().X, 450.0, 0.001);
		TestNearlyEqual(TEXT("LaunchVelocity Z is 550"), State.GetLaunchVelocity().Z, 550.0, 0.001);

		// Right attacker (Target -> Attacker is (0, 1, 0)) at ImpactReferenceYaw = 45:
		TestTrue(TEXT("TryFreeze succeeds for right attacker at Yaw=45"),
			State.TryFreeze(FVector(0.0, 1.0, 0.0), 45.0f, 450.0f, 550.0f));
		TestNearlyEqual(TEXT("StartYaw is 45"), State.GetStartYaw(), 45.0f, 0.001f);
		TestNearlyEqual(TEXT("TargetYaw is 135 for right attacker at Yaw=45"), State.GetTargetYaw(), 135.0f, 0.001f);

		// 1.4 Invalid Freezes fail-closed and leave state reset
		TestFalse(TEXT("TryFreeze fails on ZeroVector direction"), State.TryFreeze(FVector::ZeroVector, 0.0f, 450.0f, 550.0f));
		TestFalse(TEXT("State is reset after failed freeze"), State.HasFrozenLaunch());

		TestFalse(TEXT("TryFreeze fails on NaN direction"), State.TryFreeze(FVector(NAN, 0.0, 0.0), 0.0f, 450.0f, 550.0f));
		TestFalse(TEXT("TryFreeze fails on Inf direction"), State.TryFreeze(FVector(INFINITY, 0.0, 0.0), 0.0f, 450.0f, 550.0f));
		TestFalse(TEXT("TryFreeze fails on NaN ReferenceYaw"), State.TryFreeze(FVector(1.0, 0.0, 0.0), NAN, 450.0f, 550.0f));
		TestFalse(TEXT("TryFreeze fails on zero horizontal speed"), State.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, 0.0f, 550.0f));
		TestFalse(TEXT("TryFreeze fails on negative horizontal speed"), State.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, -450.0f, 550.0f));
		TestFalse(TEXT("TryFreeze fails on zero vertical speed"), State.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, 450.0f, 0.0f));
		TestFalse(TEXT("TryFreeze fails on negative vertical speed"), State.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, 450.0f, -550.0f));

		// 1.5 Ordering Case A: Commit received BEFORE Turn completed
		{
			FLaunchFacingSmoothingState StateA;
			TestTrue(TEXT("Case A: TryFreeze succeeds"), StateA.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, 450.0f, 550.0f));

			TestTrue(TEXT("Case A: First TryReceiveCommit succeeds"), StateA.TryReceiveCommit());
			TestTrue(TEXT("Case A: bCommitReceived is true"), StateA.IsCommitReceived());
			TestFalse(TEXT("Case A: Duplicate TryReceiveCommit returns false"), StateA.TryReceiveCommit());

			FVector OutVel = FVector::ZeroVector;
			TestFalse(TEXT("Case A: Cannot consume velocity before turn completes"), StateA.TryConsumeLaunchVelocity(OutVel));
			TestTrue(TEXT("Case A: OutVel remains ZeroVector on premature consume"), OutVel.IsZero());

			StateA.MarkTurnCompleted();
			TestTrue(TEXT("Case A: bTurnCompleted is true"), StateA.IsTurnCompleted());

			TestTrue(TEXT("Case A: TryConsumeLaunchVelocity succeeds after both events"), StateA.TryConsumeLaunchVelocity(OutVel));
			TestNearlyEqual(TEXT("Case A: Consumed velocity X matches frozen value"), OutVel.X, -450.0, 0.001);
			TestNearlyEqual(TEXT("Case A: Consumed velocity Z matches frozen value"), OutVel.Z, 550.0, 0.001);
			TestTrue(TEXT("Case A: bLaunchIssued is true"), StateA.IsLaunchIssued());

			FVector OutVelSecond = FVector(1.0, 1.0, 1.0);
			TestFalse(TEXT("Case A: Second TryConsumeLaunchVelocity fails"), StateA.TryConsumeLaunchVelocity(OutVelSecond));
			TestTrue(TEXT("Case A: OutVelSecond reset to ZeroVector"), OutVelSecond.IsZero());

			TestFalse(TEXT("Case A: Cannot accept commit after launch issued"), StateA.TryReceiveCommit());
		}

		// 1.6 Ordering Case B: Turn completed BEFORE Commit received
		{
			FLaunchFacingSmoothingState StateB;
			TestTrue(TEXT("Case B: TryFreeze succeeds"), StateB.TryFreeze(FVector(-1.0, 0.0, 0.0), 0.0f, 450.0f, 550.0f));

			StateB.MarkTurnCompleted();
			TestTrue(TEXT("Case B: bTurnCompleted is true"), StateB.IsTurnCompleted());
			TestFalse(TEXT("Case B: bCommitReceived is false"), StateB.IsCommitReceived());

			FVector OutVel = FVector::ZeroVector;
			TestFalse(TEXT("Case B: Cannot consume velocity before commit received"), StateB.TryConsumeLaunchVelocity(OutVel));
			TestTrue(TEXT("Case B: OutVel remains ZeroVector on premature consume"), OutVel.IsZero());

			TestTrue(TEXT("Case B: TryReceiveCommit succeeds"), StateB.TryReceiveCommit());
			TestTrue(TEXT("Case B: bCommitReceived is true"), StateB.IsCommitReceived());

			TestTrue(TEXT("Case B: TryConsumeLaunchVelocity succeeds after both events"), StateB.TryConsumeLaunchVelocity(OutVel));
			TestNearlyEqual(TEXT("Case B: Consumed velocity X matches frozen value"), OutVel.X, 450.0, 0.001);
			TestNearlyEqual(TEXT("Case B: Consumed velocity Z matches frozen value"), OutVel.Z, 550.0, 0.001);
			TestTrue(TEXT("Case B: bLaunchIssued is true"), StateB.IsLaunchIssued());
		}

		// 1.7 Reset restores clean initial state
		{
			FLaunchFacingSmoothingState StateReset;
			StateReset.TryFreeze(FVector(1.0, 0.0, 0.0), 0.0f, 450.0f, 550.0f);
			StateReset.TryReceiveCommit();
			StateReset.MarkTurnCompleted();
			FVector OutVel;
			StateReset.TryConsumeLaunchVelocity(OutVel);
			TestTrue(TEXT("State is fully completed before reset"), StateReset.IsLaunchIssued());

			StateReset.Reset();
			TestEqual(TEXT("Reset StartYaw is 0"), StateReset.GetStartYaw(), 0.0f);
			TestEqual(TEXT("Reset TargetYaw is 0"), StateReset.GetTargetYaw(), 0.0f);
			TestTrue(TEXT("Reset LaunchVelocity is ZeroVector"), StateReset.GetLaunchVelocity().IsZero());
			TestFalse(TEXT("Reset bHasFrozenLaunch is false"), StateReset.HasFrozenLaunch());
			TestFalse(TEXT("Reset bCommitReceived is false"), StateReset.IsCommitReceived());
			TestFalse(TEXT("Reset bTurnCompleted is false"), StateReset.IsTurnCompleted());
			TestFalse(TEXT("Reset bLaunchIssued is false"), StateReset.IsLaunchIssued());
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: AbilityTask & Turn Execution Integration Tests (World Ticks)
	// -------------------------------------------------------------------------
	if (!TestNotNull(TEXT("Engine is available for launch facing automation"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LaunchFacingTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FLaunchFacingWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform::Identity);
	if (!TestNotNull(TEXT("Player spawned successfully"), Player))
	{
		return false;
	}

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC is valid"), PlayerASC))
	{
		return false;
	}

	FGameplayAbilitySpecHandle AbilityHandle = PlayerASC->GiveAbility(
		FGameplayAbilitySpec(UTestLaunchFacingSmoothingAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("UTestLaunchFacingSmoothingAbility given to Player ASC"), AbilityHandle.IsValid());

	const bool bActivated = PlayerASC->TryActivateAbility(AbilityHandle);
	TestTrue(TEXT("UTestLaunchFacingSmoothingAbility activated successfully"), bActivated);

	FGameplayAbilitySpec* AbilitySpec = PlayerASC->FindAbilitySpecFromHandle(AbilityHandle);
	UTestLaunchFacingSmoothingAbility* ActiveAbility = AbilitySpec ? Cast<UTestLaunchFacingSmoothingAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Active test ability instance found"), ActiveAbility))
	{
		return false;
	}

	auto AdvanceWorld = [World](float DeltaSeconds = 0.033f)
	{
		World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
		++GFrameCounter;
	};

	// 2.1 Fixed-rate angular progression: 0 -> 180 at 1440 deg/s
	// Total angle: 180 deg. Step: 0.05s * 1440 deg/s = 72 deg.
	// Expected: Tick 1 (0.05s) -> turned 72 deg, Tick 2 (0.05s) -> turned 144 deg, Tick 3 (0.025s) -> 180 deg (complete)
	{
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		ActiveAbility->StartTurnTask(0.0f, 180.0f, 1440.0f);

		TestFalse(TEXT("2.1: Task not completed initially"), ActiveAbility->WasCompleted());
		TestEqual(TEXT("2.1: Completed count is 0 initially"), ActiveAbility->GetCompletedCount(), 0);

		// Tick 1: 0.05s -> rotated 72 deg
		AdvanceWorld(0.05f);
		TestNearlyEqual(TEXT("2.1: Turned ~72 deg after 0.05s"), FMath::Abs(static_cast<float>(Player->GetActorRotation().Yaw)), 72.0f, 0.1f);
		TestFalse(TEXT("2.1: Task not completed after 0.05s"), ActiveAbility->WasCompleted());

		// Tick 2: 0.05s -> rotated another 72 deg -> 144 deg
		AdvanceWorld(0.05f);
		TestNearlyEqual(TEXT("2.1: Turned ~144 deg after 0.10s"), FMath::Abs(static_cast<float>(Player->GetActorRotation().Yaw)), 144.0f, 0.1f);
		TestFalse(TEXT("2.1: Task not completed after 0.10s"), ActiveAbility->WasCompleted());

		// Tick 3: 0.025s -> remaining 36 deg covered -> snaps to exact 180 deg
		AdvanceWorld(0.025f);
		TestTrue(TEXT("2.1: Yaw is 180 deg at completion"), FMath::IsNearlyEqual(FMath::Abs(static_cast<float>(Player->GetActorRotation().Yaw)), 180.0f, 0.01f));
		TestTrue(TEXT("2.1: Task completed after 0.125s total"), ActiveAbility->WasCompleted());
		TestEqual(TEXT("2.1: Completed count is 1"), ActiveAbility->GetCompletedCount(), 1);

		// Subsequent tick: yaw does not change and completion is not broadcast again
		AdvanceWorld(0.05f);
		TestTrue(TEXT("2.1: Yaw remains 180 deg on subsequent tick"), FMath::IsNearlyEqual(FMath::Abs(static_cast<float>(Player->GetActorRotation().Yaw)), 180.0f, 0.01f));
		TestEqual(TEXT("2.1: Completed count remains 1 on subsequent tick"), ActiveAbility->GetCompletedCount(), 1);
	}

	// 2.2 Shortest arc across +180 / -180 boundary
	// Start at 170 deg, Target at -170 deg (Delta is +20 deg across 180 boundary)
	// Rate = 1440 deg/s. Step for 0.01s = 14.4 deg.
	{
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 170.0f, 0.0f));
		ActiveAbility->StartTurnTask(170.0f, -170.0f, 1440.0f);

		// Tick 0.01s -> 170 + 14.4 = 184.4 -> -175.6 deg
		AdvanceWorld(0.01f);
		TestNearlyEqual(TEXT("2.2: Yaw crosses 180 boundary along shortest arc to ~ -175.6 deg"),
			Player->GetActorRotation().Yaw, -175.6, 0.1);
		TestFalse(TEXT("2.2: Task not completed after first step"), ActiveAbility->WasCompleted());

		// Tick 0.01s -> remaining 5.6 deg <= 14.4 deg -> completes at -170.0 deg
		AdvanceWorld(0.01f);
		TestNearlyEqual(TEXT("2.2: Yaw reached exact target -170.0 deg"), Player->GetActorRotation().Yaw, -170.0, 0.01);
		TestTrue(TEXT("2.2: Task completed along shortest arc"), ActiveAbility->WasCompleted());
		TestEqual(TEXT("2.2: Completed count is 1"), ActiveAbility->GetCompletedCount(), 1);
	}

	// 2.3 Sub-tolerance synchronous completion (<= 0.01 deg)
	{
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));
		ActiveAbility->StartTurnTask(45.0f, 45.005f, 1440.0f);

		TestTrue(TEXT("2.3: Task completes synchronously on delta <= 0.01 deg"), ActiveAbility->WasCompleted());
		TestNearlyEqual(TEXT("2.3: Yaw set to exact target 45.005 deg"), Player->GetActorRotation().Yaw, 45.005, 0.001);
		TestEqual(TEXT("2.3: Completed count is 1"), ActiveAbility->GetCompletedCount(), 1);
	}

	// 2.4 Mid-turn cancellation: no subsequent transform writes or completions
	{
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		ActiveAbility->StartTurnTask(0.0f, 180.0f, 1440.0f);

		// Tick to ~72 deg
		AdvanceWorld(0.05f);
		TestNearlyEqual(TEXT("2.4: Turned ~72 deg before cancellation"), FMath::Abs(static_cast<float>(Player->GetActorRotation().Yaw)), 72.0f, 0.1f);

		const int32 CompletedBeforeCancel = ActiveAbility->GetCompletedCount();
		ActiveAbility->EndTurnTask();

		// Tick again after cancellation
		AdvanceWorld(0.05f);
		TestNearlyEqual(TEXT("2.4: Turned ~72 deg remains unchanged after cancellation"), FMath::Abs(static_cast<float>(Player->GetActorRotation().Yaw)), 72.0f, 0.1f);
		TestEqual(TEXT("2.4: No completion broadcast synthesized after cancellation"),
			ActiveAbility->GetCompletedCount(), CompletedBeforeCancel);
	}

	// 2.5 Fail-closed on invalid parameters: broadcasts failure and does not mutate transform
	{
		// Zero rate
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 10.0f, 0.0f));
		ActiveAbility->StartTurnTask(10.0f, 90.0f, 0.0f);
		TestTrue(TEXT("2.5: Zero rate broadcasts failure"), ActiveAbility->WasFailed());
		TestNearlyEqual(TEXT("2.5: Actor yaw unchanged on zero rate failure"), Player->GetActorRotation().Yaw, 10.0, 0.001);

		// Negative rate
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 10.0f, 0.0f));
		ActiveAbility->StartTurnTask(10.0f, 90.0f, -1440.0f);
		TestTrue(TEXT("2.5: Negative rate broadcasts failure"), ActiveAbility->WasFailed());
		TestNearlyEqual(TEXT("2.5: Actor yaw unchanged on negative rate failure"), Player->GetActorRotation().Yaw, 10.0, 0.001);

		// NaN rate
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 10.0f, 0.0f));
		ActiveAbility->StartTurnTask(10.0f, 90.0f, NAN);
		TestTrue(TEXT("2.5: NaN rate broadcasts failure"), ActiveAbility->WasFailed());
		TestNearlyEqual(TEXT("2.5: Actor yaw unchanged on NaN rate failure"), Player->GetActorRotation().Yaw, 10.0, 0.001);

		// NaN target yaw
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 10.0f, 0.0f));
		ActiveAbility->StartTurnTask(10.0f, NAN, 1440.0f);
		TestTrue(TEXT("2.5: NaN target yaw broadcasts failure"), ActiveAbility->WasFailed());
		TestNearlyEqual(TEXT("2.5: Actor yaw unchanged on NaN target yaw failure"), Player->GetActorRotation().Yaw, 10.0, 0.001);

		// NaN start yaw
		ActiveAbility->ResetCounters();
		Player->SetActorRotation(FRotator(0.0f, 10.0f, 0.0f));
		ActiveAbility->StartTurnTask(NAN, 90.0f, 1440.0f);
		TestTrue(TEXT("2.5: NaN start yaw broadcasts failure"), ActiveAbility->WasFailed());
		TestNearlyEqual(TEXT("2.5: Actor yaw unchanged on NaN start yaw failure"), Player->GetActorRotation().Yaw, 10.0, 0.001);
	}

	return true;
}

#endif
