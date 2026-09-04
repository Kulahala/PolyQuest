#pragma once

#include "CoreMinimal.h"

enum class EExecutionSnapSide : uint8
{
	Front,
	Backstab
};

/**
 * Stateless mathematical helper for calculating player execution snap transforms.
 * Does not hold UObject references, query World/ASC, or manage Ability lifetimes.
 */
struct POLYQUEST_API FExecutionSnapAlignment
{
	/** Returns true only if SnapDistance is finite and strictly greater than zero. */
	static bool IsSnapDistanceValid(const float SnapDistance);

	/**
	 * Builds the snap transform for the player relative to the target.
	 * Returns false and clears OutTransform to Identity if any input is NaN/Inf,
	 * distance is invalid, target forward XY is zero, or side is unrecognized.
	 * Output Z is anchored to PlayerLocation.Z (maintaining player's grounded height).
	 */
	static bool TryBuildTransform(
		const FVector& PlayerLocation,
		const FVector& TargetLocation,
		const FVector& TargetForward,
		const float SnapDistance,
		const EExecutionSnapSide Side,
		FTransform& OutTransform);
};
