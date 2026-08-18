#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAIProfile.generated.h"

/**
 * Immutable authored spatial/target behavior profile for an enemy preset.
 * Runtime state, targets, timers, request counters, and side selection remain on the Controller.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UEnemyAIProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Rejects non-finite, zero, or negative spatial configuration.
	 */
	bool IsValidAIProfile(FString& OutReason) const;
	bool IsValidAIProfile() const;

	UFUNCTION(BlueprintPure, Category = "AI|Spacing")
	float GetPreferredCombatDistance() const { return PreferredCombatDistance; }

	UFUNCTION(BlueprintPure, Category = "AI|Spacing")
	float GetLateralRepositionDistance() const { return LateralRepositionDistance; }

	UFUNCTION(BlueprintPure, Category = "AI|Spacing")
	float GetRepositionAcceptanceRadius() const { return RepositionAcceptanceRadius; }

	UFUNCTION(BlueprintPure, Category = "AI|Spacing")
	float GetRepositionRetryDelay() const { return RepositionRetryDelay; }

	UFUNCTION(BlueprintPure, Category = "AI|Leash")
	float GetLeashRadius() const { return LeashRadius; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	float GetApproachTimeout() const { return ApproachTimeout; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestPreferredCombatDistance(float InDistance) { PreferredCombatDistance = InDistance; }
	void SetTestLateralRepositionDistance(float InDistance) { LateralRepositionDistance = InDistance; }
	void SetTestRepositionAcceptanceRadius(float InRadius) { RepositionAcceptanceRadius = InRadius; }
	void SetTestRepositionRetryDelay(float InDelay) { RepositionRetryDelay = InDelay; }
	void SetTestLeashRadius(float InRadius) { LeashRadius = InRadius; }
	void SetTestApproachTimeout(float InTimeout) { ApproachTimeout = InTimeout; }
#endif

private:
	/**
	 * Radial distance from target to maintain during combat spacing.
	 * Must not exceed AttackSet EngagementRange when validated on Controller.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters"))
	float PreferredCombatDistance = 180.0f;

	/**
	 * Lateral perpendicular displacement applied left/right during repositioning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float LateralRepositionDistance = 120.0f;

	/**
	 * Nav acceptance radius for the reposition MoveTo request.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters"))
	float RepositionAcceptanceRadius = 40.0f;

	/**
	 * Delay before retrying a failed reposition attempt during cooldown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
	float RepositionRetryDelay = 0.25f;

	/**
	 * Maximum horizontal distance from HomeLocation before combat disengages and returns Home.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Leash", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters"))
	float LeashRadius = 2500.0f;

	/**
	 * Maximum duration in seconds allowed for melee approach before timing out and clearing decision.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Seconds"))
	float ApproachTimeout = 3.0f;
};
