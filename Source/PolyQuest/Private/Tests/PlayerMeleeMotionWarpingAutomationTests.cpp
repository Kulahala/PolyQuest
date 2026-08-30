// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "AbilitySystem/Abilities/ChargedAttackAbility.h"
#include "AbilitySystem/Abilities/LightAttackAbility.h"
#include "AbilitySystem/Abilities/SprintAttackAbility.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/ComboChainDataAsset.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
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

			const FMeleeMotionWarpConfig DefaultSharedConfig;
			TestFalse(TEXT("Default shared config bUseMotionWarping is false"), DefaultSharedConfig.bUseMotionWarping);
			TestEqual(TEXT("Default shared config WarpTargetName is MeleeContact"), DefaultSharedConfig.WarpTargetName, FName(TEXT("MeleeContact")));
			TestEqual(TEXT("Default shared config WarpStopDistance is 190.0f"), DefaultSharedConfig.WarpStopDistance, 190.0f);
			TestEqual(TEXT("Default shared config MaxWarpDistance is 110.0f"), DefaultSharedConfig.MaxWarpDistance, 110.0f);
			TestEqual(TEXT("Default shared config MaxWarpAngleDegrees is 60.0f"), DefaultSharedConfig.MaxWarpAngleDegrees, 60.0f);

			FComboChainEntry DisabledEntry = ValidEntry;
			DisabledEntry.bUseMotionWarping = false;
			TestFalse(TEXT("Disabled entry fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, DisabledEntry, OutTransform));

			FMeleeMotionWarpConfig SharedDisabledConfig;
			SharedDisabledConfig.bUseMotionWarping = false;
			SharedDisabledConfig.WarpTargetName = FName(TEXT("MeleeContact"));
			SharedDisabledConfig.WarpStopDistance = 100.0f;
			SharedDisabledConfig.MaxWarpDistance = 60.0f;
			SharedDisabledConfig.MaxWarpAngleDegrees = 60.0f;
			FTransform SharedOutTransform;
			TestFalse(TEXT("Disabled shared config fails closed"),
				FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, SharedDisabledConfig, SharedOutTransform));

			FComboChainEntry NoneNameEntry = ValidEntry;
			NoneNameEntry.WarpTargetName = NAME_None;
			TestFalse(TEXT("NAME_None warp target fails closed"),
				ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
					FVector(0, 0, 0), FVector(1, 0, 0), true,
					FVector(140, 0, 0), true, NoneNameEntry, OutTransform));

			// Direct equivalence assertion between Light wrapper and shared evaluator
			FTransform LightTransform;
			FTransform HelperTransform;
			const bool bLightResult = ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
				FVector(0, 0, 0), FVector(1, 0, 0), true,
				FVector(140, 0, 0), true, ValidEntry, LightTransform);
			FMeleeMotionWarpConfig ValidSharedConfig;
			ValidSharedConfig.bUseMotionWarping = ValidEntry.bUseMotionWarping;
			ValidSharedConfig.WarpTargetName = ValidEntry.WarpTargetName;
			ValidSharedConfig.WarpStopDistance = ValidEntry.WarpStopDistance;
			ValidSharedConfig.MaxWarpDistance = ValidEntry.MaxWarpDistance;
			ValidSharedConfig.MaxWarpAngleDegrees = ValidEntry.MaxWarpAngleDegrees;
			const bool bHelperResult = FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
				FVector(0, 0, 0), FVector(1, 0, 0), true,
				FVector(140, 0, 0), true, ValidSharedConfig, HelperTransform);
			TestTrue(TEXT("Light wrapper and shared helper produce identical true result"), bLightResult && bHelperResult);
			TestEqual(TEXT("Light wrapper and shared helper produce identical transform location"), LightTransform.GetLocation(), HelperTransform.GetLocation());
			TestEqual(TEXT("Light wrapper and shared helper produce identical transform rotation"), LightTransform.GetRotation(), HelperTransform.GetRotation());
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

	// -------------------------------------------------------------------------
	// SECTION 3: LightAttackAbility Light Combo Motion-Warp Adoption Matrix
	// -------------------------------------------------------------------------
	if (GEngine)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerMeleeMotionWarpingAbilityTestWorld"));
		WorldContext.SetCurrentWorld(World);

		if (TestNotNull(TEXT("Transient ability test world created"), World))
		{
			const FURL WorldURL;
			World->InitializeActorsForPlay(WorldURL);
			World->BeginPlay();

			struct FTestScopeCleanup
			{
				UWorld* WorldToDestroy = nullptr;
				ELogVerbosity::Type OriginalVerbosity = ELogVerbosity::Log;

				~FTestScopeCleanup()
				{
					LogAbilitySystem.SetVerbosity(OriginalVerbosity);
					if (WorldToDestroy)
					{
						GEngine->DestroyWorldContext(WorldToDestroy);
						WorldToDestroy->DestroyWorld(false);
					}
				}
			} ScopeCleanup{ World, LogAbilitySystem.GetVerbosity() };

			LogAbilitySystem.SetVerbosity(ELogVerbosity::Error);

			APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0, 0, 100)));
			APlayerController* PlayerController = World->SpawnActor<APlayerController>();
			AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(140, 0, 100)));

			if (TestNotNull(TEXT("Player spawned for ability test"), Player)
				&& TestNotNull(TEXT("PlayerController spawned for ability test"), PlayerController)
				&& TestNotNull(TEXT("Enemy spawned for ability test"), Enemy))
			{
				PlayerController->Possess(Player);
				Player->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false));
				Enemy->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false));

				if (UCharacterMovementComponent* PlayerMoveComp = Player->GetCharacterMovement())
				{
					PlayerMoveComp->SetMovementMode(MOVE_Walking);
				}
				if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
				{
					EnemyMoveComp->SetMovementMode(MOVE_Walking);
				}

				// Lock-on projection hook returning true with distinct screen positions so ResolveValidLockedTarget succeeds
				Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
				{
					OutViewportSize = FVector2D(1920.0f, 1080.0f);
					if (WorldPoint.X > 50.0f)
					{
						OutScreenPosition = FVector2D(1160.0f, 540.0f);
					}
					else
					{
						OutScreenPosition = FVector2D(960.0f, 540.0f);
					}
					return true;
				});

				// Create 4 distinct test montages for combo definition
				UAnimMontage* Montage0 = NewObject<UAnimMontage>(World, TEXT("TestMontage0"));
				UAnimMontage* Montage1 = NewObject<UAnimMontage>(World, TEXT("TestMontage1"));
				UAnimMontage* Montage2 = NewObject<UAnimMontage>(World, TEXT("TestMontage2"));
				UAnimMontage* Montage3 = NewObject<UAnimMontage>(World, TEXT("TestMontage3"));

				UComboChainDataAsset* ComboAsset = NewObject<UComboChainDataAsset>(World);
				ComboAsset->Entries.SetNum(4);

				// Entry 0 (opt-in)
				ComboAsset->Entries[0].Montage = Montage0;
				ComboAsset->Entries[0].bUseMotionWarping = true;
				ComboAsset->Entries[0].WarpTargetName = FName(TEXT("MeleeContact_0"));
				ComboAsset->Entries[0].WarpStopDistance = 100.0f;
				ComboAsset->Entries[0].MaxWarpDistance = 60.0f;
				ComboAsset->Entries[0].MaxWarpAngleDegrees = 60.0f;

				// Entry 1 (opt-in, distinct params)
				ComboAsset->Entries[1].Montage = Montage1;
				ComboAsset->Entries[1].bUseMotionWarping = true;
				ComboAsset->Entries[1].WarpTargetName = FName(TEXT("MeleeContact_1"));
				ComboAsset->Entries[1].WarpStopDistance = 90.0f;
				ComboAsset->Entries[1].MaxWarpDistance = 70.0f;
				ComboAsset->Entries[1].MaxWarpAngleDegrees = 45.0f;

				// Entry 2 (opt-in, distinct params)
				ComboAsset->Entries[2].Montage = Montage2;
				ComboAsset->Entries[2].bUseMotionWarping = true;
				ComboAsset->Entries[2].WarpTargetName = FName(TEXT("MeleeContact_2"));
				ComboAsset->Entries[2].WarpStopDistance = 80.0f;
				ComboAsset->Entries[2].MaxWarpDistance = 80.0f;
				ComboAsset->Entries[2].MaxWarpAngleDegrees = 30.0f;

				// Entry 3 (opt-in, but beyond allowed 0..2 index)
				ComboAsset->Entries[3].Montage = Montage3;
				ComboAsset->Entries[3].bUseMotionWarping = true;
				ComboAsset->Entries[3].WarpTargetName = FName(TEXT("MeleeContact_3"));
				ComboAsset->Entries[3].WarpStopDistance = 80.0f;
				ComboAsset->Entries[3].MaxWarpDistance = 80.0f;
				ComboAsset->Entries[3].MaxWarpAngleDegrees = 30.0f;

				ULightAttackAbility* Ability = NewObject<ULightAttackAbility>(Player);
				UAnimInstance* TestAnimInstance = NewObject<UAnimInstance>(Player->GetMesh());
				Ability->SetTestActorInfo(Player->GetAbilitySystemComponent()->AbilityActorInfo.Get());
				Ability->SetTestComboDefinition(ComboAsset);
				Ability->SetTestBoundAnimInstance(TestAnimInstance);
				Ability->SetTestBypassMontageActiveCheck(true);

				// 3.1 Entry 0 / 1 / 2 success path with independent parameters
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Player->SetTestLockedTarget(Enemy);

					// Entry 0
					TestTrue(TEXT("StartComboEntry(0) succeeds"), Ability->TestStartComboEntry(0));
					TestTrue(TEXT("MeleeMotionWarpSnapshot attempted capture is true"), Ability->HasTestMeleeMotionWarpCaptureAttempted());
					TestTrue(TEXT("MeleeMotionWarpSnapshot has captured target"), Ability->HasTestMeleeMotionWarpSnapshot());
					TestTrue(TEXT("Captured location X matches Enemy initial location"), FMath::IsNearlyEqual(Ability->GetTestMeleeMotionWarpCapturedLocation().X, 140.0f, 0.1f));
					TestTrue(TEXT("Player has recorded MeleeContact_0"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0"))));

					FTransform StoredTransform0;
					Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), &StoredTransform0);
					TestTrue(TEXT("Entry 0 warp target X is 40cm (140 - 100)"), FMath::IsNearlyEqual(StoredTransform0.GetLocation().X, 40.0f, 0.1f));

					// Entry 1 (replaces Entry 0, clears old, writes new)
					TestTrue(TEXT("StartComboEntry(1) succeeds"), Ability->TestStartComboEntry(1));
					TestFalse(TEXT("Entry 0 target MeleeContact_0 was cleared"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0"))));
					TestTrue(TEXT("Player has recorded MeleeContact_1"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1"))));

					FTransform StoredTransform1;
					Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1")), &StoredTransform1);
					TestTrue(TEXT("Entry 1 warp target X is 50cm (140 - 90)"), FMath::IsNearlyEqual(StoredTransform1.GetLocation().X, 50.0f, 0.1f));

					// Entry 2 (replaces Entry 1, clears old, writes new)
					TestTrue(TEXT("StartComboEntry(2) succeeds"), Ability->TestStartComboEntry(2));
					TestFalse(TEXT("Entry 1 target MeleeContact_1 was cleared"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1"))));
					TestTrue(TEXT("Player has recorded MeleeContact_2"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_2"))));

					FTransform StoredTransform2;
					Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_2")), &StoredTransform2);
					TestTrue(TEXT("Entry 2 warp target X is 60cm (140 - 80)"), FMath::IsNearlyEqual(StoredTransform2.GetLocation().X, 60.0f, 0.1f));
				}

				// 3.2 Entry 3+ index gate fail-closed
				{
					TestTrue(TEXT("StartComboEntry(3) executes basic combo"), Ability->TestStartComboEntry(3));
					TestEqual(TEXT("Entry 3 does not write motion warp target (count == 0)"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
					TestFalse(TEXT("Player does not have MeleeContact_3 target"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_3"))));
				}

				// 3.3 Independent per-entry opt-in and clearing
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Player->SetTestLockedTarget(Enemy);

					ComboAsset->Entries[0].bUseMotionWarping = true;
					ComboAsset->Entries[1].bUseMotionWarping = false; // Disabled entry
					ComboAsset->Entries[2].bUseMotionWarping = true;

					TestTrue(TEXT("StartComboEntry(0) succeeds"), Ability->TestStartComboEntry(0));
					TestTrue(TEXT("Player has MeleeContact_0"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0"))));

					TestTrue(TEXT("StartComboEntry(1) succeeds"), Ability->TestStartComboEntry(1));
					TestEqual(TEXT("Disabled entry 1 clears previous target and leaves 0 targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

					TestTrue(TEXT("StartComboEntry(2) succeeds"), Ability->TestStartComboEntry(2));
					TestTrue(TEXT("Re-enabled entry 2 writes MeleeContact_2"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_2"))));

					// Restore Entry 1
					ComboAsset->Entries[1].bUseMotionWarping = true;
				}

				// 3.4 Disabled and illegal config do not consume one-shot capture
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Player->SetTestLockedTarget(nullptr); // Unlocked initially

					ComboAsset->Entries[0].bUseMotionWarping = false; // Disabled
					ComboAsset->Entries[1].bUseMotionWarping = true;

					TestTrue(TEXT("StartComboEntry(0) with disabled config succeeds"), Ability->TestStartComboEntry(0));
					TestFalse(TEXT("Disabled entry 0 did not attempt capture"), Ability->HasTestMeleeMotionWarpCaptureAttempted());

					// Now lock onto enemy before Entry 1
					Player->SetTestLockedTarget(Enemy);
					TestTrue(TEXT("StartComboEntry(1) becomes the first capture attempt"), Ability->TestStartComboEntry(1));
					TestTrue(TEXT("Entry 1 attempted capture is true"), Ability->HasTestMeleeMotionWarpCaptureAttempted());
					TestTrue(TEXT("Entry 1 captured target is valid"), Ability->HasTestMeleeMotionWarpSnapshot());
					TestTrue(TEXT("Entry 1 wrote warp target"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1"))));

					// Restore Entry 0
					ComboAsset->Entries[0].bUseMotionWarping = true;
				}

				// 3.5 First legal entry capture failure locks ability (no re-targeting on lock change)
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Player->SetTestLockedTarget(nullptr); // Unlocked on first legal entry

					TestTrue(TEXT("StartComboEntry(0) succeeds without target"), Ability->TestStartComboEntry(0));
					TestTrue(TEXT("Capture was attempted and failed"), Ability->HasTestMeleeMotionWarpCaptureAttempted());
					TestFalse(TEXT("No snapshot target recorded"), Ability->HasTestMeleeMotionWarpSnapshot());
					TestEqual(TEXT("Player has 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

					// Now player acquires a target during combo
					Player->SetTestLockedTarget(Enemy);

					TestTrue(TEXT("StartComboEntry(1) succeeds"), Ability->TestStartComboEntry(1));
					TestFalse(TEXT("Entry 1 does not re-target after prior capture failure"), Ability->HasTestMeleeMotionWarpSnapshot());
					TestEqual(TEXT("Player still has 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
				}

				// 3.6 Snapshot static location & ground state reuse (does not follow moved/airborne target)
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
					Player->SetTestLockedTarget(Enemy);

					TestTrue(TEXT("StartComboEntry(0) captures initial location"), Ability->TestStartComboEntry(0));
					TestTrue(TEXT("Captured location is (140, 0, 100)"), FMath::IsNearlyEqual(Ability->GetTestMeleeMotionWarpCapturedLocation().X, 140.0f, 0.1f));

					// Move enemy far away and into the air
					Enemy->SetActorLocation(FVector(500.0f, 500.0f, 300.0f));
					if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
					{
						EnemyMoveComp->SetMovementMode(MOVE_Falling);
					}

					TestTrue(TEXT("StartComboEntry(1) evaluates against cached snapshot"), Ability->TestStartComboEntry(1));
					TestTrue(TEXT("Player still has MeleeContact_1 based on cached location"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1"))));

					FTransform StoredTransform1;
					Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1")), &StoredTransform1);
					TestTrue(TEXT("Entry 1 warp target X is strictly based on cached 140cm (X=50cm)"), FMath::IsNearlyEqual(StoredTransform1.GetLocation().X, 50.0f, 0.1f));

					// Restore enemy
					Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
					if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
					{
						EnemyMoveComp->SetMovementMode(MOVE_Walking);
					}
				}

				// 3.7 First entry geometry failure preserves snapshot for subsequent entry
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Enemy->SetActorLocation(FVector(240.0f, 0.0f, 100.0f)); // 240cm away
					Player->SetTestLockedTarget(Enemy);

					// Entry 0: WarpStopDistance=100, MaxWarpDistance=60 -> max allowed 160cm -> 240cm fails evaluator
					TestTrue(TEXT("StartComboEntry(0) starts combo"), Ability->TestStartComboEntry(0));
					TestEqual(TEXT("Entry 0 evaluator fails, 0 targets written"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
					TestTrue(TEXT("Snapshot was successfully captured despite evaluator failure"), Ability->HasTestMeleeMotionWarpSnapshot());
					TestTrue(TEXT("Captured location X is 240cm"), FMath::IsNearlyEqual(Ability->GetTestMeleeMotionWarpCapturedLocation().X, 240.0f, 0.1f));

					// Entry 1: configured with larger MaxWarpDistance = 150cm -> max allowed 250cm -> 240cm passes
					ComboAsset->Entries[1].WarpStopDistance = 100.0f;
					ComboAsset->Entries[1].MaxWarpDistance = 150.0f;

					TestTrue(TEXT("StartComboEntry(1) succeeds with relaxed entry config"), Ability->TestStartComboEntry(1));
					TestTrue(TEXT("Player has recorded MeleeContact_1 from preserved snapshot"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1"))));

					FTransform StoredTransform1;
					Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_1")), &StoredTransform1);
					TestTrue(TEXT("Warp location X is 140cm (240 - 100)"), FMath::IsNearlyEqual(StoredTransform1.GetLocation().X, 140.0f, 0.1f));

					// Restore Entry 1 & Enemy
					ComboAsset->Entries[1].WarpStopDistance = 90.0f;
					ComboAsset->Entries[1].MaxWarpDistance = 70.0f;
					Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
				}

				// 3.8 Target death defense
				{
					Ability->TestResetMeleeMotionWarpState();
					Player->ClearMeleeMotionWarpTargets();
					Player->SetTestLockedTarget(Enemy);

					TestTrue(TEXT("StartComboEntry(0) succeeds"), Ability->TestStartComboEntry(0));
					TestTrue(TEXT("Player has MeleeContact_0"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0"))));

					// Enemy dies during combo
					if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
					{
						EnemyASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead"))));
					}
					TestTrue(TEXT("Enemy is dead"), Enemy->IsDead());

					TestTrue(TEXT("StartComboEntry(1) succeeds for normal montage continuation"), Ability->TestStartComboEntry(1));
					TestEqual(TEXT("Target death fails motion warp closed (count == 0)"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

					// Restore enemy alive state
					if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
					{
						EnemyASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false), 0);
					}
					TestFalse(TEXT("Enemy restored to alive state"), Enemy->IsDead());
				}

				// 3.9 Ability state reset on reset helper
				{
					Ability->SetTestBypassMontageActiveCheck(true);
					Ability->TestResetMeleeMotionWarpState();
					TestFalse(TEXT("Reset clears attempted capture"), Ability->HasTestMeleeMotionWarpCaptureAttempted());
					TestFalse(TEXT("Reset clears snapshot target"), Ability->HasTestMeleeMotionWarpSnapshot());
				}

				// 3.10 Real Enemy HandleDeath() production entry immediately clears warp targets on player
				{
					AEnemyCharacter* EnemyForDeath = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(140.0f, 0.0f, 100.0f)));
					if (TestNotNull(TEXT("EnemyForDeath spawned for death test"), EnemyForDeath))
					{
						EnemyForDeath->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false));
						if (UCharacterMovementComponent* MoveComp = EnemyForDeath->GetCharacterMovement())
						{
							MoveComp->SetMovementMode(MOVE_Walking);
						}

						Ability->TestResetMeleeMotionWarpState();
						Ability->SetTestBypassMontageActiveCheck(true);
						Player->ClearMeleeMotionWarpTargets();
						Player->SetTestLockedTarget(EnemyForDeath);

						TestTrue(TEXT("StartComboEntry(0) succeeds"), Ability->TestStartComboEntry(0));
						TestTrue(TEXT("Player has MeleeContact_0"), Player->HasTestMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0"))));

						// Trigger real Enemy HandleDeath teardown via Dead state tag change
						const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
						if (UAbilitySystemComponent* EnemyASC = EnemyForDeath->GetAbilitySystemComponent())
						{
							EnemyASC->SetLooseGameplayTagCount(DeadTag, 1);
						}
						TestTrue(TEXT("Enemy enters Dead state"), EnemyForDeath->IsDead());
						TestEqual(TEXT("Enemy HandleDeath() immediately cleared player warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

						// Next entry must fail-closed without re-targeting
						TestTrue(TEXT("StartComboEntry(1) continues montage but does not warp"), Ability->TestStartComboEntry(1));
						TestEqual(TEXT("Player still has 0 warp targets after dead enemy combo continuation"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
					}
				}

				// 3.11 Real Enemy Destroy() / EndPlay() path immediately clears warp targets on player
				{
					AEnemyCharacter* TempEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(140.0f, 0.0f, 100.0f)));
					if (TestNotNull(TEXT("TempEnemy spawned for destroy test"), TempEnemy))
					{
						TempEnemy->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false));
						if (UCharacterMovementComponent* TempMoveComp = TempEnemy->GetCharacterMovement())
						{
							TempMoveComp->SetMovementMode(MOVE_Walking);
						}

						Player->ClearMeleeMotionWarpTargets();
						Player->SetTestLockedTarget(TempEnemy);
						Player->SetMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), FTransform(FVector(100.0f, 0.0f, 0.0f)));
						TestEqual(TEXT("Player has 1 warp target before TempEnemy Destroy"), Player->GetTestMeleeMotionWarpTargetCount(), 1);

						// Destroy TempEnemy (triggers EndPlay)
						TempEnemy->Destroy();
						TestEqual(TEXT("TempEnemy EndPlay immediately cleared player warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
					}
				}

				// 3.12 One-shot capture gating: Controller context failure on first legal entry consumes capture opportunity and prevents re-targeting upon recovery
				{
					AEnemyCharacter* FreshEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(140.0f, 0.0f, 100.0f)));
					if (TestNotNull(TEXT("FreshEnemy spawned for context invalidation test"), FreshEnemy))
					{
						FreshEnemy->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false));
						if (UCharacterMovementComponent* FreshMoveComp = FreshEnemy->GetCharacterMovement())
						{
							FreshMoveComp->SetMovementMode(MOVE_Walking);
						}

						Ability->TestResetMeleeMotionWarpState();
						Ability->SetTestBypassMontageActiveCheck(true);
						Player->ClearMeleeMotionWarpTargets();
						Player->SetTestLockedTarget(FreshEnemy);

						// Invalidate controller context BEFORE the first legal entry
						PlayerController->UnPossess();

						// Entry 0 fails context check, but MUST consume the one-shot capture opportunity
						Ability->TestStartComboEntry(0);
						TestTrue(TEXT("Entry 0 with detached controller consumed capture attempt"), Ability->HasTestMeleeMotionWarpCaptureAttempted());
						TestFalse(TEXT("Entry 0 with detached controller did not record snapshot"), Ability->HasTestMeleeMotionWarpSnapshot());
						TestEqual(TEXT("Entry 0 wrote 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

						// Restore controller possession and lock-on
						PlayerController->Possess(Player);
						Player->SetTestLockedTarget(FreshEnemy);

						// Subsequent legal Entry 1 must NOT re-target despite valid controller and target
						Ability->TestStartComboEntry(1);
						TestFalse(TEXT("Entry 1 does not re-target after prior context failure"), Ability->HasTestMeleeMotionWarpSnapshot());
						TestEqual(TEXT("Entry 1 wrote 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
					}
				}

				// 3.13 Player ClearLockedTarget() immediately clears warp targets
				{
					Player->ClearMeleeMotionWarpTargets();
					Player->SetMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), FTransform(FVector(100.0f, 0.0f, 0.0f)));
					TestEqual(TEXT("Player has 1 warp target before ClearLockedTarget"), Player->GetTestMeleeMotionWarpTargetCount(), 1);

					Player->TestClearLockedTarget();
					TestEqual(TEXT("ClearLockedTarget() immediately cleared warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
				}

				// 3.14 StartComboEntry early returns clean up existing warp targets (cannot be bypassed by bTestBypassMontageActiveCheck)
				{
					Ability->SetTestBypassMontageActiveCheck(true);
					Player->ClearMeleeMotionWarpTargets();
					Player->SetMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), FTransform(FVector(100.0f, 0.0f, 0.0f)));
					TestEqual(TEXT("Player has 1 warp target before early return"), Player->GetTestMeleeMotionWarpTargetCount(), 1);

					// Early return due to missing BoundAnimInstance
					Ability->SetTestBoundAnimInstance(nullptr);
					TestFalse(TEXT("StartComboEntry(0) returns false without AnimInstance despite bypass"), Ability->TestStartComboEntry(0));
					TestEqual(TEXT("Early return without AnimInstance clears player warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

					// Restore AnimInstance
					Ability->SetTestBoundAnimInstance(TestAnimInstance);

					// Early return due to invalid entry index (null Entry / missing Montage)
					Player->SetMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), FTransform(FVector(100.0f, 0.0f, 0.0f)));
					TestFalse(TEXT("StartComboEntry(-1) returns false on invalid index despite bypass"), Ability->TestStartComboEntry(-1));
					TestEqual(TEXT("Early return on invalid index clears player warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

					// Synchronously ended ability must fail closed, clear warp, and NOT resurrect previous active identity
					Player->SetMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), FTransform(FVector(100.0f, 0.0f, 0.0f)));
					Ability->SetTestActiveEntryIndex(INDEX_NONE);
					Ability->SetTestEndAbilityRequested(true);
					TestFalse(TEXT("StartComboEntry(0) returns false when ability is synchronously ended"), Ability->TestStartComboEntry(0));
					TestEqual(TEXT("Synchronous ability end clears player warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
					TestEqual(TEXT("Synchronous ability end does not resurrect active entry identity"), Ability->GetTestActiveEntryIndex(), INDEX_NONE);
					Ability->SetTestEndAbilityRequested(false);
				}

				// 3.15 Player UnPossessed() clears warp targets and cancels light attack ability
				{
					Player->SetMeleeMotionWarpTarget(FName(TEXT("MeleeContact_0")), FTransform(FVector(100.0f, 0.0f, 0.0f)));
					TestEqual(TEXT("Player has 1 warp target before UnPossessed"), Player->GetTestMeleeMotionWarpTargetCount(), 1);

					Player->TestUnPossessed();
					TestEqual(TEXT("UnPossessed() immediately cleared warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

					// Restore possession for subsequent sections
					PlayerController->UnPossess();
					PlayerController->Possess(Player);
					if (UCharacterMovementComponent* PlayerMoveComp = Player->GetCharacterMovement())
					{
						PlayerMoveComp->SetMovementMode(MOVE_Walking);
					}
					if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
					{
						EnemyMoveComp->SetMovementMode(MOVE_Walking);
					}
				}

				// -------------------------------------------------------------------------
				// SECTION 4: Charged Attack Motion Warping Matrix (UChargedAttackAbility)
				// -------------------------------------------------------------------------
				{
					UChargedAttackAbility* ChargedAbility = NewObject<UChargedAttackAbility>(Player);
					if (TestNotNull(TEXT("ChargedAbility constructed"), ChargedAbility))
					{
						ChargedAbility->SetTestCurrentActorInfo(Player->GetAbilitySystemComponent()->AbilityActorInfo.Get());

						// 4.1 Defaults: bUseMotionWarping is false, WarpTargetName is MeleeContact
						{
							Player->ClearMeleeMotionWarpTargets();
							Player->SetTestLockedTarget(Enemy);
							ChargedAbility->TestResetMeleeMotionWarpState();

							ChargedAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestFalse(TEXT("Charged attack default config does not attempt capture"), ChargedAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestFalse(TEXT("Charged attack default config does not record snapshot"), ChargedAbility->HasTestMeleeMotionWarpSnapshot());
							TestEqual(TEXT("Charged attack default config writes 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
						}

						// 4.2 HoldReady/Charging does not capture or write warp targets
						{
							ChargedAbility->SetTestMotionWarpConfig(true, FName(TEXT("MeleeContact")), 100.0f, 60.0f, 60.0f);
							Player->ClearMeleeMotionWarpTargets();
							Player->SetTestLockedTarget(Enemy);
							ChargedAbility->TestResetMeleeMotionWarpState();

							ChargedAbility->Test_SimulateHoldReady();
							TestTrue(TEXT("HoldReady latched state"), ChargedAbility->Test_IsMontagePausedAtHoldReady());
							TestFalse(TEXT("HoldReady did not attempt capture"), ChargedAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestFalse(TEXT("HoldReady did not record snapshot"), ChargedAbility->HasTestMeleeMotionWarpSnapshot());
							TestEqual(TEXT("HoldReady wrote 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
						}

						// 4.3 Successful release captures target once and writes warp target to player
						{
							Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
							Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
							if (UCharacterMovementComponent* PlayerMoveComp = Player->GetCharacterMovement())
							{
								PlayerMoveComp->SetMovementMode(MOVE_Walking);
							}
							if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
							{
								EnemyMoveComp->SetMovementMode(MOVE_Walking);
							}
							Player->ClearMeleeMotionWarpTargets();
							Player->SetTestLockedTarget(Enemy);
							ChargedAbility->TestResetMeleeMotionWarpState();
							ChargedAbility->SetTestMotionWarpConfig(true, FName(TEXT("MeleeContact")), 100.0f, 60.0f, 60.0f);

							ChargedAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestTrue(TEXT("Charged release consumed capture attempt"), ChargedAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestTrue(TEXT("Charged release recorded snapshot"), ChargedAbility->HasTestMeleeMotionWarpSnapshot());
							TestEqual(TEXT("Charged release wrote 1 warp target"), Player->GetTestMeleeMotionWarpTargetCount(), 1);
							TestTrue(TEXT("Captured location matches enemy"), FMath::IsNearlyEqual(ChargedAbility->GetTestMeleeMotionWarpCapturedLocation().X, 140.0f, 0.1f));
						}

						// 4.4 Static snapshot: moving target or changing lock after capture does not alter cached snapshot
						{
							Enemy->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
							Player->TestClearLockedTarget();

							ChargedAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestTrue(TEXT("Charged snapshot location remains unchanged after enemy moved and lock cleared"),
								FMath::IsNearlyEqual(ChargedAbility->GetTestMeleeMotionWarpCapturedLocation().X, 140.0f, 0.1f));

							// Restore enemy location
							Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
							if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
							{
								EnemyMoveComp->SetMovementMode(MOVE_Walking);
							}
							Player->SetTestLockedTarget(Enemy);
						}

						// 4.5 Target invalidated (Dead) clears warp targets on next evaluation
						{
							if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
							{
								EnemyASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
							}
							ChargedAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestEqual(TEXT("Dead target evaluation clears player warp targets in charged attack"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

							// Restore enemy alive
							if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
							{
								EnemyASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false), 0);
							}
						}

						// 4.6 Reset and EndAbility clean up state
						{
							Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
							Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
							if (UCharacterMovementComponent* PlayerMoveComp = Player->GetCharacterMovement())
							{
								PlayerMoveComp->SetMovementMode(MOVE_Walking);
							}
							if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
							{
								EnemyMoveComp->SetMovementMode(MOVE_Walking);
							}
							Player->SetTestLockedTarget(Enemy);
							ChargedAbility->TestResetMeleeMotionWarpState();
							ChargedAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestEqual(TEXT("Player has 1 warp target in charged test"), Player->GetTestMeleeMotionWarpTargetCount(), 1);

							ChargedAbility->TestResetMeleeMotionWarpState();
							Player->ClearMeleeMotionWarpTargets();
							TestFalse(TEXT("Snapshot reset in charged test"), ChargedAbility->HasTestMeleeMotionWarpSnapshot());
							TestFalse(TEXT("Capture attempt reset in charged test"), ChargedAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestEqual(TEXT("Player warp targets cleared in charged test"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

							// EndAbility resets test bypass flag
							ChargedAbility->SetTestBypassMontageActiveCheck(true);
							TestTrue(TEXT("Charged ability bypass set to true for test"), ChargedAbility->GetTestBypassMontageActiveCheck());
							ChargedAbility->EndAbility(FGameplayAbilitySpecHandle(), ChargedAbility->GetCurrentActorInfo(), FGameplayAbilityActivationInfo(), true, false);
							TestFalse(TEXT("Charged ability EndAbility resets test bypass flag"), ChargedAbility->GetTestBypassMontageActiveCheck());
						}
					}
				}

				// -------------------------------------------------------------------------
				// SECTION 5: Sprint Attack Motion Warping Matrix (USprintAttackAbility)
				// -------------------------------------------------------------------------
				{
					USprintAttackAbility* SprintAbility = NewObject<USprintAttackAbility>(Player);
					if (TestNotNull(TEXT("SprintAbility constructed"), SprintAbility))
					{
						SprintAbility->SetTestCurrentActorInfo(Player->GetAbilitySystemComponent()->AbilityActorInfo.Get());

						// 5.1 Defaults: bUseMotionWarping is false, WarpTargetName is MeleeContact
						{
							Player->ClearMeleeMotionWarpTargets();
							Player->SetTestLockedTarget(Enemy);
							SprintAbility->TestResetMeleeMotionWarpState();

							SprintAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestFalse(TEXT("Sprint attack default config does not attempt capture"), SprintAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestFalse(TEXT("Sprint attack default config does not record snapshot"), SprintAbility->HasTestMeleeMotionWarpSnapshot());
							TestEqual(TEXT("Sprint attack default config writes 0 warp targets"), Player->GetTestMeleeMotionWarpTargetCount(), 0);
						}

						// 5.2 Successful apply captures target once and writes warp target to player
						{
							Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
							Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
							if (UCharacterMovementComponent* PlayerMoveComp = Player->GetCharacterMovement())
							{
								PlayerMoveComp->SetMovementMode(MOVE_Walking);
							}
							if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
							{
								EnemyMoveComp->SetMovementMode(MOVE_Walking);
							}
							Player->ClearMeleeMotionWarpTargets();
							Player->SetTestLockedTarget(Enemy);
							SprintAbility->TestResetMeleeMotionWarpState();
							SprintAbility->SetTestMotionWarpConfig(true, FName(TEXT("MeleeContact")), 100.0f, 60.0f, 60.0f);

							SprintAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestTrue(TEXT("Sprint attack consumed capture attempt"), SprintAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestTrue(TEXT("Sprint attack recorded snapshot"), SprintAbility->HasTestMeleeMotionWarpSnapshot());
							TestEqual(TEXT("Sprint attack wrote 1 warp target"), Player->GetTestMeleeMotionWarpTargetCount(), 1);
							TestTrue(TEXT("Sprint captured location matches enemy"), FMath::IsNearlyEqual(SprintAbility->GetTestMeleeMotionWarpCapturedLocation().X, 140.0f, 0.1f));
						}

						// 5.3 Static snapshot: moving target or changing lock after capture does not alter cached snapshot
						{
							Enemy->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
							Player->TestClearLockedTarget();

							SprintAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestTrue(TEXT("Sprint snapshot location remains unchanged after enemy moved and lock cleared"),
								FMath::IsNearlyEqual(SprintAbility->GetTestMeleeMotionWarpCapturedLocation().X, 140.0f, 0.1f));

							// Restore enemy location
							Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
							if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
							{
								EnemyMoveComp->SetMovementMode(MOVE_Walking);
							}
							Player->SetTestLockedTarget(Enemy);
						}

						// 5.4 Target invalidated (Dead) clears warp targets on next evaluation
						{
							if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
							{
								EnemyASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
							}
							SprintAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestEqual(TEXT("Dead target evaluation clears player warp targets in sprint attack"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

							// Restore enemy alive
							if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
							{
								EnemyASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false), 0);
							}
						}

						// 5.5 Reset and EndAbility clean up state
						{
							Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
							Enemy->SetActorLocation(FVector(140.0f, 0.0f, 100.0f));
							if (UCharacterMovementComponent* PlayerMoveComp = Player->GetCharacterMovement())
							{
								PlayerMoveComp->SetMovementMode(MOVE_Walking);
							}
							if (UCharacterMovementComponent* EnemyMoveComp = Enemy->GetCharacterMovement())
							{
								EnemyMoveComp->SetMovementMode(MOVE_Walking);
							}
							Player->SetTestLockedTarget(Enemy);
							SprintAbility->TestResetMeleeMotionWarpState();
							SprintAbility->Test_TryApplyMeleeMotionWarpTarget(Player);
							TestEqual(TEXT("Player has 1 warp target in sprint test"), Player->GetTestMeleeMotionWarpTargetCount(), 1);

							SprintAbility->TestResetMeleeMotionWarpState();
							Player->ClearMeleeMotionWarpTargets();
							TestFalse(TEXT("Sprint snapshot reset"), SprintAbility->HasTestMeleeMotionWarpSnapshot());
							TestFalse(TEXT("Sprint capture attempt reset"), SprintAbility->HasTestMeleeMotionWarpCaptureAttempted());
							TestEqual(TEXT("Player warp targets cleared in sprint test"), Player->GetTestMeleeMotionWarpTargetCount(), 0);

							// EndAbility resets test bypass flag
							SprintAbility->SetTestBypassMontageActiveCheck(true);
							TestTrue(TEXT("Sprint ability bypass set to true for test"), SprintAbility->GetTestBypassMontageActiveCheck());
							SprintAbility->EndAbility(FGameplayAbilitySpecHandle(), SprintAbility->GetCurrentActorInfo(), FGameplayAbilityActivationInfo(), true, false);
							TestFalse(TEXT("Sprint ability EndAbility resets test bypass flag"), SprintAbility->GetTestBypassMontageActiveCheck());
						}
					}
				}
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
