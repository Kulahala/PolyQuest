#include "AbilitySystem/Abilities/LaunchFacingSmoothingState.h"

#include "Combat/Reaction/HitReactionImpactResolver.h"

void FLaunchFacingSmoothingState::Reset()
{
	StartYaw = 0.0f;
	TargetYaw = 0.0f;
	LaunchVelocity = FVector::ZeroVector;
	bHasFrozenLaunch = false;
	bCommitReceived = false;
	bTurnCompleted = false;
	bLaunchIssued = false;
}

bool FLaunchFacingSmoothingState::TryFreeze(
	const FVector& LocalAttackerDirection,
	float ImpactReferenceYaw,
	float HorizontalSpeed,
	float VerticalSpeed)
{
	Reset();

	float ResolvedFacingYaw = 0.0f;
	FVector ResolvedLaunchVelocity = FVector::ZeroVector;
	if (!FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(
			LocalAttackerDirection,
			ImpactReferenceYaw,
			HorizontalSpeed,
			VerticalSpeed,
			ResolvedFacingYaw,
			ResolvedLaunchVelocity))
	{
		return false;
	}

	StartYaw = ImpactReferenceYaw;
	TargetYaw = ResolvedFacingYaw;
	LaunchVelocity = ResolvedLaunchVelocity;
	bHasFrozenLaunch = true;
	return true;
}

bool FLaunchFacingSmoothingState::TryReceiveCommit()
{
	if (!bHasFrozenLaunch || bCommitReceived || bLaunchIssued)
	{
		return false;
	}

	bCommitReceived = true;
	return true;
}

void FLaunchFacingSmoothingState::MarkTurnCompleted()
{
	if (bHasFrozenLaunch && !bLaunchIssued)
	{
		bTurnCompleted = true;
	}
}

bool FLaunchFacingSmoothingState::TryConsumeLaunchVelocity(FVector& OutLaunchVelocity)
{
	OutLaunchVelocity = FVector::ZeroVector;

	if (!bHasFrozenLaunch || !bCommitReceived || !bTurnCompleted || bLaunchIssued)
	{
		return false;
	}

	bLaunchIssued = true;
	OutLaunchVelocity = LaunchVelocity;
	return true;
}
