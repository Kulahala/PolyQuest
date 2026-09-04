// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Execution/ExecutionSnapAlignment.h"
#include "Math/UnrealMathUtility.h"

bool FExecutionSnapAlignment::IsSnapDistanceValid(const float SnapDistance)
{
	return FMath::IsFinite(SnapDistance) && SnapDistance > 0.0f;
}

bool FExecutionSnapAlignment::IsExecutionDistanceRangeValid(
	const float MinDist,
	const float MaxDist,
	const float SnapDist,
	FString* OutFailureReason)
{
	if (OutFailureReason)
	{
		OutFailureReason->Empty();
	}

	// 1. All values must be finite numbers
	if (!FMath::IsFinite(MinDist) || !FMath::IsFinite(MaxDist) || !FMath::IsFinite(SnapDist))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("Execution distance values must be finite numbers.");
		}
		return false;
	}

	// 2. MinDist must be non-negative (>= 0)
	if (MinDist < 0.0f)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("MinExecutionDistance must be non-negative.");
		}
		return false;
	}

	// 3. MaxDist must be strictly greater than MinDist
	if (MaxDist <= MinDist)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("MaxExecutionDistance must be strictly greater than MinExecutionDistance.");
		}
		return false;
	}

	// 4. SnapDist must be strictly greater than zero (> 0)
	if (SnapDist <= 0.0f)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("ExecutionSnapDistance must be strictly positive.");
		}
		return false;
	}

	// 5. SnapDist must lie within [MinDist, MaxDist]
	if (SnapDist < MinDist || SnapDist > MaxDist)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("ExecutionSnapDistance must be within [MinExecutionDistance, MaxExecutionDistance].");
		}
		return false;
	}

	// 6. MaxDist must not exceed Native Hard Cap of 250cm
	constexpr float NativeMaxDistanceHardCap = 250.0f;
	if (MaxDist > NativeMaxDistanceHardCap)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("MaxExecutionDistance exceeds native hard cap of 250cm.");
		}
		return false;
	}

	return true;
}

bool FExecutionSnapAlignment::TryBuildTransform(
	const FVector& PlayerLocation,
	const FVector& TargetLocation,
	const FVector& TargetForward,
	const float SnapDistance,
	const EExecutionSnapSide Side,
	FTransform& OutTransform)
{
	OutTransform = FTransform::Identity;

	if (!FMath::IsFinite(PlayerLocation.X) || !FMath::IsFinite(PlayerLocation.Y) || !FMath::IsFinite(PlayerLocation.Z)
		|| !FMath::IsFinite(TargetLocation.X) || !FMath::IsFinite(TargetLocation.Y) || !FMath::IsFinite(TargetLocation.Z)
		|| !FMath::IsFinite(TargetForward.X) || !FMath::IsFinite(TargetForward.Y) || !FMath::IsFinite(TargetForward.Z))
	{
		return false;
	}

	if (!IsSnapDistanceValid(SnapDistance))
	{
		return false;
	}

	if (Side != EExecutionSnapSide::Front && Side != EExecutionSnapSide::Backstab)
	{
		return false;
	}

	FVector2D Forward2D(TargetForward.X, TargetForward.Y);
	if (!Forward2D.Normalize() || Forward2D.IsNearlyZero() || !FMath::IsFinite(Forward2D.X) || !FMath::IsFinite(Forward2D.Y))
	{
		return false;
	}

	const bool bIsFront = (Side == EExecutionSnapSide::Front);
	const FVector SnapLocation = bIsFront
		? FVector(TargetLocation.X + Forward2D.X * SnapDistance, TargetLocation.Y + Forward2D.Y * SnapDistance, PlayerLocation.Z)
		: FVector(TargetLocation.X - Forward2D.X * SnapDistance, TargetLocation.Y - Forward2D.Y * SnapDistance, PlayerLocation.Z);

	const FVector Facing2D = bIsFront
		? FVector(-Forward2D.X, -Forward2D.Y, 0.0f)
		: FVector(Forward2D.X, Forward2D.Y, 0.0f);

	const float YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Facing2D.Y, Facing2D.X));
	if (!FMath::IsFinite(YawDegrees))
	{
		return false;
	}

	OutTransform = FTransform(FRotator(0.0f, YawDegrees, 0.0f), SnapLocation);
	return true;
}
