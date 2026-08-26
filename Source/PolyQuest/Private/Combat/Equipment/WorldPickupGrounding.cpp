#include "Combat/Equipment/WorldPickupGrounding.h"

bool FWorldPickupGrounding::TryComputeGroundedRootLocation(
	const FBox& LocalBox,
	const FTransform& DisplayTransform,
	const FVector& ImpactPoint,
	const FVector& GroundNormal,
	float Clearance,
	FVector& OutRootLocation)
{
	OutRootLocation = FVector::ZeroVector;

	if (!LocalBox.IsValid || LocalBox.Min.X > LocalBox.Max.X || LocalBox.Min.Y > LocalBox.Max.Y || LocalBox.Min.Z > LocalBox.Max.Z)
	{
		return false;
	}

	if (!FMath::IsFinite(LocalBox.Min.X) || !FMath::IsFinite(LocalBox.Min.Y) || !FMath::IsFinite(LocalBox.Min.Z) ||
		!FMath::IsFinite(LocalBox.Max.X) || !FMath::IsFinite(LocalBox.Max.Y) || !FMath::IsFinite(LocalBox.Max.Z))
	{
		return false;
	}

	if (DisplayTransform.ContainsNaN())
	{
		return false;
	}

	const FVector Translation = DisplayTransform.GetTranslation();
	if (!FMath::IsFinite(Translation.X) || !FMath::IsFinite(Translation.Y) || !FMath::IsFinite(Translation.Z))
	{
		return false;
	}

	const FQuat Rotation = DisplayTransform.GetRotation();
	if (!FMath::IsFinite(Rotation.X) || !FMath::IsFinite(Rotation.Y) || !FMath::IsFinite(Rotation.Z) || !FMath::IsFinite(Rotation.W) || !Rotation.IsNormalized())
	{
		return false;
	}

	const FVector Scale = DisplayTransform.GetScale3D();
	if (!FMath::IsFinite(Scale.X) || !FMath::IsFinite(Scale.Y) || !FMath::IsFinite(Scale.Z))
	{
		return false;
	}

	if (!FMath::IsFinite(ImpactPoint.X) || !FMath::IsFinite(ImpactPoint.Y) || !FMath::IsFinite(ImpactPoint.Z))
	{
		return false;
	}

	if (!FMath::IsFinite(GroundNormal.X) || !FMath::IsFinite(GroundNormal.Y) || !FMath::IsFinite(GroundNormal.Z) ||
		GroundNormal.IsNearlyZero() || !GroundNormal.IsNormalized())
	{
		return false;
	}

	if (!FMath::IsFinite(Clearance))
	{
		return false;
	}

	const FVector Corners[8] = {
		FVector(LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Min.Z),
		FVector(LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Max.Z),
		FVector(LocalBox.Min.X, LocalBox.Max.Y, LocalBox.Min.Z),
		FVector(LocalBox.Min.X, LocalBox.Max.Y, LocalBox.Max.Z),
		FVector(LocalBox.Max.X, LocalBox.Min.Y, LocalBox.Min.Z),
		FVector(LocalBox.Max.X, LocalBox.Min.Y, LocalBox.Max.Z),
		FVector(LocalBox.Max.X, LocalBox.Max.Y, LocalBox.Min.Z),
		FVector(LocalBox.Max.X, LocalBox.Max.Y, LocalBox.Max.Z)
	};

	float MinProjection = MAX_FLT;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector TransformedCorner = DisplayTransform.TransformPosition(Corners[Index]);
		const float Projection = FVector::DotProduct(TransformedCorner, GroundNormal);
		if (Projection < MinProjection)
		{
			MinProjection = Projection;
		}
	}

	if (!FMath::IsFinite(MinProjection) || MinProjection == MAX_FLT)
	{
		return false;
	}

	const FVector ComputedLocation = ImpactPoint + GroundNormal * (Clearance - MinProjection);
	if (!FMath::IsFinite(ComputedLocation.X) || !FMath::IsFinite(ComputedLocation.Y) || !FMath::IsFinite(ComputedLocation.Z))
	{
		return false;
	}

	OutRootLocation = ComputedLocation;
	return true;
}
