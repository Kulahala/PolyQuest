#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AEnemyCharacter;

/**
 * Authored configuration struct for melee motion warping contact assist.
 */
struct POLYQUEST_API FMeleeMotionWarpConfig
{
	bool bUseMotionWarping = false;
	FName WarpTargetName = FName(TEXT("MeleeContact"));
	float MinTriggerDistance = 190.0f;
	float WarpStopDistance = 190.0f;
	float MaxTriggerDistance = 300.0f;
	float MaxWarpAngleDegrees = 60.0f;
};

/**
 * Non-reflected, per-ability-instance snapshot for one-shot motion warping target evaluation.
 */
struct POLYQUEST_API FMeleeMotionWarpSnapshot
{
	bool bAttemptedCapture = false;
	TWeakObjectPtr<AEnemyCharacter> CapturedTarget = nullptr;
	FVector CapturedTargetLocation = FVector::ZeroVector;
	bool bCapturedTargetOnGround = false;

	void Reset()
	{
		bAttemptedCapture = false;
		CapturedTarget = nullptr;
		CapturedTargetLocation = FVector::ZeroVector;
		bCapturedTargetOnGround = false;
	}
};

/**
 * Stateless geometric helper and pure validator for melee motion warping.
 * Does not hold UObject references, query Lock-On/ASC/World, or manage Ability lifetimes.
 */
struct POLYQUEST_API FMeleeMotionWarpingLifecycle
{
	static bool IsConfigValid(const FMeleeMotionWarpConfig& Config);

	static bool EvaluateMeleeMotionWarpTransform(
		const FVector& PlayerLocation,
		const FVector& PlayerForwardVector,
		bool bPlayerOnGround,
		const FVector& TargetLocation,
		bool bTargetOnGround,
		const FMeleeMotionWarpConfig& Config,
		FTransform& OutWarpTransform);
};
