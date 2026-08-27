#pragma once

#include "CoreMinimal.h"

class UAnimMontage;

/**
 * Non-reflected container holding non-owning pointers to directional reaction Montages.
 * Directional fields correspond to the attacker's relative position in target-local coordinates.
 */
struct FHitReactionFourWayMontageSet
{
	UAnimMontage* Front = nullptr;
	UAnimMontage* Back = nullptr;
	UAnimMontage* Left = nullptr;
	UAnimMontage* Right = nullptr;

	bool IsComplete() const
	{
		return Front && Back && Left && Right;
	}
};

/**
 * Pure, stateless four-way Montage selector for target-local planar attacker directions.
 * Direction semantic: Target -> Attacker (in Target's local coordinate space).
 * Evaluates XY only, ignoring Z.
 */
class FHitReactionFourWayMontageSelector
{
public:
	/**
	 * Selects a directional Montage from the provided complete set based on local planar attacker direction.
	 * If MontageSet is incomplete, LocalAttackerDirection is non-finite, or near-zero, returns nullptr.
	 */
	static UAnimMontage* SelectFromLocalAttackerDirection(
		const FVector& LocalAttackerDirection,
		const FHitReactionFourWayMontageSet& MontageSet);
};
