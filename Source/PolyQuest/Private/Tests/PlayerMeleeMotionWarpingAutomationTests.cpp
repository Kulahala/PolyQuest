// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "AbilitySystem/Abilities/LightAttackAbility.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/ComboChainDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Tests/CombatAutomationFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerMeleeMotionWarpingAutomationTest, "PolyQuest.Combat.PlayerMeleeMotionWarping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerMeleeMotionWarpingAutomationTest::RunTest(const FString&)
{
	// -------------------------------------------------------------------------
	// SECTION 1: Pure Geometric Evaluator Matrix (ULightAttackAbility::EvaluateMeleeMotionWarpTransform)
	// -------------------------------------------------------------------------
	{
		FComboChainEntry ValidEntry;
		ValidEntry.bUseMotionWarping = true;
		ValidEntry.WarpTargetName = FName(TEXT("MeleeContact"));
		ValidEntry.WarpStopDistance = 100.0f;
		ValidEntry.MaxWarpDistance = 60.0f;
		ValidEntry.MaxWarpAngleDegrees = 60.0f;

		FTransform OutTransform;

		// 1.1 Config defaults / disabled / invalid names
		{
			const FComboChainEntry DefaultConstructedEntry;
			TestFalse(TEXT("Default bUseMotionWarping is false"), DefaultConstructedEntry.bUseMotionWarping);
			TestEqual(TEXT("Default WarpTargetName is MeleeContact"), DefaultConstructedEntry.WarpTargetName, FName(TEXT("MeleeContact")));
			TestEqual(TEXT("Default WarpStopDistance is 190.0f"), DefaultConstructedEntry.WarpStopDistance, 190.0f);
			TestEqual(TEXT("Default MaxWarpDistance is 110.0f"), DefaultConstructedEntry.MaxWarpDistance, 110.0f);
			TestEqual(TEXT("Default MaxWarpAngleDegrees is 60.0f"), DefaultConstructedEntry.MaxWarpAngleDegrees, 60.0f);

			FComboChainEntry DisabledEntry = ValidEntry;
			DisabledEntry.bUseMotionWarping = false;
			TestFalse(TEXT("Disabled entry fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, DisabledEntry, OutTransform));

			FComboChainEntry NoneNameEntry = ValidEntry;
			NoneNameEntry.WarpTargetName = NAME_None;
			TestFalse(TEXT("NAME_None warp target fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, NoneNameEntry, OutTransform));
		}

		// 1.2 Config non-finite and out-of-range boundaries
		{
			constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
			constexpr float Inf = std::numeric_limits<float>::infinity();

			FComboChainEntry BadConfig = ValidEntry;
			BadConfig.WarpStopDistance = -10.0f;
			TestFalse(TEXT("Negative WarpStopDistance fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, BadConfig, OutTransform));

			BadConfig = ValidEntry;
			BadConfig.MaxWarpDistance = -5.0f;
			TestFalse(TEXT("Negative MaxWarpDistance fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, BadConfig, OutTransform));

			BadConfig = ValidEntry;
			BadConfig.MaxWarpAngleDegrees = -1.0f;
			TestFalse(TEXT("Negative MaxWarpAngleDegrees fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, BadConfig, OutTransform));

			BadConfig = ValidEntry;
			BadConfig.MaxWarpAngleDegrees = 181.0f;
			TestFalse(TEXT("MaxWarpAngleDegrees > 180 fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, BadConfig, OutTransform));

			BadConfig = ValidEntry;
			BadConfig.WarpStopDistance = NaN;
			TestFalse(TEXT("NaN WarpStopDistance fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, BadConfig, OutTransform));

			BadConfig = ValidEntry;
			BadConfig.MaxWarpDistance = Inf;
			TestFalse(TEXT("Inf MaxWarpDistance fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, BadConfig, OutTransform));
		}

		// 1.3 Ground state requirement
		{
			TestFalse(TEXT("Player airborne fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), false,
					FVector(140, 0, 0), true, ValidEntry, OutTransform));

			TestFalse(TEXT("Target airborne fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), false, ValidEntry, OutTransform));

			TestFalse(TEXT("Both airborne fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), false,
					FVector(140, 0, 0), false, ValidEntry, OutTransform));
		}

		// 1.4 Coordinate finite checks & Zero distance
		{
			constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

			TestFalse(TEXT("Player location with NaN fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(NaN, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, ValidEntry, OutTransform));

			TestFalse(TEXT("Target location with NaN fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, NaN, 0), true, ValidEntry, OutTransform));

			TestFalse(TEXT("Player forward with NaN fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(NaN, 0, 0), true,
					FVector(140, 0, 0), true, ValidEntry, OutTransform));

			TestFalse(TEXT("Coincident 2D locations fail closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(50, 50, 0), FVector(1, 0, 0), true,
					FVector(50, 50, 100), true, ValidEntry, OutTransform));
		}

		// 1.5 Distance boundaries (WarpStopDistance=100cm, MaxWarpDistance=60cm)
		{
			// Target distance <= 100cm -> rejected (<= rejected)
			TestFalse(TEXT("Target distance 50cm <= 100cm fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(50, 0, 0), true, ValidEntry, OutTransform));

			TestFalse(TEXT("Target distance 100cm <= 100cm fails closed (exact stop distance boundary)"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(100, 0, 0), true, ValidEntry, OutTransform));

			// Target distance = 140cm -> WarpLocation = (40, 0, 0), correction = 40cm <= 60cm -> Accepted
			TestTrue(TEXT("Target distance 140cm is accepted"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 50), FVector(1, 0, 0), true,
					FVector(140, 0, 120), true, ValidEntry, OutTransform));
			TestTrue(TEXT("WarpLocation X matches expected stop distance offset"),
				FMath::IsNearlyEqual(OutTransform.GetLocation().X, 40.0f, 0.1f));
			TestTrue(TEXT("WarpLocation Y is 0"),
				FMath::IsNearlyEqual(OutTransform.GetLocation().Y, 0.0f, 0.1f));
			TestTrue(TEXT("WarpLocation Z strictly preserves Player Z"),
				FMath::IsNearlyEqual(OutTransform.GetLocation().Z, 50.0f, 0.1f));
			TestTrue(TEXT("WarpRotation Yaw is 0 facing target"),
				FMath::IsNearlyEqual(OutTransform.Rotator().Yaw, 0.0f, 0.1f));

			// Target distance = 160cm -> WarpLocation = (60, 0, 0), correction = 60cm <= 60cm -> Accepted (exact boundary)
			TestTrue(TEXT("Target distance 160cm (exact max warp distance 60cm) is accepted"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(160, 0, 0), true, ValidEntry, OutTransform));
			TestTrue(TEXT("WarpLocation X is 60cm at max boundary"),
				FMath::IsNearlyEqual(OutTransform.GetLocation().X, 60.0f, 0.1f));

			// Target distance = 160.5cm -> correction = 60.5cm > 60cm -> Rejected (> rejected)
			TestFalse(TEXT("Target distance 160.5cm (> 60cm correction) fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(160.5f, 0, 0), true, ValidEntry, OutTransform));

			TestFalse(TEXT("Target distance 200cm (> 60cm correction) fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(200, 0, 0), true, ValidEntry, OutTransform));
		}

		// 1.6 Angle boundaries (MaxWarpAngleDegrees = 60.0f)
		{
			// Target at 45 deg, distance = 140cm
			const float TargetX_45 = 140.0f * FMath::Cos(FMath::DegreesToRadians(45.0f));
			const float TargetY_45 = 140.0f * FMath::Sin(FMath::DegreesToRadians(45.0f));
			TestTrue(TEXT("Target at 45 deg <= 60 deg is accepted"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(TargetX_45, TargetY_45, 0), true, ValidEntry, OutTransform));
			TestTrue(TEXT("WarpRotation Yaw matches 45 deg target vector"),
				FMath::IsNearlyEqual(OutTransform.Rotator().Yaw, 45.0f, 0.1f));

			// Target at 60 deg (exact boundary <= 60 deg)
			const float TargetX_60 = 140.0f * FMath::Cos(FMath::DegreesToRadians(60.0f));
			const float TargetY_60 = 140.0f * FMath::Sin(FMath::DegreesToRadians(60.0f));
			TestTrue(TEXT("Target at 60 deg (exact boundary) is accepted"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(TargetX_60, TargetY_60, 0), true, ValidEntry, OutTransform));
			TestTrue(TEXT("WarpRotation Yaw matches 60 deg target vector"),
				FMath::IsNearlyEqual(OutTransform.Rotator().Yaw, 60.0f, 0.1f));

			// Target at 60.5 deg (> 60 deg) -> rejected
			const float TargetX_60_5 = 140.0f * FMath::Cos(FMath::DegreesToRadians(60.5f));
			const float TargetY_60_5 = 140.0f * FMath::Sin(FMath::DegreesToRadians(60.5f));
			TestFalse(TEXT("Target at 60.5 deg > 60 deg fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(TargetX_60_5, TargetY_60_5, 0), true, ValidEntry, OutTransform));

			// Target at 90 deg -> rejected
			TestFalse(TEXT("Target at 90 deg fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(0, 140, 0), true, ValidEntry, OutTransform));

			// Target behind (180 deg) -> rejected
			TestFalse(TEXT("Target behind at 180 deg fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(-140, 0, 0), true, ValidEntry, OutTransform));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Player Component & C++ Bridge Lifecycle
	// -------------------------------------------------------------------------
	if (GEngine)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerMeleeMotionWarpingTestWorld"));
		WorldContext.SetCurrentWorld(World);

		if (TestNotNull(TEXT("Transient test world created"), World))
		{
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

			APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0, 0, 100)));
			if (TestNotNull(TEXT("Player spawned in test world"), Player))
			{
				// 2.1 Motion Warping component ownership and montage search setting
				const UMotionWarpingComponent* WarpComp = Player->FindComponentByClass<UMotionWarpingComponent>();
				if (TestNotNull(TEXT("Player owns UMotionWarpingComponent"), WarpComp))
				{
					TestFalse(TEXT("bSearchForWindowsInAnimsWithinMontages is false"),
						WarpComp->bSearchForWindowsInAnimsWithinMontages);
				}

				// 2.2 Bridge methods: SetMeleeMotionWarpTarget & ClearMeleeMotionWarpTargets
				const FName TargetName(TEXT("MeleeContact"));
				const FTransform TestWarpTransform(FRotator(0.0f, 45.0f, 0.0f), FVector(50.0f, 20.0f, 100.0f));

				TestFalse(TEXT("SetMeleeMotionWarpTarget with NAME_None fails"),
					Player->SetMeleeMotionWarpTarget(NAME_None, TestWarpTransform));

				constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
				const FTransform NonFiniteTransform(FRotator::ZeroRotator, FVector(NaN, 0, 0));
				TestFalse(TEXT("SetMeleeMotionWarpTarget with non-finite transform fails"),
					Player->SetMeleeMotionWarpTarget(TargetName, NonFiniteTransform));

				TestTrue(TEXT("SetMeleeMotionWarpTarget with valid transform succeeds"),
					Player->SetMeleeMotionWarpTarget(TargetName, TestWarpTransform));

				FTransform StoredTransform;
				TestTrue(TEXT("Player has recorded MeleeContact warp target"),
					Player->HasTestMeleeMotionWarpTarget(TargetName, &StoredTransform));
				TestTrue(TEXT("Recorded warp location matches input"),
					StoredTransform.GetLocation().Equals(TestWarpTransform.GetLocation(), 0.1f));
				TestTrue(TEXT("Recorded warp rotation matches input"),
					StoredTransform.Rotator().Equals(TestWarpTransform.Rotator(), 0.1f));
				TestEqual(TEXT("Warp target count is 1"),
					Player->GetTestMeleeMotionWarpTargetCount(), 1);

				// 2.3 Clear targets
				Player->ClearMeleeMotionWarpTargets();
				TestEqual(TEXT("ClearMeleeMotionWarpTargets clears all targets"),
					Player->GetTestMeleeMotionWarpTargetCount(), 0);
				TestFalse(TEXT("HasTestMeleeMotionWarpTarget returns false after clear"),
					Player->HasTestMeleeMotionWarpTarget(TargetName));

				// 2.4 UnPossessed clears targets
				Player->SetMeleeMotionWarpTarget(TargetName, TestWarpTransform);
				TestEqual(TEXT("Warp target re-added"), Player->GetTestMeleeMotionWarpTargetCount(), 1);
				Player->TriggerTestUnPossessed();
				TestEqual(TEXT("UnPossessed clears warp targets"),
					Player->GetTestMeleeMotionWarpTargetCount(), 0);

				// 2.5 OnMovementModeChanged (Falling) clears targets
				Player->SetMeleeMotionWarpTarget(TargetName, TestWarpTransform);
				TestEqual(TEXT("Warp target re-added before fall"), Player->GetTestMeleeMotionWarpTargetCount(), 1);
				if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
				{
					MoveComp->SetMovementMode(MOVE_Falling);
				}
				Player->TriggerTestOnMovementModeChanged(MOVE_Walking, 0);
				TestEqual(TEXT("Falling movement mode clears warp targets"),
					Player->GetTestMeleeMotionWarpTargetCount(), 0);

				// 2.6 EndPlay clears targets
				Player->SetMeleeMotionWarpTarget(TargetName, TestWarpTransform);
				TestEqual(TEXT("Warp target re-added before EndPlay"), Player->GetTestMeleeMotionWarpTargetCount(), 1);
				Player->TriggerTestEndPlay(EEndPlayReason::Destroyed);
				TestEqual(TEXT("EndPlay clears warp targets"),
					Player->GetTestMeleeMotionWarpTargetCount(), 0);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
