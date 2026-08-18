#include "AI/EnemyAIProfile.h"

bool UEnemyAIProfile::IsValidAIProfile(FString& OutReason) const
{
	OutReason.Empty();

	if (!FMath::IsFinite(PreferredCombatDistance) || PreferredCombatDistance <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("EnemyAIProfile '%s' has non-positive or non-finite PreferredCombatDistance (%f)."), *GetNameSafe(this), PreferredCombatDistance);
		return false;
	}

	if (!FMath::IsFinite(LateralRepositionDistance) || LateralRepositionDistance < 0.0f)
	{
		OutReason = FString::Printf(TEXT("EnemyAIProfile '%s' has negative or non-finite LateralRepositionDistance (%f)."), *GetNameSafe(this), LateralRepositionDistance);
		return false;
	}

	if (!FMath::IsFinite(RepositionAcceptanceRadius) || RepositionAcceptanceRadius <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("EnemyAIProfile '%s' has non-positive or non-finite RepositionAcceptanceRadius (%f)."), *GetNameSafe(this), RepositionAcceptanceRadius);
		return false;
	}

	if (!FMath::IsFinite(RepositionRetryDelay) || RepositionRetryDelay < 0.0f)
	{
		OutReason = FString::Printf(TEXT("EnemyAIProfile '%s' has negative or non-finite RepositionRetryDelay (%f)."), *GetNameSafe(this), RepositionRetryDelay);
		return false;
	}

	if (!FMath::IsFinite(LeashRadius) || LeashRadius <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("EnemyAIProfile '%s' has non-positive or non-finite LeashRadius (%f)."), *GetNameSafe(this), LeashRadius);
		return false;
	}

	if (!FMath::IsFinite(ApproachTimeout) || ApproachTimeout <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("EnemyAIProfile '%s' has non-positive or non-finite ApproachTimeout (%f)."), *GetNameSafe(this), ApproachTimeout);
		return false;
	}

	return true;
}

bool UEnemyAIProfile::IsValidAIProfile() const
{
	FString DummyReason;
	return IsValidAIProfile(DummyReason);
}
