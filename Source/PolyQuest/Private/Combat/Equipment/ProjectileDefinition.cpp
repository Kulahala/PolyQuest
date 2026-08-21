#include "Combat/Equipment/ProjectileDefinition.h"

#include "GameplayEffect.h"

bool UProjectileDefinition::IsValidProjectileDefinition(FString& OutReason) const
{
	OutReason.Empty();

	if (!FMath::IsFinite(InitialSpeed) || InitialSpeed <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("InitialSpeed (%.2f) must be positive and finite."), InitialSpeed);
		return false;
	}

	if (!FMath::IsFinite(MaxSpeed) || MaxSpeed < InitialSpeed)
	{
		OutReason = FString::Printf(TEXT("MaxSpeed (%.2f) must be finite and >= InitialSpeed (%.2f)."), MaxSpeed, InitialSpeed);
		return false;
	}

	if (!FMath::IsFinite(LifespanSeconds) || LifespanSeconds <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("LifespanSeconds (%.2f) must be positive and finite."), LifespanSeconds);
		return false;
	}

	if (!FMath::IsFinite(CollisionRadius) || CollisionRadius <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("CollisionRadius (%.2f) must be positive and finite."), CollisionRadius);
		return false;
	}

	if (!DamageGameplayEffectClass)
	{
		OutReason = TEXT("DamageGameplayEffectClass is null.");
		return false;
	}

	if (!FMath::IsFinite(DisplayRotationOffset.Pitch) || !FMath::IsFinite(DisplayRotationOffset.Yaw) || !FMath::IsFinite(DisplayRotationOffset.Roll))
	{
		OutReason = TEXT("DisplayRotationOffset contains non-finite values.");
		return false;
	}

	if (!FMath::IsFinite(DisplayScale.X) || !FMath::IsFinite(DisplayScale.Y) || !FMath::IsFinite(DisplayScale.Z)
		|| FMath::IsNearlyZero(DisplayScale.X) || FMath::IsNearlyZero(DisplayScale.Y) || FMath::IsNearlyZero(DisplayScale.Z))
	{
		OutReason = TEXT("DisplayScale must be non-zero and finite.");
		return false;
	}

	if (bEnableLimitedHoming && !bEnableTargetAssist)
	{
		OutReason = TEXT("bEnableLimitedHoming requires bEnableTargetAssist to be enabled.");
		return false;
	}

	if (bEnableTargetAssist)
	{
		if (!FMath::IsFinite(TargetAssistMaxDistance) || TargetAssistMaxDistance <= 0.0f)
		{
			OutReason = FString::Printf(TEXT("TargetAssistMaxDistance (%.2f) must be positive and finite."), TargetAssistMaxDistance);
			return false;
		}

		if (!FMath::IsFinite(TargetAssistMaxAngleDegrees) || TargetAssistMaxAngleDegrees <= 0.0f || TargetAssistMaxAngleDegrees > 180.0f)
		{
			OutReason = FString::Printf(TEXT("TargetAssistMaxAngleDegrees (%.2f) must be > 0 and <= 180."), TargetAssistMaxAngleDegrees);
			return false;
		}

		if (!FMath::IsFinite(TargetAssistMaxHeightDelta) || TargetAssistMaxHeightDelta <= 0.0f)
		{
			OutReason = FString::Printf(TEXT("TargetAssistMaxHeightDelta (%.2f) must be positive and finite."), TargetAssistMaxHeightDelta);
			return false;
		}

		if (!FMath::IsFinite(TargetAssistMaxPitchDegrees) || TargetAssistMaxPitchDegrees <= 0.0f || TargetAssistMaxPitchDegrees >= 90.0f)
		{
			OutReason = FString::Printf(TEXT("TargetAssistMaxPitchDegrees (%.2f) must be > 0 and < 90."), TargetAssistMaxPitchDegrees);
			return false;
		}
	}

	if (bEnableLimitedHoming)
	{
		if (!FMath::IsFinite(HomingStartDelaySeconds) || HomingStartDelaySeconds < 0.0f || HomingStartDelaySeconds >= LifespanSeconds)
		{
			OutReason = FString::Printf(TEXT("HomingStartDelaySeconds (%.2f) must be non-negative, finite, and < LifespanSeconds (%.2f)."), HomingStartDelaySeconds, LifespanSeconds);
			return false;
		}

		if (!FMath::IsFinite(HomingDurationSeconds) || HomingDurationSeconds <= 0.0f || HomingDurationSeconds > LifespanSeconds)
		{
			OutReason = FString::Printf(TEXT("HomingDurationSeconds (%.2f) must be positive, finite, and <= LifespanSeconds (%.2f)."), HomingDurationSeconds, LifespanSeconds);
			return false;
		}

		if (!FMath::IsFinite(HomingTurnRateDegreesPerSecond) || HomingTurnRateDegreesPerSecond <= 0.0f)
		{
			OutReason = FString::Printf(TEXT("HomingTurnRateDegreesPerSecond (%.2f) must be positive and finite."), HomingTurnRateDegreesPerSecond);
			return false;
		}

		if (!FMath::IsFinite(HomingMaxTotalTurnDegrees) || HomingMaxTotalTurnDegrees <= 0.0f)
		{
			OutReason = FString::Printf(TEXT("HomingMaxTotalTurnDegrees (%.2f) must be positive and finite."), HomingMaxTotalTurnDegrees);
			return false;
		}
	}

	return true;
}
