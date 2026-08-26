#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Equipment/WorldPickupGrounding.h"
#include "Math/Box.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorldPickupGroundingAutomationTest,
	"PolyQuest.Equipment.WorldPickupGrounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldPickupGroundingAutomationTest::RunTest(const FString& Parameters)
{
	constexpr float Tolerance = 0.01f;

	const auto GetBoxCorners = [](const FBox& Box, TArray<FVector>& OutCorners)
	{
		OutCorners.Reset();
		OutCorners.Add(FVector(Box.Min.X, Box.Min.Y, Box.Min.Z));
		OutCorners.Add(FVector(Box.Min.X, Box.Min.Y, Box.Max.Z));
		OutCorners.Add(FVector(Box.Min.X, Box.Max.Y, Box.Min.Z));
		OutCorners.Add(FVector(Box.Min.X, Box.Max.Y, Box.Max.Z));
		OutCorners.Add(FVector(Box.Max.X, Box.Min.Y, Box.Min.Z));
		OutCorners.Add(FVector(Box.Max.X, Box.Min.Y, Box.Max.Z));
		OutCorners.Add(FVector(Box.Max.X, Box.Max.Y, Box.Min.Z));
		OutCorners.Add(FVector(Box.Max.X, Box.Max.Y, Box.Max.Z));
	};

	// 1. Identity bounds on flat normal produce 2cm clearance within 0.01cm tolerance
	{
		const FBox LocalBox(FVector(-50.0f, -10.0f, -5.0f), FVector(50.0f, 10.0f, 5.0f));
		const FTransform DisplayTransform = FTransform::Identity;
		const FVector ImpactPoint(100.0f, 200.0f, 0.0f);
		const FVector GroundNormal = FVector::UpVector;
		constexpr float Clearance = 2.0f;

		FVector OutRootLocation = FVector::ZeroVector;
		const bool bSuccess = FWorldPickupGrounding::TryComputeGroundedRootLocation(
			LocalBox,
			DisplayTransform,
			ImpactPoint,
			GroundNormal,
			Clearance,
			OutRootLocation);

		TestTrue(TEXT("Identity bounds flat grounding succeeds"), bSuccess);

		TArray<FVector> Corners;
		GetBoxCorners(LocalBox, Corners);
		float MinDistance = MAX_FLT;
		for (const FVector& Corner : Corners)
		{
			const FVector WorldCorner = OutRootLocation + DisplayTransform.TransformPosition(Corner);
			const float Distance = FVector::DotProduct(WorldCorner - ImpactPoint, GroundNormal);
			MinDistance = FMath::Min(MinDistance, Distance);
		}

		TestNearlyEqual(TEXT("Identity bounds lowest corner clearance is 2cm on flat ground"), MinDistance, Clearance, Tolerance);
	}

	// 2. Non-identity rotation, relative translation, and non-uniform scale
	{
		const FBox LocalBox(FVector(-30.0f, -5.0f, -2.0f), FVector(30.0f, 5.0f, 2.0f));
		const FTransform DisplayTransform(
			FRotator(30.0f, 45.0f, 60.0f),
			FVector(15.0f, -10.0f, 5.0f),
			FVector(1.5f, 0.8f, 2.0f));
		const FVector ImpactPoint(-50.0f, 75.0f, 10.0f);
		const FVector GroundNormal = FVector::UpVector;
		constexpr float Clearance = 2.0f;

		FVector OutRootLocation = FVector::ZeroVector;
		const bool bSuccess = FWorldPickupGrounding::TryComputeGroundedRootLocation(
			LocalBox,
			DisplayTransform,
			ImpactPoint,
			GroundNormal,
			Clearance,
			OutRootLocation);

		TestTrue(TEXT("Non-identity transform grounding succeeds"), bSuccess);

		TArray<FVector> Corners;
		GetBoxCorners(LocalBox, Corners);
		float MinDistance = MAX_FLT;
		for (const FVector& Corner : Corners)
		{
			const FVector WorldCorner = OutRootLocation + DisplayTransform.TransformPosition(Corner);
			const float Distance = FVector::DotProduct(WorldCorner - ImpactPoint, GroundNormal);
			MinDistance = FMath::Min(MinDistance, Distance);
			TestTrue(TEXT("All corners stay at or above clearance plane"), Distance >= (Clearance - Tolerance));
		}

		TestNearlyEqual(TEXT("Non-identity transform lowest corner clearance is 2cm"), MinDistance, Clearance, Tolerance);
	}

	// 3. Sloped normal moves only along normal and does not alter display transform
	{
		const FBox LocalBox(FVector(-20.0f, -20.0f, -20.0f), FVector(20.0f, 20.0f, 20.0f));
		const FTransform DisplayTransform(
			FRotator(15.0f, 30.0f, 0.0f),
			FVector(0.0f, 0.0f, 10.0f),
			FVector(1.0f));
		const FVector ImpactPoint(200.0f, -100.0f, 50.0f);
		const FVector GroundNormal = FVector(0.0f, 0.6f, 0.8f).GetSafeNormal();
		constexpr float Clearance = 2.0f;

		FVector OutRootLocation = FVector::ZeroVector;
		const bool bSuccess = FWorldPickupGrounding::TryComputeGroundedRootLocation(
			LocalBox,
			DisplayTransform,
			ImpactPoint,
			GroundNormal,
			Clearance,
			OutRootLocation);

		TestTrue(TEXT("Sloped normal grounding succeeds"), bSuccess);

		TArray<FVector> Corners;
		GetBoxCorners(LocalBox, Corners);
		float MinDistance = MAX_FLT;
		for (const FVector& Corner : Corners)
		{
			const FVector WorldCorner = OutRootLocation + DisplayTransform.TransformPosition(Corner);
			const float Distance = FVector::DotProduct(WorldCorner - ImpactPoint, GroundNormal);
			MinDistance = FMath::Min(MinDistance, Distance);
			TestTrue(TEXT("All corners on slope stay at or above clearance plane"), Distance >= (Clearance - Tolerance));
		}

		TestNearlyEqual(TEXT("Sloped normal lowest corner clearance is 2cm along normal"), MinDistance, Clearance, Tolerance);
	}

	// 4. Invalid bounds, zero/non-finite normals, non-finite transform, and non-finite clearance fail closed
	{
		const FBox ValidBox(FVector(-10.0f), FVector(10.0f));
		const FTransform ValidTransform = FTransform::Identity;
		const FVector ValidImpactPoint = FVector::ZeroVector;
		const FVector ValidNormal = FVector::UpVector;
		constexpr float ValidClearance = 2.0f;

		FVector OutLocation = FVector(123.0f, 456.0f, 789.0f);

		// 4.1 Invalid box (Min > Max)
		const FBox InvertedBox(FVector(10.0f), FVector(-10.0f));
		TestFalse(TEXT("Inverted box rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(InvertedBox, ValidTransform, ValidImpactPoint, ValidNormal, ValidClearance, OutLocation));
		TestEqual(TEXT("Output reset to zero on invalid box"), OutLocation, FVector::ZeroVector);

		// 4.2 Non-finite box
		OutLocation = FVector(123.0f);
		const FBox NaNBox(FVector(NAN, 0.0f, 0.0f), FVector(10.0f));
		TestFalse(TEXT("NaN box rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(NaNBox, ValidTransform, ValidImpactPoint, ValidNormal, ValidClearance, OutLocation));
		TestEqual(TEXT("Output reset to zero on NaN box"), OutLocation, FVector::ZeroVector);

		// 4.3 Zero normal
		OutLocation = FVector(123.0f);
		TestFalse(TEXT("Zero normal rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(ValidBox, ValidTransform, ValidImpactPoint, FVector::ZeroVector, ValidClearance, OutLocation));
		TestEqual(TEXT("Output reset to zero on zero normal"), OutLocation, FVector::ZeroVector);

		// 4.4 Unnormalized normal
		OutLocation = FVector(123.0f);
		TestFalse(TEXT("Unnormalized normal rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(ValidBox, ValidTransform, ValidImpactPoint, FVector(0.0f, 2.0f, 0.0f), ValidClearance, OutLocation));
		TestEqual(TEXT("Output reset to zero on unnormalized normal"), OutLocation, FVector::ZeroVector);

		// 4.5 NaN normal
		OutLocation = FVector(123.0f);
		TestFalse(TEXT("NaN normal rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(ValidBox, ValidTransform, ValidImpactPoint, FVector(NAN, 0.0f, 1.0f), ValidClearance, OutLocation));
		TestEqual(TEXT("Output reset to zero on NaN normal"), OutLocation, FVector::ZeroVector);

		// 4.6 NaN transform
		OutLocation = FVector(123.0f);
		const FTransform NaNTransform(FQuat(NAN, 0.0f, 0.0f, 1.0f), FVector::ZeroVector, FVector::OneVector);
		TestFalse(TEXT("NaN transform rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(ValidBox, NaNTransform, ValidImpactPoint, ValidNormal, ValidClearance, OutLocation));
		TestEqual(TEXT("Output reset to zero on NaN transform"), OutLocation, FVector::ZeroVector);

		// 4.7 NaN clearance
		OutLocation = FVector(123.0f);
		TestFalse(TEXT("NaN clearance rejected"), FWorldPickupGrounding::TryComputeGroundedRootLocation(ValidBox, ValidTransform, ValidImpactPoint, ValidNormal, NAN, OutLocation));
		TestEqual(TEXT("Output reset to zero on NaN clearance"), OutLocation, FVector::ZeroVector);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
