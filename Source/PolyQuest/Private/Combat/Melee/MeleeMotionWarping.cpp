// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Melee/MeleeMotionWarping.h"
#include "Math/UnrealMathUtility.h"

bool FMeleeMotionWarpingLifecycle::IsConfigValid(const FMeleeMotionWarpConfig& Config)
{
	if (!Config.bUseMotionWarping || Config.WarpTargetName.IsNone())
	{
		return false;
	}

	if (!FMath::IsFinite(Config.WarpStopDistance)
		|| !FMath::IsFinite(Config.MaxWarpDistance)
		|| !FMath::IsFinite(Config.MaxWarpAngleDegrees))
	{
		return false;
	}

	if (Config.WarpStopDistance < 0.0f
		|| Config.MaxWarpDistance < 0.0f
		|| Config.MaxWarpAngleDegrees < 0.0f
		|| Config.MaxWarpAngleDegrees > 180.0f)
	{
		return false;
	}

	return true;
}

bool FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
	const FVector& PlayerLocation,
	const FVector& PlayerForwardVector,
	const bool bPlayerOnGround,
	const FVector& TargetLocation,
	const bool bTargetOnGround,
	const FMeleeMotionWarpConfig& Config,
	FTransform& OutWarpTransform)
{
	OutWarpTransform = FTransform::Identity;

	if (!IsConfigValid(Config))
	{
		return false;
	}

	if (!bPlayerOnGround || !bTargetOnGround)
	{
		return false;
	}

	if (!FMath::IsFinite(PlayerLocation.X) || !FMath::IsFinite(PlayerLocation.Y) || !FMath::IsFinite(PlayerLocation.Z)
		|| !FMath::IsFinite(TargetLocation.X) || !FMath::IsFinite(TargetLocation.Y) || !FMath::IsFinite(TargetLocation.Z)
		|| !FMath::IsFinite(PlayerForwardVector.X) || !FMath::IsFinite(PlayerForwardVector.Y) || !FMath::IsFinite(PlayerForwardVector.Z))
	{
		return false;
	}

	FVector ToTarget2D = FVector(TargetLocation.X - PlayerLocation.X, TargetLocation.Y - PlayerLocation.Y, 0.0f);
	const float DistSq2D = ToTarget2D.SizeSquared();
	if (DistSq2D <= KINDA_SMALL_NUMBER || !FMath::IsFinite(DistSq2D))
	{
		return false;
	}

	const float TargetDistance2D = FMath::Sqrt(DistSq2D);
	if (!FMath::IsFinite(TargetDistance2D))
	{
		return false;
	}

	// Fail-closed if target is already at or inside the desired stop distance
	if (TargetDistance2D <= Config.WarpStopDistance)
	{
		return false;
	}

	ToTarget2D /= TargetDistance2D;
	if (!FMath::IsFinite(ToTarget2D.X) || !FMath::IsFinite(ToTarget2D.Y))
	{
		return false;
	}

	// Calculate desired warp position
	FVector WarpLocation = TargetLocation - (ToTarget2D * Config.WarpStopDistance);
	WarpLocation.Z = PlayerLocation.Z;

	if (!FMath::IsFinite(WarpLocation.X) || !FMath::IsFinite(WarpLocation.Y) || !FMath::IsFinite(WarpLocation.Z))
	{
		return false;
	}

	// Maximum horizontal warp correction distance check (PlayerLocation -> WarpLocation)
	const float HorizontalCorrectionDistance = FVector::Dist2D(PlayerLocation, WarpLocation);
	if (!FMath::IsFinite(HorizontalCorrectionDistance) || HorizontalCorrectionDistance > Config.MaxWarpDistance)
	{
		return false;
	}

	// Angle check between player forward and direction to target
	FVector Forward2D = FVector(PlayerForwardVector.X, PlayerForwardVector.Y, 0.0f);
	if (!Forward2D.Normalize() || !FMath::IsFinite(Forward2D.X) || !FMath::IsFinite(Forward2D.Y))
	{
		return false;
	}

	const float Dot2D = FMath::Clamp(FVector::DotProduct(Forward2D, ToTarget2D), -1.0f, 1.0f);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot2D));
	if (!FMath::IsFinite(AngleDegrees) || AngleDegrees > Config.MaxWarpAngleDegrees)
	{
		return false;
	}

	const float WarpYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget2D.Y, ToTarget2D.X));
	if (!FMath::IsFinite(WarpYaw))
	{
		return false;
	}

	OutWarpTransform = FTransform(FRotator(0.0f, WarpYaw, 0.0f), WarpLocation);
	return true;
}
