// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Player/PlayerLockOnTargeting.h"

#include "Character/Enemy/EnemyCharacter.h"

namespace
{
	bool IsFiniteVector2D(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool IsCandidateBeforeClockwise(const FPlayerLockOnCandidate& Left, const FPlayerLockOnCandidate& Right)
	{
		if (!FMath::IsNearlyEqual(Left.ClockwiseAngleRadians, Right.ClockwiseAngleRadians))
		{
			return Left.ClockwiseAngleRadians < Right.ClockwiseAngleRadians;
		}

		if (!FMath::IsNearlyEqual(Left.PlayerScreenDistanceSquared, Right.PlayerScreenDistanceSquared))
		{
			return Left.PlayerScreenDistanceSquared < Right.PlayerScreenDistanceSquared;
		}

		return Left.StableKey.Compare(Right.StableKey, ESearchCase::CaseSensitive) < 0;
	}

	bool IsValidClockwiseAnchor(const FPlayerLockOnCandidate& Candidate)
	{
		return IsFiniteVector2D(Candidate.ScreenPosition)
			&& FMath::IsFinite(Candidate.ClockwiseAngleRadians)
			&& Candidate.ClockwiseAngleRadians >= 0.0f
			&& Candidate.ClockwiseAngleRadians < 2.0f * PI
			&& FMath::IsFinite(Candidate.PlayerScreenDistanceSquared)
			&& Candidate.PlayerScreenDistanceSquared >= 0.0f
			&& !Candidate.StableKey.IsEmpty();
	}
}

bool FPlayerLockOnTargeting::IsStrictlyWithinViewport(const FVector2D& ScreenPosition, const FVector2D& ViewportSize)
{
	return IsWithinViewportWithMargin(ScreenPosition, ViewportSize, 0.0f);
}

bool FPlayerLockOnTargeting::IsWithinViewportWithMargin(
	const FVector2D& ScreenPosition,
	const FVector2D& ViewportSize,
	const float MarginRatio)
{
	if (!IsFiniteVector2D(ScreenPosition)
		|| !IsFiniteVector2D(ViewportSize)
		|| ViewportSize.X <= 0.0f
		|| ViewportSize.Y <= 0.0f
		|| !FMath::IsFinite(MarginRatio)
		|| MarginRatio < 0.0f)
	{
		return false;
	}

	const float MinX = -MarginRatio * ViewportSize.X;
	const float MaxX = (1.0f + MarginRatio) * ViewportSize.X;
	const float MinY = -MarginRatio * ViewportSize.Y;
	const float MaxY = (1.0f + MarginRatio) * ViewportSize.Y;

	if (!FMath::IsFinite(MinX) || !FMath::IsFinite(MaxX) || !FMath::IsFinite(MinY) || !FMath::IsFinite(MaxY))
	{
		return false;
	}

	return ScreenPosition.X > MinX
		&& ScreenPosition.X < MaxX
		&& ScreenPosition.Y > MinY
		&& ScreenPosition.Y < MaxY;
}

bool FPlayerLockOnTargeting::TryCalculateClockwiseAngle(
	const FVector2D& PlayerScreenPosition,
	const FVector2D& TargetScreenPosition,
	float& OutAngleRadians)
{
	OutAngleRadians = 0.0f;
	if (!IsFiniteVector2D(PlayerScreenPosition) || !IsFiniteVector2D(TargetScreenPosition))
	{
		return false;
	}

	const FVector2D Delta = TargetScreenPosition - PlayerScreenPosition;
	float AngleRadians = FMath::Atan2(Delta.Y, Delta.X);
	if (!FMath::IsFinite(AngleRadians))
	{
		return false;
	}

	if (AngleRadians < 0.0f)
	{
		AngleRadians += 2.0f * PI;
	}

	OutAngleRadians = AngleRadians;
	return true;
}

void FPlayerLockOnTargeting::SortClockwise(TArray<FPlayerLockOnCandidate>& Candidates)
{
	Candidates.Sort([](const FPlayerLockOnCandidate& Left, const FPlayerLockOnCandidate& Right)
	{
		return IsCandidateBeforeClockwise(Left, Right);
	});
}

int32 FPlayerLockOnTargeting::FindNearestToCursor(const TArray<FPlayerLockOnCandidate>& Candidates, const FVector2D& CursorScreenPosition)
{
	if (!IsFiniteVector2D(CursorScreenPosition))
	{
		return INDEX_NONE;
	}

	int32 BestIndex = INDEX_NONE;
	float BestDistanceSquared = 0.0f;

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FPlayerLockOnCandidate& Candidate = Candidates[Index];
		if (!Candidate.TargetActor.IsValid() || !IsFiniteVector2D(Candidate.ScreenPosition))
		{
			continue;
		}

		const float DistanceSquared = FVector2D::DistSquared(Candidate.ScreenPosition, CursorScreenPosition);
		if (!FMath::IsFinite(DistanceSquared))
		{
			continue;
		}

		if (BestIndex == INDEX_NONE
			|| DistanceSquared < BestDistanceSquared
			|| (FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared)
				&& IsCandidateBeforeClockwise(Candidate, Candidates[BestIndex])))
		{
			BestIndex = Index;
			BestDistanceSquared = DistanceSquared;
		}
	}

	return BestIndex;
}

int32 FPlayerLockOnTargeting::FindTargetIndex(const TArray<FPlayerLockOnCandidate>& Candidates, const AEnemyCharacter* TargetActor)
{
	if (!TargetActor)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (Candidates[Index].TargetActor.Get() == TargetActor)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 FPlayerLockOnTargeting::FindCycledTargetIndex(
	const TArray<FPlayerLockOnCandidate>& Candidates,
	const AEnemyCharacter* CurrentTarget,
	const int32 Direction)
{
	const int32 CurrentIndex = FindTargetIndex(Candidates, CurrentTarget);
	if (CurrentIndex == INDEX_NONE || Candidates.IsEmpty() || Direction == 0)
	{
		return INDEX_NONE;
	}

	const int32 Step = Direction > 0 ? 1 : -1;
	return (CurrentIndex + Step + Candidates.Num()) % Candidates.Num();
}

int32 FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(
	const TArray<FPlayerLockOnCandidate>& Candidates,
	const FPlayerLockOnCandidate& AnchorCandidate)
{
	if (Candidates.IsEmpty() || !IsValidClockwiseAnchor(AnchorCandidate))
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (const FPlayerLockOnCandidate& Candidate = Candidates[Index]; Candidate.TargetActor.IsValid() && IsValidClockwiseAnchor(Candidate)
			&& IsCandidateBeforeClockwise(AnchorCandidate, Candidate))
		{
			return Index;
		}
	}

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (const FPlayerLockOnCandidate& Candidate = Candidates[Index]; Candidate.TargetActor.IsValid() && IsValidClockwiseAnchor(Candidate))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
