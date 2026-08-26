#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

/**
 * Pure C++ non-reflected geometry helper for computing the grounded root location
 * of an unequipped world pickup actor given its definition-authored display transform
 * and static mesh asset bounds.
 */
struct FWorldPickupGrounding
{
	/**
	 * Computes the Actor Root location that places the lowest vertex of the transformed
	 * local bounding box exactly Clearance distance above the ground impact plane along GroundNormal.
	 *
	 * @param LocalBox Local bounding box of the mesh asset.
	 * @param DisplayTransform Authored relative transform applied to the mesh component.
	 * @param ImpactPoint World location of the line trace ground hit.
	 * @param GroundNormal Surface normal of the line trace ground hit.
	 * @param Clearance Desired clearance distance along the ground normal (typically 2.0cm).
	 * @param OutRootLocation Resulting Actor Root location in world space.
	 * @return True if inputs are valid, finite, and calculation succeeded; false otherwise (OutRootLocation reset to zero).
	 */
	static bool TryComputeGroundedRootLocation(
		const FBox& LocalBox,
		const FTransform& DisplayTransform,
		const FVector& ImpactPoint,
		const FVector& GroundNormal,
		float Clearance,
		FVector& OutRootLocation);
};
