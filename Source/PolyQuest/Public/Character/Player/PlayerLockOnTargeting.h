// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AEnemyCharacter;

/** One already-qualified Enemy candidate represented in screen space. */
struct POLYQUEST_API FPlayerLockOnCandidate
{
	TWeakObjectPtr<AEnemyCharacter> TargetActor = nullptr;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	float ClockwiseAngleRadians = 0.0f;
	float PlayerScreenDistanceSquared = 0.0f;
	FString StableKey;
};

/**
 * Pure screen-space ordering and selection rules for Player lock-on.
 * World scans, GameplayTag validation, target ownership, and UI state remain on APlayerCharacter.
 */
class POLYQUEST_API FPlayerLockOnTargeting
{
public:
	static bool IsStrictlyWithinViewport(const FVector2D& ScreenPosition, const FVector2D& ViewportSize);
	static bool TryCalculateClockwiseAngle(const FVector2D& PlayerScreenPosition, const FVector2D& TargetScreenPosition, float& OutAngleRadians);
	static void SortClockwise(TArray<FPlayerLockOnCandidate>& Candidates);
	static int32 FindNearestToCursor(const TArray<FPlayerLockOnCandidate>& Candidates, const FVector2D& CursorScreenPosition);
	static int32 FindTargetIndex(const TArray<FPlayerLockOnCandidate>& Candidates, const AEnemyCharacter* TargetActor);
	static int32 FindCycledTargetIndex(const TArray<FPlayerLockOnCandidate>& Candidates, const AEnemyCharacter* CurrentTarget, int32 Direction);
};
