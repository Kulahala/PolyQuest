// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Execution/ExecutionSnapAlignment.h"
#include "Math/UnrealMathUtility.h"

bool FExecutionSnapAlignment::IsSnapDistanceValid(const float SnapDistance)
{
	return FMath::IsFinite(SnapDistance) && SnapDistance > 0.0f;
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
