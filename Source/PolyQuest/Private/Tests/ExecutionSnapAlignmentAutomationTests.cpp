// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Execution/ExecutionSnapAlignment.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "GameplayTagContainer.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExecutionSnapAlignmentAutomationTest,
	"PolyQuest.Combat.ExecutionSnapAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExecutionSnapAlignmentAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. IsSnapDistanceValid
	// =========================================================================
	{
		TestTrue(TEXT("190.0f is valid snap distance"), FExecutionSnapAlignment::IsSnapDistanceValid(190.0f));
		TestTrue(TEXT("1.0f is valid snap distance"), FExecutionSnapAlignment::IsSnapDistanceValid(1.0f));
		TestFalse(TEXT("0.0f is invalid snap distance"), FExecutionSnapAlignment::IsSnapDistanceValid(0.0f));
		TestFalse(TEXT("-10.0f is invalid snap distance"), FExecutionSnapAlignment::IsSnapDistanceValid(-10.0f));
		TestFalse(TEXT("NaN is invalid snap distance"), FExecutionSnapAlignment::IsSnapDistanceValid(NAN));
		TestFalse(TEXT("Infinity is invalid snap distance"), FExecutionSnapAlignment::IsSnapDistanceValid(INFINITY));
	}

	// =========================================================================
	// 2. Front Geometry Calculation (TargetLocation + Forward * Distance, Facing -Forward)
	// =========================================================================
	{
		const FVector PlayerLoc(0.0f, 0.0f, 100.0f);
		const FVector TargetLoc(100.0f, 200.0f, 50.0f);
		const FVector TargetForward(1.0f, 0.0f, 0.0f); // Facing +X
		const float SnapDistance = 190.0f;

		FTransform OutTransform;
		const bool bSuccess = FExecutionSnapAlignment::TryBuildTransform(
			PlayerLoc,
			TargetLoc,
			TargetForward,
			SnapDistance,
			EExecutionSnapSide::Front,
			OutTransform);

		TestTrue(TEXT("Front snap succeeds with valid inputs"), bSuccess);

		// Expected location: TargetLoc + Forward * Distance with PlayerLoc.Z = (100 + 190, 200, 100) = (290, 200, 100)
		const FVector ExpectedLoc(290.0f, 200.0f, 100.0f);
		TestEqual(TEXT("Front snap location matches Target + Forward * Distance"), OutTransform.GetLocation(), ExpectedLoc);

		// Output Z must equal PlayerLocation.Z (maintaining player grounded height)
		TestEqual(TEXT("Front snap Z anchors to PlayerLocation.Z"), OutTransform.GetLocation().Z, 100.0);

		// Expected player facing: -Forward = (-1, 0, 0) -> Yaw = 180 or -180
		const float ResultYaw = OutTransform.Rotator().Yaw;
		const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(180.0f, ResultYaw));
		TestTrue(TEXT("Front snap player faces target (-Forward)"), YawDelta <= 1.0f);
	}

	// =========================================================================
	// 3. Backstab Geometry Calculation (TargetLocation - Forward * Distance, Facing +Forward)
	// =========================================================================
	{
		const FVector PlayerLoc(0.0f, 0.0f, 500.0f);
		const FVector TargetLoc(100.0f, 200.0f, 50.0f);
		const FVector TargetForward(1.0f, 0.0f, 0.0f); // Facing +X
		const float SnapDistance = 190.0f;

		FTransform OutTransform;
		const bool bSuccess = FExecutionSnapAlignment::TryBuildTransform(
			PlayerLoc,
			TargetLoc,
			TargetForward,
			SnapDistance,
			EExecutionSnapSide::Backstab,
			OutTransform);

		TestTrue(TEXT("Backstab snap succeeds with valid inputs"), bSuccess);

		// Expected location: TargetLoc - Forward * Distance with PlayerLoc.Z = (100 - 190, 200, 500) = (-90, 200, 500)
		const FVector ExpectedLoc(-90.0f, 200.0f, 500.0f);
		TestEqual(TEXT("Backstab snap location matches Target - Forward * Distance"), OutTransform.GetLocation(), ExpectedLoc);

		// Output Z must equal PlayerLocation.Z
		TestEqual(TEXT("Backstab snap Z anchors to PlayerLocation.Z"), OutTransform.GetLocation().Z, 500.0);

		// Expected player facing: +Forward = (1, 0, 0) -> Yaw = 0
		const float ResultYaw = OutTransform.Rotator().Yaw;
		const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(0.0f, ResultYaw));
		TestTrue(TEXT("Backstab snap player faces target (+Forward)"), YawDelta <= 1.0f);
	}

	// =========================================================================
	// 4. Diagonal Forward Direction & 2D Normalization
	// =========================================================================
	{
		const FVector PlayerLoc(0.0f, 0.0f, 0.0f);
		const FVector TargetLoc(0.0f, 0.0f, 20.0f);
		// Unnormalized forward with non-zero Z component (should be ignored and normalized in XY)
		const FVector TargetForward(0.0f, 5.0f, 10.0f); // Direction +Y
		const float SnapDistance = 100.0f;

		FTransform OutTransformFront;
		const bool bFrontOk = FExecutionSnapAlignment::TryBuildTransform(
			PlayerLoc,
			TargetLoc,
			TargetForward,
			SnapDistance,
			EExecutionSnapSide::Front,
			OutTransformFront);

		TestTrue(TEXT("Diagonal forward front snap succeeds"), bFrontOk);
		// Output Z must anchor to PlayerLoc.Z (0.0f), preserving player grounded height
		TestEqual(TEXT("Front location along +Y"), OutTransformFront.GetLocation(), FVector(0.0f, 100.0f, 0.0f));
		// Facing -Y -> Yaw = -90
		TestTrue(TEXT("Front facing -Y has Yaw -90"), FMath::IsNearlyEqual(OutTransformFront.Rotator().Yaw, -90.0f, 0.01f));

		FTransform OutTransformBackstab;
		const bool bBackOk = FExecutionSnapAlignment::TryBuildTransform(
			PlayerLoc,
			TargetLoc,
			TargetForward,
			SnapDistance,
			EExecutionSnapSide::Backstab,
			OutTransformBackstab);

		TestTrue(TEXT("Diagonal forward backstab snap succeeds"), bBackOk);
		// Output Z must anchor to PlayerLoc.Z (0.0f), preserving player grounded height
		TestEqual(TEXT("Backstab location along -Y"), OutTransformBackstab.GetLocation(), FVector(0.0f, -100.0f, 0.0f));
		// Facing +Y -> Yaw = +90
		TestTrue(TEXT("Backstab facing +Y has Yaw +90"), FMath::IsNearlyEqual(OutTransformBackstab.Rotator().Yaw, 90.0f, 0.01f));
	}

	// =========================================================================
	// 5. Fail-Closed on Invalid Inputs & Output Identity Reset
	// =========================================================================
	{
		const FVector ValidPlayer(0.0f, 0.0f, 0.0f);
		const FVector ValidTarget(100.0f, 100.0f, 0.0f);
		const FVector ValidForward(1.0f, 0.0f, 0.0f);

		FTransform OutTransform(FRotator(10.0f, 20.0f, 30.0f), FVector(999.0f, 999.0f, 999.0f));

		// 5.1 Invalid Distance (<= 0)
		TestFalse(TEXT("Zero distance fails"), FExecutionSnapAlignment::TryBuildTransform(ValidPlayer, ValidTarget, ValidForward, 0.0f, EExecutionSnapSide::Front, OutTransform));
		TestEqual(TEXT("Transform reset on failure"), OutTransform.GetLocation(), FVector::ZeroVector);

		// 5.2 NaN/Inf in Player Location
		TestFalse(TEXT("Player NaN fails"), FExecutionSnapAlignment::TryBuildTransform(FVector(NAN, 0.0f, 0.0f), ValidTarget, ValidForward, 190.0f, EExecutionSnapSide::Front, OutTransform));
		TestEqual(TEXT("Transform reset on failure"), OutTransform.GetLocation(), FVector::ZeroVector);

		// 5.3 NaN/Inf in Target Location
		TestFalse(TEXT("Target Inf fails"), FExecutionSnapAlignment::TryBuildTransform(ValidPlayer, FVector(INFINITY, 0.0f, 0.0f), ValidForward, 190.0f, EExecutionSnapSide::Front, OutTransform));

		// 5.4 Zero XY Forward (vertical only)
		TestFalse(TEXT("Vertical forward (zero XY) fails"), FExecutionSnapAlignment::TryBuildTransform(ValidPlayer, ValidTarget, FVector(0.0f, 0.0f, 1.0f), 190.0f, EExecutionSnapSide::Front, OutTransform));

		// 5.5 Unknown Side Enum
		TestFalse(TEXT("Unknown side enum fails"), FExecutionSnapAlignment::TryBuildTransform(ValidPlayer, ValidTarget, ValidForward, 190.0f, static_cast<EExecutionSnapSide>(99), OutTransform));
	}

	// =========================================================================
	// 6. Tolerance & 180/-180 Wrapping Invariants
	// =========================================================================
	{
		// Test FindDeltaAngleDegrees handles 180 vs -180 wrapping properly
		const float Delta180 = FMath::FindDeltaAngleDegrees(180.0f, -180.0f);
		TestTrue(TEXT("FindDeltaAngleDegrees between 180 and -180 is 0"), FMath::IsNearlyZero(Delta180, KINDA_SMALL_NUMBER));

		const float DeltaWrapNear = FMath::FindDeltaAngleDegrees(179.5f, -179.8f);
		TestTrue(TEXT("FindDeltaAngleDegrees across 180 boundary is under 1 deg delta"), FMath::Abs(DeltaWrapNear) < 1.0f);
	}

	// =========================================================================
	// 7. IsExecutionDistanceRangeValid Pure Validation Matrix
	// =========================================================================
	{
		FString Reason;

		// 7.1 Default values: 0 / 250 / 190 is valid
		TestTrue(TEXT("Default 0 / 250 / 190 is valid"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, 250.0f, 190.0f, &Reason));
		TestTrue(TEXT("Reason is empty on success"), Reason.IsEmpty());

		// 7.2 Boundary inclusive: Snap == Min
		TestTrue(TEXT("Snap == Min is valid (inclusive)"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(100.0f, 200.0f, 100.0f, &Reason));

		// 7.3 Boundary inclusive: Snap == Max
		TestTrue(TEXT("Snap == Max is valid (inclusive)"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(100.0f, 200.0f, 200.0f, &Reason));

		// 7.4 Non-finite values rejected (order 1)
		TestFalse(TEXT("NaN Min is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(NAN, 250.0f, 190.0f, &Reason));
		TestEqual(TEXT("NaN Min failure reason"), Reason, FString(TEXT("Execution distance values must be finite numbers.")));

		TestFalse(TEXT("Inf Max is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, INFINITY, 190.0f, &Reason));
		TestEqual(TEXT("Inf Max failure reason"), Reason, FString(TEXT("Execution distance values must be finite numbers.")));

		TestFalse(TEXT("NaN Snap is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, 250.0f, NAN, &Reason));
		TestEqual(TEXT("NaN Snap failure reason"), Reason, FString(TEXT("Execution distance values must be finite numbers.")));

		// 7.5 Negative Min rejected (order 2)
		TestFalse(TEXT("Negative Min is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(-1.0f, 250.0f, 190.0f, &Reason));
		TestEqual(TEXT("Negative Min failure reason"), Reason, FString(TEXT("MinExecutionDistance must be non-negative.")));

		// 7.6 Max <= Min rejected (order 3)
		TestFalse(TEXT("Max == Min is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(100.0f, 100.0f, 100.0f, &Reason));
		TestEqual(TEXT("Max == Min failure reason"), Reason, FString(TEXT("MaxExecutionDistance must be strictly greater than MinExecutionDistance.")));

		TestFalse(TEXT("Max < Min is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(150.0f, 100.0f, 120.0f, &Reason));
		TestEqual(TEXT("Max < Min failure reason"), Reason, FString(TEXT("MaxExecutionDistance must be strictly greater than MinExecutionDistance.")));

		// 7.7 Snap <= 0 rejected (order 4)
		TestFalse(TEXT("Snap == 0 is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, 250.0f, 0.0f, &Reason));
		TestEqual(TEXT("Snap == 0 failure reason"), Reason, FString(TEXT("ExecutionSnapDistance must be strictly positive.")));

		TestFalse(TEXT("Negative Snap is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, 250.0f, -10.0f, &Reason));
		TestEqual(TEXT("Negative Snap failure reason"), Reason, FString(TEXT("ExecutionSnapDistance must be strictly positive.")));

		// 7.8 Snap out of [Min, Max] rejected (order 5)
		TestFalse(TEXT("Snap < Min is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(50.0f, 200.0f, 40.0f, &Reason));
		TestEqual(TEXT("Snap < Min failure reason"), Reason, FString(TEXT("ExecutionSnapDistance must be within [MinExecutionDistance, MaxExecutionDistance].")));

		TestFalse(TEXT("Snap > Max is rejected"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(50.0f, 200.0f, 210.0f, &Reason));
		TestEqual(TEXT("Snap > Max failure reason"), Reason, FString(TEXT("ExecutionSnapDistance must be within [MinExecutionDistance, MaxExecutionDistance].")));

		// 7.9 Max > 250 Native Hard Cap rejected (order 6)
		TestFalse(TEXT("Max 250.1f exceeds 250cm hard cap"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, 250.1f, 190.0f, &Reason));
		TestEqual(TEXT("Max > 250cm failure reason"), Reason, FString(TEXT("MaxExecutionDistance exceeds native hard cap of 250cm.")));

		TestFalse(TEXT("Max 300.0f exceeds 250cm hard cap"),
			FExecutionSnapAlignment::IsExecutionDistanceRangeValid(0.0f, 300.0f, 190.0f, &Reason));
		TestEqual(TEXT("Max 300cm failure reason"), Reason, FString(TEXT("MaxExecutionDistance exceeds native hard cap of 250cm.")));
	}

	// =========================================================================
	// 8. MeleeWeaponDefinition IsValidWeaponDefinition Range Validation
	// =========================================================================
	{
		UMeleeWeaponDefinition* MeleeDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		MeleeDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		MeleeDef->LocomotionMode = EWeaponLocomotionMode::Default;
		MeleeDef->AttachSocketName = FName(TEXT("Weapon_R"));
		MeleeDef->WeaponMesh = nullptr;
		MeleeDef->bUseOwnerMeshSocketForTrace = true;
		MeleeDef->BladeBaseMarkerRelativeLocation = FVector::ZeroVector;
		MeleeDef->BladeTipMarkerRelativeLocation = FVector(20.0f, 0.0f, 0.0f);
		MeleeDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		MeleeDef->PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);

		FString Reason;
		// Default 0 / 250 / 190 is valid
		TestTrue(FString::Printf(TEXT("Default execution range passes (Reason: %s)"), *Reason), MeleeDef->IsValidWeaponDefinition(Reason));
		TestEqual(TEXT("Default MinExecutionDistance is 0.0f"), MeleeDef->MinExecutionDistance, 0.0f);
		TestEqual(TEXT("Default MaxExecutionDistance is 250.0f"), MeleeDef->MaxExecutionDistance, 250.0f);
		TestEqual(TEXT("Default ExecutionSnapDistance is 190.0f"), MeleeDef->ExecutionSnapDistance, 190.0f);

		// Negative MinExecutionDistance fails
		MeleeDef->MinExecutionDistance = -5.0f;
		TestFalse(TEXT("Negative MinExecutionDistance fails validation"), MeleeDef->IsValidWeaponDefinition(Reason));
		TestEqual(TEXT("Negative Min failure reason"), Reason, FString(TEXT("MinExecutionDistance must be non-negative.")));
		MeleeDef->MinExecutionDistance = 0.0f;

		// MaxExecutionDistance > 250cm hard cap fails
		MeleeDef->MaxExecutionDistance = 300.0f;
		TestFalse(TEXT("MaxExecutionDistance 300.0f fails validation"), MeleeDef->IsValidWeaponDefinition(Reason));
		TestEqual(TEXT("Max > 250 failure reason"), Reason, FString(TEXT("MaxExecutionDistance exceeds native hard cap of 250cm.")));
		MeleeDef->MaxExecutionDistance = 250.0f;

		// Snap outside [Min, Max] fails
		MeleeDef->MinExecutionDistance = 100.0f;
		MeleeDef->MaxExecutionDistance = 150.0f;
		MeleeDef->ExecutionSnapDistance = 190.0f; // 190 > 150
		TestFalse(TEXT("ExecutionSnapDistance out of range fails validation"), MeleeDef->IsValidWeaponDefinition(Reason));
		TestEqual(TEXT("Snap out of range failure reason"), Reason, FString(TEXT("ExecutionSnapDistance must be within [MinExecutionDistance, MaxExecutionDistance].")));

		// Restore valid
		MeleeDef->MinExecutionDistance = 0.0f;
		MeleeDef->MaxExecutionDistance = 250.0f;
		MeleeDef->ExecutionSnapDistance = 190.0f;
		TestTrue(FString::Printf(TEXT("Restored ExecutionSnapDistance passes (Reason: %s)"), *Reason), MeleeDef->IsValidWeaponDefinition(Reason));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
