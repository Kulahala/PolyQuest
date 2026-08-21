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

	return true;
}
