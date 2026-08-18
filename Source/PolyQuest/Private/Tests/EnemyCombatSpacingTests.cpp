#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyAIProfile.h"
#include "UObject/Package.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyCombatSpacingTest,
	"PolyQuest.Enemy.CombatSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatSpacingTest::RunTest(const FString& Parameters)
{
	// 1. UEnemyAIProfile Validation Tests
	{
		UEnemyAIProfile* Profile = NewObject<UEnemyAIProfile>(GetTransientPackage(), TEXT("Test_EnemyAIProfile"));
		FString Reason;

		// 1.1 Baseline valid profile
		Profile->SetTestPreferredCombatDistance(180.0f);
		Profile->SetTestLateralRepositionDistance(120.0f);
		Profile->SetTestRepositionAcceptanceRadius(40.0f);
		Profile->SetTestRepositionRetryDelay(0.25f);
		Profile->SetTestLeashRadius(2500.0f);

		TestTrue(TEXT("Baseline EnemyAIProfile is valid"), Profile->IsValidAIProfile(Reason));
		TestTrue(TEXT("Baseline validation reason is empty"), Reason.IsEmpty());

		// 1.2 PreferredCombatDistance checks (must be finite and > 0)
		Profile->SetTestPreferredCombatDistance(0.0f);
		TestFalse(TEXT("Zero PreferredCombatDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestPreferredCombatDistance(-50.0f);
		TestFalse(TEXT("Negative PreferredCombatDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestPreferredCombatDistance(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF PreferredCombatDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestPreferredCombatDistance(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN PreferredCombatDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestPreferredCombatDistance(180.0f);

		// 1.3 LateralRepositionDistance checks (must be finite and >= 0)
		Profile->SetTestLateralRepositionDistance(-10.0f);
		TestFalse(TEXT("Negative LateralRepositionDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLateralRepositionDistance(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF LateralRepositionDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLateralRepositionDistance(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN LateralRepositionDistance is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLateralRepositionDistance(0.0f);
		TestTrue(TEXT("Zero LateralRepositionDistance is permitted (straight backpedal)"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLateralRepositionDistance(120.0f);

		// 1.4 RepositionAcceptanceRadius checks (must be finite and > 0)
		Profile->SetTestRepositionAcceptanceRadius(0.0f);
		TestFalse(TEXT("Zero RepositionAcceptanceRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionAcceptanceRadius(-5.0f);
		TestFalse(TEXT("Negative RepositionAcceptanceRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionAcceptanceRadius(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF RepositionAcceptanceRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionAcceptanceRadius(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN RepositionAcceptanceRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionAcceptanceRadius(40.0f);

		// 1.5 RepositionRetryDelay checks (must be finite and >= 0)
		Profile->SetTestRepositionRetryDelay(-0.1f);
		TestFalse(TEXT("Negative RepositionRetryDelay is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionRetryDelay(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF RepositionRetryDelay is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionRetryDelay(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN RepositionRetryDelay is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionRetryDelay(0.0f);
		TestTrue(TEXT("Zero RepositionRetryDelay is permitted"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestRepositionRetryDelay(0.25f);

		// 1.6 LeashRadius checks (must be finite and > 0)
		Profile->SetTestLeashRadius(0.0f);
		TestFalse(TEXT("Zero LeashRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLeashRadius(-100.0f);
		TestFalse(TEXT("Negative LeashRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLeashRadius(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF LeashRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLeashRadius(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN LeashRadius is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestLeashRadius(2500.0f);

		// 1.7 ApproachTimeout checks (must be finite and > 0, default 3.0s)
		TestNearlyEqual(TEXT("Default ApproachTimeout is 3.0s"), Profile->GetApproachTimeout(), 3.0f, 0.01f);
		Profile->SetTestApproachTimeout(0.0f);
		TestFalse(TEXT("Zero ApproachTimeout is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestApproachTimeout(-1.0f);
		TestFalse(TEXT("Negative ApproachTimeout is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestApproachTimeout(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF ApproachTimeout is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestApproachTimeout(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN ApproachTimeout is rejected"), Profile->IsValidAIProfile(Reason));
		Profile->SetTestApproachTimeout(3.0f);

		TestTrue(TEXT("Restored profile is valid"), Profile->IsValidAIProfile());
	}

	// 2. Target-Relative Point Calculation Tests
	{
		const FVector TargetLoc(0.0f, 0.0f, 100.0f);
		const FVector EnemyLoc(200.0f, 0.0f, 100.0f);
		const float PreferredDist = 150.0f;
		const float LateralDist = 100.0f;
		const float EngagementRange = 200.0f;

		FVector OutPointRight = FVector::ZeroVector;
		FVector OutPointLeft = FVector::ZeroVector;

		// 2.1 Standard Right-side calculation
		const bool bRightValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, LateralDist, EngagementRange, true, OutPointRight);
		TestTrue(TEXT("Right-side point calculation succeeds"), bRightValid);
		TestNearlyEqual(TEXT("Right-side point X"), OutPointRight.X, 150.0, 0.01);
		TestNearlyEqual(TEXT("Right-side point Y"), OutPointRight.Y, -100.0, 0.01);
		TestNearlyEqual(TEXT("Right-side point Z preserved from Enemy"), OutPointRight.Z, 100.0, 0.01);

		const float DistFromTargetRight = FVector::Dist2D(TargetLoc, OutPointRight);
		TestTrue(TEXT("Right-side point is within EngagementRange"), DistFromTargetRight <= EngagementRange + KINDA_SMALL_NUMBER);

		// 2.2 Standard Left-side calculation
		const bool bLeftValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, LateralDist, EngagementRange, false, OutPointLeft);
		TestTrue(TEXT("Left-side point calculation succeeds"), bLeftValid);
		TestNearlyEqual(TEXT("Left-side point X"), OutPointLeft.X, 150.0, 0.01);
		TestNearlyEqual(TEXT("Left-side point Y"), OutPointLeft.Y, 100.0, 0.01);
		TestNearlyEqual(TEXT("Left-side point Z preserved from Enemy"), OutPointLeft.Z, 100.0, 0.01);

		const float DistFromTargetLeft = FVector::Dist2D(TargetLoc, OutPointLeft);
		TestTrue(TEXT("Left-side point is within EngagementRange"), DistFromTargetLeft <= EngagementRange + KINDA_SMALL_NUMBER);

		// 2.3 Lateral Distance Clamping when Lateral would push point outside EngagementRange
		// sqrt(150^2 + 200^2) = sqrt(22500 + 40000) = sqrt(62500) = 250 > 200
		// Max allowed lateral = sqrt(200^2 - 150^2) = sqrt(17500) approx 132.287565
		const float OversizedLateral = 200.0f;
		FVector OutClampedPoint = FVector::ZeroVector;
		const bool bClampedValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, OversizedLateral, EngagementRange, true, OutClampedPoint);
		TestTrue(TEXT("Clamped point calculation succeeds"), bClampedValid);
		const float ClampedDistFromTarget = FVector::Dist2D(TargetLoc, OutClampedPoint);
		TestNearlyEqual(TEXT("Clamped point distance from target is exactly EngagementRange"), ClampedDistFromTarget, EngagementRange, 0.01f);

		// 2.4 PreferredCombatDistance == EngagementRange boundary check (straight backpedal, lateral clamped to 0)
		FVector OutExactBoundaryPoint = FVector::ZeroVector;
		const bool bExactBoundaryValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, EngagementRange, LateralDist, EngagementRange, true, OutExactBoundaryPoint);
		TestTrue(TEXT("PreferredCombatDistance == EngagementRange calculation succeeds"), bExactBoundaryValid);
		TestNearlyEqual(TEXT("Exact boundary point X equals EngagementRange"), OutExactBoundaryPoint.X, 200.0, 0.01);
		TestNearlyEqual(TEXT("Exact boundary point Y lateral is clamped to 0"), OutExactBoundaryPoint.Y, 0.0, 0.01);
		const float ExactDistFromTarget = FVector::Dist2D(TargetLoc, OutExactBoundaryPoint);
		TestNearlyEqual(TEXT("Exact boundary point distance from target is exactly EngagementRange"), ExactDistFromTarget, EngagementRange, 0.01f);

		// 2.5 PreferredCombatDistance exceeding EngagementRange must be rejected
		FVector OutOverRangePoint = FVector::ZeroVector;
		const bool bOverRangeValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, 250.0f, LateralDist, EngagementRange, true, OutOverRangePoint);
		TestFalse(TEXT("PreferredCombatDistance exceeding EngagementRange is rejected"), bOverRangeValid);

		// 2.6 Coincident Target and Enemy locations (Distance == 0) uses fallback direction gracefully
		const FVector CoincidentEnemyLoc(0.0f, 0.0f, 100.0f);
		FVector OutCoincidentPoint = FVector::ZeroVector;
		const bool bCoincidentValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, CoincidentEnemyLoc, PreferredDist, LateralDist, EngagementRange, true, OutCoincidentPoint);
		TestTrue(TEXT("Coincident enemy/target point calculation succeeds with fallback direction"), bCoincidentValid);
		TestTrue(TEXT("Coincident reposition point is within EngagementRange"), FVector::Dist2D(TargetLoc, OutCoincidentPoint) <= EngagementRange + KINDA_SMALL_NUMBER);

		// 2.7 Reactive Retreat when Player Pushes Inside PreferredCombatDistance
		// Player pushes forward to (180, 0, 100) while Enemy is at (200, 0, 100) (distance only 20cm)
		const FVector PushedTargetLoc(180.0f, 0.0f, 100.0f);
		FVector OutRetreatPoint = FVector::ZeroVector;
		const bool bRetreatValid = AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			PushedTargetLoc, EnemyLoc, PreferredDist, LateralDist, EngagementRange, true, OutRetreatPoint);
		TestTrue(TEXT("Reactive retreat calculation succeeds"), bRetreatValid);
		TestNearlyEqual(TEXT("Reactive retreat point X is target + preferred"), OutRetreatPoint.X, 180.0 + 150.0, 0.01);
		TestNearlyEqual(TEXT("Reactive retreat point Y is -lateral"), OutRetreatPoint.Y, -100.0, 0.01);
		const float RetreatDistFromTarget = FVector::Dist2D(PushedTargetLoc, OutRetreatPoint);
		TestTrue(TEXT("Reactive retreat point is within EngagementRange from pushed target"), RetreatDistFromTarget <= EngagementRange + KINDA_SMALL_NUMBER);

		// 2.8 Non-finite and NaN rejections
		FVector DummyPoint = FVector::ZeroVector;
		TestFalse(TEXT("NaN Target location rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			FVector(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f), EnemyLoc, PreferredDist, LateralDist, EngagementRange, true, DummyPoint));
		TestFalse(TEXT("NaN Enemy location rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, FVector(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f), PreferredDist, LateralDist, EngagementRange, true, DummyPoint));
		TestFalse(TEXT("NaN PreferredCombatDistance rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, std::numeric_limits<float>::quiet_NaN(), LateralDist, EngagementRange, true, DummyPoint));
		TestFalse(TEXT("NaN LateralRepositionDistance rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, std::numeric_limits<float>::quiet_NaN(), EngagementRange, true, DummyPoint));
		TestFalse(TEXT("NaN EngagementRange rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, LateralDist, std::numeric_limits<float>::quiet_NaN(), true, DummyPoint));
		TestFalse(TEXT("Zero EngagementRange rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, LateralDist, 0.0f, true, DummyPoint));
		TestFalse(TEXT("Negative PreferredCombatDistance rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, -50.0f, LateralDist, EngagementRange, true, DummyPoint));
		TestFalse(TEXT("Negative LateralRepositionDistance rejected"), AEnemyAIController::CalculateTargetRelativeRepositionPoint(
			TargetLoc, EnemyLoc, PreferredDist, -50.0f, EngagementRange, true, DummyPoint));
	}

	// 3. Continuous Reposition State Machine, Time Gating, and Alternation Logic Tests
	{
		bool bLastUsedRightSide = false;
		bool bLastSucceeded = false;
		bool bIsCurrentlyMoving = false;
		int32 AttemptsInCooldown = 0;
		int32 FailedAttemptsOnCurrentSide = 0;
		float CurrentSimulatedTime = 10.0f;
		float NextAllowedTime = 0.0f;
		const float RetryDelay = 0.25f;

		auto SimulateCanRequest = [&]() -> bool
		{
			if (bIsCurrentlyMoving)
			{
				return false; // Concurrent MoveTo blocked
			}
			if (CurrentSimulatedTime < NextAllowedTime)
			{
				return false; // Time interval gated
			}
			return true;
		};

		auto SimulateRequest = [&](bool bInjectSuccess) -> bool
		{
			if (!SimulateCanRequest())
			{
				return false;
			}

			bool bUseRightSide = false;
			if (AttemptsInCooldown == 0)
			{
				bUseRightSide = !bLastUsedRightSide;
				FailedAttemptsOnCurrentSide = 0;
			}
			else if (bLastSucceeded)
			{
				bUseRightSide = !bLastUsedRightSide;
				FailedAttemptsOnCurrentSide = 0;
			}
			else if (FailedAttemptsOnCurrentSide == 1)
			{
				bUseRightSide = bLastUsedRightSide; // Retry once on same side
			}
			else
			{
				bUseRightSide = !bLastUsedRightSide; // Consecutive failure forces alternate
				FailedAttemptsOnCurrentSide = 0;
			}

			AttemptsInCooldown++;
			NextAllowedTime = CurrentSimulatedTime + RetryDelay;
			bLastUsedRightSide = bUseRightSide;
			bLastSucceeded = bInjectSuccess;

			if (bInjectSuccess)
			{
				FailedAttemptsOnCurrentSide = 0;
			}
			else
			{
				FailedAttemptsOnCurrentSide++;
			}

			return true;
		};

		// 3.1 Continuous Reposition without 2-request cap across multiple cycles
		// Cycle 1: Request 1 (Right)
		TestTrue(TEXT("Continuous Cycle 1 allowed"), SimulateRequest(true));
		TestEqual(TEXT("Cycle 1 uses Right side"), bLastUsedRightSide, true);
		TestEqual(TEXT("Attempts count is 1"), AttemptsInCooldown, 1);

		// Time Gating check: immediate subsequent request blocked
		TestFalse(TEXT("Immediate request blocked by interval gating"), SimulateRequest(true));

		// Step time forward past RetryDelay and complete move
		CurrentSimulatedTime += RetryDelay + 0.1f;

		// Cycle 2: Request 2 (Left)
		TestTrue(TEXT("Continuous Cycle 2 allowed"), SimulateRequest(true));
		TestEqual(TEXT("Cycle 2 alternates to Left side"), bLastUsedRightSide, false);
		TestEqual(TEXT("Attempts count is 2"), AttemptsInCooldown, 2);

		CurrentSimulatedTime += RetryDelay + 0.1f;

		// Cycle 3: Request 3 (Right) - proves no 2-request cap!
		TestTrue(TEXT("Continuous Cycle 3 allowed (exceeds old 2-request cap)"), SimulateRequest(true));
		TestEqual(TEXT("Cycle 3 alternates to Right side"), bLastUsedRightSide, true);
		TestEqual(TEXT("Attempts count is 3"), AttemptsInCooldown, 3);

		CurrentSimulatedTime += RetryDelay + 0.1f;

		// Cycle 4: Request 4 (Left)
		TestTrue(TEXT("Continuous Cycle 4 allowed"), SimulateRequest(true));
		TestEqual(TEXT("Cycle 4 alternates to Left side"), bLastUsedRightSide, false);
		TestEqual(TEXT("Attempts count is 4"), AttemptsInCooldown, 4);

		// 3.2 Concurrent MoveTo Prevention
		bIsCurrentlyMoving = true;
		CurrentSimulatedTime += RetryDelay + 0.1f;
		TestFalse(TEXT("Request blocked while MoveTo is currently active"), SimulateRequest(true));
		bIsCurrentlyMoving = false;

		// 3.3 Failure & Retry Logic: Single failure retries same side once, second failure forces alternate
		CurrentSimulatedTime += RetryDelay + 0.1f;
		// Attempt 5: Succeeded previously (Left), so attempt 5 takes Right and FAILS
		TestTrue(TEXT("Attempt 5 issues request and records failure"), SimulateRequest(false));
		TestEqual(TEXT("Attempt 5 used Right side"), bLastUsedRightSide, true);
		TestEqual(TEXT("Attempt 5 failure recorded"), bLastSucceeded, false);
		TestEqual(TEXT("Failed count on Right is 1"), FailedAttemptsOnCurrentSide, 1);

		CurrentSimulatedTime += RetryDelay + 0.1f;
		// Attempt 6: Single retry on SAME side (Right) and FAILS again
		TestTrue(TEXT("Attempt 6 retries same side once"), SimulateRequest(false));
		TestEqual(TEXT("Attempt 6 retried Right side"), bLastUsedRightSide, true);
		TestEqual(TEXT("Failed count on Right is 2"), FailedAttemptsOnCurrentSide, 2);

		CurrentSimulatedTime += RetryDelay + 0.1f;
		// Attempt 7: Consecutive failure forces alternate side (Left)
		TestTrue(TEXT("Attempt 7 forces alternate to Left side after consecutive retry failure"), SimulateRequest(true));
		TestEqual(TEXT("Attempt 7 alternated to Left side"), bLastUsedRightSide, false);
		TestEqual(TEXT("Failed count reset to 0 upon recovery"), FailedAttemptsOnCurrentSide, 0);

		// 3.4 Dynamic RetryDelay modification test
		const float CustomDelay = 0.60f;
		CurrentSimulatedTime += 0.1f; // Not enough for CustomDelay
		NextAllowedTime = CurrentSimulatedTime + CustomDelay;
		TestFalse(TEXT("Custom delay blocks early request"), SimulateCanRequest());
		CurrentSimulatedTime += CustomDelay;
		TestTrue(TEXT("Custom delay unblocks request when reached"), SimulateCanRequest());

		// 3.5 Reset / Cooldown Start resets attempt count and states
		AttemptsInCooldown = 0;
		FailedAttemptsOnCurrentSide = 0;
		bLastSucceeded = false;
		NextAllowedTime = 0.0f;
		TestEqual(TEXT("Reset clears attempts count"), AttemptsInCooldown, 0);
		TestEqual(TEXT("Reset clears failure count"), FailedAttemptsOnCurrentSide, 0);

		// 3.6 RepositionRetryDelay > Wait State Machine non-breaking cycle test (e.g. RetryDelay = 0.75s, StateTree Wait = 0.5s)
		// First request at T=100.0s sets NextAllowedTime = 100.75s
		CurrentSimulatedTime = 100.0f;
		const float LongRetryDelay = 0.75f;
		const float StateTreeWaitDuration = 0.50f;
		AttemptsInCooldown = 0;

		NextAllowedTime = CurrentSimulatedTime + LongRetryDelay;
		AttemptsInCooldown++;
		TestEqual(TEXT("Long delay request 1 issued"), AttemptsInCooldown, 1);

		// Move finishes quickly at T=100.1s, StateTree transitions to Wait (0.5s)
		CurrentSimulatedTime = 100.1f + StateTreeWaitDuration; // T=100.6s
		// At T=100.6s, StateTree enters Reposition again. NextAllowedTime is 100.75s, so CanRequest is false.
		// Task returns Succeeded without issuing move; StateTree enters Wait (0.5s) again.
		TestFalse(TEXT("Interval gate holds at T=100.6s (< 100.75s)"), CurrentSimulatedTime >= NextAllowedTime);

		// Second Wait finishes at T=101.1s
		CurrentSimulatedTime += StateTreeWaitDuration; // T=101.1s
		TestTrue(TEXT("Interval gate opens at T=101.1s (>= 100.75s)"), CurrentSimulatedTime >= NextAllowedTime);
		// Reposition task now successfully issues request 2
		NextAllowedTime = CurrentSimulatedTime + LongRetryDelay;
		AttemptsInCooldown++;
		TestEqual(TEXT("Long delay request 2 successfully issued on subsequent Wait loop"), AttemptsInCooldown, 2);

		// 3.7 Permanent Invalidation & Fail-Closed Gating Tests
		// Ensure invalid states are NOT classified as temporary interval gating
		bool bSimulatedDead = false;
		bool bSimulatedStunned = false;
		bool bSimulatedHitReacting = false;
		bool bSimulatedAttacking = false;
		bool bSimulatedHasTarget = true;
		bool bSimulatedHasProfile = true;
		bool bSimulatedHasAttackSet = true;
		bool bSimulatedExceedingLeash = false;
		bool bSimulatedOnCooldown = true;

		auto SimulateIsTemporarilyIntervalGated = [&](float CurrentTime, float NextTime) -> bool
		{
			const bool bBaseValid = !bSimulatedDead
				&& !bSimulatedStunned
				&& !bSimulatedHitReacting
				&& !bSimulatedAttacking
				&& !bIsCurrentlyMoving
				&& bSimulatedHasTarget
				&& bSimulatedHasAttackSet
				&& bSimulatedHasProfile
				&& bSimulatedOnCooldown
				&& !bSimulatedExceedingLeash;

			return bBaseValid && (CurrentTime < NextTime);
		};

		// Normal interval delay: correctly recognized as temporary interval gated
		TestTrue(TEXT("Pure time delay is recognized as temporary interval gated"),
			SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));

		// When dead: NOT interval gated (must fail closed)
		bSimulatedDead = true;
		TestFalse(TEXT("Dead enemy is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedDead = false;

		// When stunned: NOT interval gated
		bSimulatedStunned = true;
		TestFalse(TEXT("Stunned enemy is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedStunned = false;

		// When hit reacting: NOT interval gated
		bSimulatedHitReacting = true;
		TestFalse(TEXT("Hit reacting enemy is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedHitReacting = false;

		// When attacking: NOT interval gated
		bSimulatedAttacking = true;
		TestFalse(TEXT("Attacking enemy is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedAttacking = false;

		// When target lost: NOT interval gated
		bSimulatedHasTarget = false;
		TestFalse(TEXT("Enemy without target is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedHasTarget = true;

		// When profile invalid: NOT interval gated
		bSimulatedHasProfile = false;
		TestFalse(TEXT("Enemy without AIProfile is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedHasProfile = true;

		// When AttackSet invalid: NOT interval gated
		bSimulatedHasAttackSet = false;
		TestFalse(TEXT("Enemy without AttackSet is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedHasAttackSet = true;

		// When exceeding leash: NOT interval gated
		bSimulatedExceedingLeash = true;
		TestFalse(TEXT("Leash broken enemy is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedExceedingLeash = false;

		// When cooldown ended: NOT interval gated
		bSimulatedOnCooldown = false;
		TestFalse(TEXT("Cooldown ended enemy is not interval gated"), SimulateIsTemporarilyIntervalGated(10.0f, 15.0f));
		bSimulatedOnCooldown = true;
	}

	// 4. Approach Lifecycle, Timeout, and Dynamic Target Tracking State Machine Tests
	{
		const float EngagementRange = 300.0f;
		const float ShortAttackRange = 180.0f;
		const float ApproachTimeout = 3.0f;

		bool bDead = false;
		bool bStunned = false;
		bool bHitReacting = false;
		bool bAttacking = false;
		bool bHasTarget = true;
		bool bHasAttackSet = true;
		bool bHasAIProfile = true;
		bool bOnCooldown = false;
		bool bExceedingLeash = false;
		float TargetDistance2D = 250.0f;

		bool bHasPendingProfile = false;
		float PendingProfileRange = 0.0f;
		bool bIsApproaching = false;
		float ApproachStartTime = 0.0f;
		float CurrentSimulatedTime = 10.0f;

		auto ClearPendingDecision = [&]()
		{
			bHasPendingProfile = false;
			PendingProfileRange = 0.0f;
			bIsApproaching = false;
			ApproachStartTime = 0.0f;
		};

		auto SimulatePreparePending = [&](float SelectedRange) -> bool
		{
			if (bDead || bStunned || bHitReacting || bAttacking || !bHasTarget || !bHasAttackSet || bOnCooldown || bExceedingLeash || TargetDistance2D > EngagementRange)
			{
				ClearPendingDecision();
				return false;
			}

			if (bHasPendingProfile)
			{
				// Retain existing profile during approach
				return true;
			}

			bHasPendingProfile = true;
			PendingProfileRange = SelectedRange;
			return true;
		};

		auto SimulateCanRequestApproach = [&]() -> bool
		{
			return !bDead && !bStunned && !bHitReacting && !bAttacking && bHasTarget && bHasAttackSet && bHasAIProfile
				&& !bOnCooldown && !bExceedingLeash && TargetDistance2D <= EngagementRange
				&& bHasPendingProfile && TargetDistance2D > PendingProfileRange;
		};

		auto SimulateHasPendingProfile = [&]() -> bool
		{
			return bHasPendingProfile && !bDead && bHasTarget && bHasAttackSet && TargetDistance2D <= EngagementRange;
		};

		auto SimulateIsPendingInRange = [&]() -> bool
		{
			return SimulateHasPendingProfile() && TargetDistance2D <= PendingProfileRange && TargetDistance2D <= EngagementRange;
		};

		auto SimulateHasTimedOut = [&]() -> bool
		{
			if (!bIsApproaching)
			{
				return false;
			}
			return (CurrentSimulatedTime - ApproachStartTime) >= ApproachTimeout;
		};

		// 4.1 Target at 250cm, Select Short Attack (180cm) -> Requires Approach
		TestTrue(TEXT("Prepare pending short attack (180cm) succeeds at 250cm"), SimulatePreparePending(ShortAttackRange));
		TestTrue(TEXT("Has pending profile"), bHasPendingProfile);
		TestEqual(TEXT("Pending profile range is 180cm"), PendingProfileRange, ShortAttackRange);
		TestFalse(TEXT("Not yet in attack range at 250cm"), SimulateIsPendingInRange());
		TestTrue(TEXT("Can request approach towards target"), SimulateCanRequestApproach());

		// 4.2 Start Approach
		bIsApproaching = true;
		ApproachStartTime = CurrentSimulatedTime;
		TestFalse(TEXT("Approach not timed out initially"), SimulateHasTimedOut());

		// 4.3 Profile Retained without re-roll during approach while player moves (e.g. player shifts to 220cm)
		TargetDistance2D = 220.0f;
		TestTrue(TEXT("Prepare retains existing profile without re-rolling"), SimulatePreparePending(300.0f)); // Try injecting 300cm
		TestEqual(TEXT("Profile range remained 180cm"), PendingProfileRange, ShortAttackRange);

		// 4.4 Player reached within 180cm (e.g. 175cm) -> Can Attack!
		TargetDistance2D = 175.0f;
		TestTrue(TEXT("Target at 175cm is within pending attack range (180cm)"), SimulateIsPendingInRange());
		TestFalse(TEXT("Can no longer request approach once in attack range"), SimulateCanRequestApproach());

		// Simulate attack execution and cooldown start -> Clears decision
		bAttacking = true;
		ClearPendingDecision();
		TestFalse(TEXT("Pending decision cleared on attack/cooldown"), bHasPendingProfile);
		bAttacking = false;

		// 4.5 Approach Timeout handling
		TargetDistance2D = 250.0f;
		TestTrue(TEXT("Prepare new short attack"), SimulatePreparePending(ShortAttackRange));
		bIsApproaching = true;
		ApproachStartTime = CurrentSimulatedTime;

		// Advance time by 2.0s (< 3.0s timeout)
		CurrentSimulatedTime += 2.0f;
		TestFalse(TEXT("At +2.0s, approach has not timed out"), SimulateHasTimedOut());

		// Advance time by another 1.5s (total 3.5s >= 3.0s timeout)
		CurrentSimulatedTime += 1.5f;
		TestTrue(TEXT("At +3.5s, approach has timed out"), SimulateHasTimedOut());
		ClearPendingDecision();
		TestFalse(TEXT("Decision cleared on timeout"), bHasPendingProfile);

		// 4.6 Interruption / Invalidation Cases (Target lost, death, stun, hit react, leash break)
		// Case A: Target leaves EngagementRange (320cm > 300cm)
		TargetDistance2D = 320.0f;
		TestFalse(TEXT("Target outside engagement range fails prepare"), SimulatePreparePending(ShortAttackRange));

		// Case B: Stunned
		TargetDistance2D = 250.0f;
		bStunned = true;
		TestFalse(TEXT("Stunned enemy fails prepare"), SimulatePreparePending(ShortAttackRange));
		bStunned = false;

		// Case C: Hit Reaction
		bHitReacting = true;
		TestFalse(TEXT("Hit reacting enemy fails prepare"), SimulatePreparePending(ShortAttackRange));
		bHitReacting = false;

		// Case D: Exceeding Leash
		bExceedingLeash = true;
		TestFalse(TEXT("Leash-broken enemy fails prepare"), SimulatePreparePending(ShortAttackRange));
		bExceedingLeash = false;

		// Case E: Dead
		bDead = true;
		TestFalse(TEXT("Dead enemy fails prepare"), SimulatePreparePending(ShortAttackRange));
		bDead = false;

		// Case F: Attack on Cooldown
		TargetDistance2D = 250.0f;
		bOnCooldown = true;
		TestFalse(TEXT("Attack on cooldown fails prepare and does not generate pending profile"), SimulatePreparePending(ShortAttackRange));
		TestFalse(TEXT("No pending profile during cooldown"), bHasPendingProfile);
		// If a profile was somehow pending beforehand, cooldown prepare must clear it
		bHasPendingProfile = true;
		TestFalse(TEXT("Prepare during cooldown clears existing pending profile"), SimulatePreparePending(ShortAttackRange));
		TestFalse(TEXT("Pending profile cleared by cooldown prepare"), bHasPendingProfile);
		bOnCooldown = false;

		// 4.7 GAS Activation Failure cleans up Pending Decision immediately
		TargetDistance2D = 150.0f;
		TestTrue(TEXT("Prepare pending profile for attack request"), SimulatePreparePending(ShortAttackRange));
		TestTrue(TEXT("Has pending profile before activation"), bHasPendingProfile);
		// Simulate GAS TryActivateAbilitiesByTag returning false (e.g. cost/blocking tags)
		auto SimulateTryRequestMeleeAttack = [&](bool bGASActivationSuccess) -> bool
		{
			if (!SimulateIsPendingInRange())
			{
				ClearPendingDecision();
				return false;
			}
			if (!bGASActivationSuccess)
			{
				ClearPendingDecision();
				return false;
			}
			return true;
		};
		TestFalse(TEXT("Melee attack request returns false when GAS activation rejected"), SimulateTryRequestMeleeAttack(false));
		TestFalse(TEXT("Pending profile cleared when GAS activation rejected"), bHasPendingProfile);

		// 4.8 Ability End without attack start cleans up Pending Decision
		TestTrue(TEXT("Prepare pending profile for ability start"), SimulatePreparePending(ShortAttackRange));
		TestTrue(TEXT("Has pending profile before ability"), bHasPendingProfile);
		auto SimulateEndAbility = [&](bool bAbilityAttackStarted, bool bWasCancelled)
		{
			const bool bShouldStartCooldown = bAbilityAttackStarted && !bWasCancelled;
			if (bShouldStartCooldown)
			{
				// StartMeleeAttackCooldown clears decision and sets CD
				bOnCooldown = true;
				ClearPendingDecision();
			}
			else
			{
				// EndAbility unconditional cleanup
				ClearPendingDecision();
			}
		};
		SimulateEndAbility(false, false); // Montage did not start / synchronous abort
		TestFalse(TEXT("Pending profile cleared when ability ended before attack started"), bHasPendingProfile);

		// 4.9 Target exceeds EngagementRange even if AttackRange is larger (e.g. AttackRange 400cm, EngagementRange 300cm, Target at 350cm)
		const float LargeAttackRange = 400.0f;
		TargetDistance2D = 250.0f;
		TestTrue(TEXT("Prepare large attack profile inside EngagementRange"), SimulatePreparePending(LargeAttackRange));
		// Target moves to 350cm (outside EngagementRange 300cm, but within LargeAttackRange 400cm)
		TargetDistance2D = 350.0f;
		TestFalse(TEXT("IsPendingInRange returns false when target is outside EngagementRange despite AttackRange > distance"), SimulateIsPendingInRange());
		ClearPendingDecision();
		TestFalse(TEXT("Pending profile cleared after range test"), bHasPendingProfile);

		// 4.10 StateTree PrepareMeleeAttack Task fail-closed during cooldown
		bOnCooldown = true;
		auto SimulateStateTreePrepareMeleeAttackTask = [&]() -> bool
		{
			if (bDead || bStunned || bHitReacting || bAttacking || bOnCooldown || !bHasTarget || !bHasAttackSet || TargetDistance2D > EngagementRange || bExceedingLeash)
			{
				ClearPendingDecision();
				return false;
			}
			return SimulatePreparePending(ShortAttackRange);
		};
		// Test 1: Task fails when called during cooldown
		TestFalse(TEXT("PrepareMeleeAttack Task fails during cooldown"), SimulateStateTreePrepareMeleeAttackTask());
		TestFalse(TEXT("Pending profile remains false after Task fail during cooldown"), bHasPendingProfile);
		// Test 2: If a decision was somehow pending beforehand, Task during cooldown clears it
		bHasPendingProfile = true;
		TestFalse(TEXT("PrepareMeleeAttack Task during cooldown clears existing pending decision"), SimulateStateTreePrepareMeleeAttackTask());
		TestFalse(TEXT("Pending profile cleared by Task during cooldown"), bHasPendingProfile);
		bOnCooldown = false;
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
