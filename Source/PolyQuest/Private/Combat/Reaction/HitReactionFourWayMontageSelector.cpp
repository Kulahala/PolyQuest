#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"

#include "Animation/AnimMontage.h"

UAnimMontage* FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(
	const FVector& LocalAttackerDirection,
	const FHitReactionFourWayMontageSet& MontageSet)
{
	if (!MontageSet.IsComplete())
	{
		return nullptr;
	}

	const FVector PlanarDirection(LocalAttackerDirection.X, LocalAttackerDirection.Y, 0.0f);

	if (!FMath::IsFinite(PlanarDirection.X) || !FMath::IsFinite(PlanarDirection.Y) || PlanarDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const float AbsX = FMath::Abs(PlanarDirection.X);
	const float AbsY = FMath::Abs(PlanarDirection.Y);

	if (AbsX >= AbsY)
	{
		return PlanarDirection.X >= 0.0f ? MontageSet.Front : MontageSet.Back;
	}

	return PlanarDirection.Y >= 0.0f ? MontageSet.Right : MontageSet.Left;
}
