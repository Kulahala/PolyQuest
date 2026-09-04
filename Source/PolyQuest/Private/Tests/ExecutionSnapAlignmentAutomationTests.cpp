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
		TestEqual(TEXT("Front location along +Y"), OutTransformFront.GetLocation(), FVector(0.0f, 100.0f, 20.0f));
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
		TestEqual(TEXT("Backstab location along -Y"), OutTransformBackstab.GetLocation(), FVector(0.0f, -100.0f, 20.0f));
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
	// 7. MeleeWeaponDefinition IsValidWeaponDefinition Validation
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
		// Default 190.0f is valid
		TestTrue(FString::Printf(TEXT("Default ExecutionSnapDistance 190.0f is valid (Reason: %s)"), *Reason), MeleeDef->IsValidWeaponDefinition(Reason));

		// Test <= 0 fails
		MeleeDef->ExecutionSnapDistance = 0.0f;
		TestFalse(TEXT("ExecutionSnapDistance 0.0f fails validation"), MeleeDef->IsValidWeaponDefinition(Reason));

		MeleeDef->ExecutionSnapDistance = -50.0f;
		TestFalse(TEXT("ExecutionSnapDistance -50.0f fails validation"), MeleeDef->IsValidWeaponDefinition(Reason));

		// Test NaN fails
		MeleeDef->ExecutionSnapDistance = NAN;
		TestFalse(TEXT("ExecutionSnapDistance NaN fails validation"), MeleeDef->IsValidWeaponDefinition(Reason));

		// Restore valid
		MeleeDef->ExecutionSnapDistance = 190.0f;
		TestTrue(FString::Printf(TEXT("Restored ExecutionSnapDistance passes (Reason: %s)"), *Reason), MeleeDef->IsValidWeaponDefinition(Reason));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
