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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters", ToolTip = "战斗走位期间维持与目标的期望径向距离（厘米），不得超过 AttackSet 的 EngagementRange。"))
	float PreferredCombatDistance = 180.0f;

	/**
	 * Lateral perpendicular displacement applied left/right during repositioning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters", ToolTip = "战术重定位（侧移）时向左或向右施加的横向切向位移距离（厘米）。"))
	float LateralRepositionDistance = 120.0f;

	/**
	 * Nav acceptance radius for the reposition MoveTo request.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters", ToolTip = "重定位导航移动请求的到达容差半径（厘米）。"))
	float RepositionAcceptanceRadius = 40.0f;

	/**
	 * Delay before retrying a failed reposition attempt during cooldown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds", ToolTip = "冷却期间重定位尝试失败后的重试等待间隔（秒）。"))
	float RepositionRetryDelay = 0.25f;

	/**
	 * Maximum horizontal distance from HomeLocation before combat disengages and returns Home.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Leash", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters", ToolTip = "脱战警戒范围半径（厘米），离开出生点超过此水平距离时强制脱战返回。"))
	float LeashRadius = 2500.0f;

	/**
	 * Maximum duration in seconds allowed for melee approach before timing out and clearing decision.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Seconds", ToolTip = "近战接近移动允许的最长持续时间（秒），超时后放弃当前攻击决策。"))
	float ApproachTimeout = 3.0f;
};
