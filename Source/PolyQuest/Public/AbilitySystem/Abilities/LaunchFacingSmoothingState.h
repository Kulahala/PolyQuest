#pragma once

#include "CoreMinimal.h"

/**
 * Non-reflected internal lifecycle state for launch facing smoothing.
 * Owns the frozen snapshot parameters, commit/turn completion flags, and the
 * single-use token to consume launch velocity.
 */
struct POLYQUEST_API FLaunchFacingSmoothingState
{
	void Reset();

	bool TryFreeze(
		const FVector& LocalAttackerDirection,
		float ImpactReferenceYaw,
		float HorizontalSpeed,
		float VerticalSpeed);

	bool TryReceiveCommit();

	void MarkTurnCompleted();

	bool TryConsumeLaunchVelocity(FVector& OutLaunchVelocity);

	float GetStartYaw() const { return StartYaw; }
	float GetTargetYaw() const { return TargetYaw; }
	const FVector& GetLaunchVelocity() const { return LaunchVelocity; }
	bool HasFrozenLaunch() const { return bHasFrozenLaunch; }
	bool IsCommitReceived() const { return bCommitReceived; }
	bool IsTurnCompleted() const { return bTurnCompleted; }
	bool IsLaunchIssued() const { return bLaunchIssued; }

private:
	float StartYaw = 0.0f;
	float TargetYaw = 0.0f;
	FVector LaunchVelocity = FVector::ZeroVector;
	bool bHasFrozenLaunch = false;
	bool bCommitReceived = false;
	bool bTurnCompleted = false;
	bool bLaunchIssued = false;
};
