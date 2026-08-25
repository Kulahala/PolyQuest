#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectileDefinition.generated.h"

class UGameplayEffect;
class UStaticMesh;
class UNiagaraSystem;

/**
 * Immutable authored data definition for travelling combat projectiles.
 * Contains display, initial velocity, collision, and damage effect data.
 * Does not store runtime handles, targets, or active flight state.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UProjectileDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Validates authored parameters; returns true if all parameters are valid and finite. */
	virtual bool IsValidProjectileDefinition(FString& OutReason) const;

	/** Initial flight speed in cm/s. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0", UIMin = "100.0", ToolTip = "投射物初始飞行速度（厘米/秒）。"))
	float InitialSpeed = 3000.0f;

	/** Maximum flight speed in cm/s. Must be >= InitialSpeed and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0", UIMin = "100.0", ToolTip = "投射物最大飞行速度（厘米/秒），必须大于等于初始速度。"))
	float MaxSpeed = 3000.0f;

	/** Maximum lifespan in seconds before automatic cleanup. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.01", UIMin = "0.5", ToolTip = "投射物自动销毁前的最长存活时间（秒）。"))
	float LifespanSeconds = 5.0f;

	/** Radius of the collision sphere in cm. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Collision", meta = (ClampMin = "0.1", UIMin = "1.0", ToolTip = "投射物碰撞球体半径（厘米）。"))
	float CollisionRadius = 12.0f;

	/** The GameplayEffect applied to target ASC upon a valid hostile hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (ToolTip = "命中敌对目标 ASC 时应用的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	/** Optional static mesh visual presentation for the projectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Display", meta = (ToolTip = "投射物外观静态网格体资产。"))
	TObjectPtr<UStaticMesh> ProjectileMesh;

	/** Display rotation offset relative to the projectile forward direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Display", meta = (ToolTip = "投射物网格体相对于飞行朝向的局部旋转偏移。"))
	FRotator DisplayRotationOffset = FRotator::ZeroRotator;

	/** Display scale multiplier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Display", meta = (ToolTip = "投射物网格体缩放比例。"))
	FVector DisplayScale = FVector::OneVector;

	/** When true, Bow Release will attempt to pick a valid hostile target within angle and distance cones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (ToolTip = "是否启用指针与锁定辅助瞄准选取。"))
	bool bEnableTargetAssist = false;

	/** Maximum horizontal distance in cm for target assist selection. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "100.0", UIMin = "500.0", ToolTip = "辅助瞄准候选目标的最大水平搜索距离（厘米）；仅在 bEnableTargetAssist 启用时生效。"))
	float TargetAssistMaxDistance = 1500.0f;

	/** Maximum horizontal half-angle in degrees from pointer direction for target assist selection. Must be > 0 and <= 180. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "1.0", ClampMax = "180.0", UIMin = "10.0", UIMax = "180.0", ToolTip = "辅助瞄准相对于指针方向的最大水平半角（度）；仅在 bEnableTargetAssist 启用时生效。"))
	float TargetAssistMaxAngleDegrees = 50.0f;

	/** Maximum absolute height difference in cm between launch socket and target aim point. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "1.0", UIMin = "50.0", ToolTip = "发射点与候选目标瞄准点之间的最大绝对高度差（厘米）；仅在 bEnableTargetAssist 启用时生效。"))
	float TargetAssistMaxHeightDelta = 250.0f;

	/** Maximum absolute pitch angle in degrees from horizontal for target assist selection. Must be > 0 and < 90. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "1.0", ClampMax = "89.0", UIMin = "5.0", UIMax = "60.0", ToolTip = "辅助瞄准候选目标相对于水平面的最大绝对俯仰角（度）；仅在 bEnableTargetAssist 启用时生效。"))
	float TargetAssistMaxPitchDegrees = 45.0f;

	/** When true, projectile will perform limited homing towards the target selected at launch. Requires bEnableTargetAssist=true. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (ToolTip = "是否对发射时锁定的目标开启有限自导追踪；需同时开启 bEnableTargetAssist。"))
	bool bEnableLimitedHoming = false;

	/** Delay in seconds before homing steering activates, during which the projectile flies straight in its initial launch direction. Must be non-negative, finite, and < LifespanSeconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "0.0", UIMin = "0.0", ToolTip = "发射后直线飞行、开始追踪前的延迟时间（秒）；需开启 bEnableLimitedHoming。"))
	float HomingStartDelaySeconds = 0.06f;

	/** Maximum homing flight duration in seconds before abandoning tracking and flying straight. Must be positive, finite, and <= LifespanSeconds. Defaults to 5.0s (matching LifespanSeconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "0.01", UIMin = "0.1", ToolTip = "自导追踪的最长持续时间（秒），超时后恢复直线飞行；需开启 bEnableLimitedHoming。"))
	float HomingDurationSeconds = 5.0f;

	/** Maximum turning rate in degrees per second. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "1.0", UIMin = "30.0", ToolTip = "投射物自导转向角速度（度/秒）；需开启 bEnableLimitedHoming。"))
	float HomingTurnRateDegreesPerSecond = 120.0f;

	/** Maximum total accumulated deflection angle in degrees relative to the initial launch direction. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "1.0", UIMin = "15.0", UIMax = "360.0", ToolTip = "自导追踪过程中相对于初始发射方向允许的最大累积偏转角（度）；需开启 bEnableLimitedHoming。"))
	float HomingMaxTotalTurnDegrees = 60.0f;

	/** Optional Niagara flight trail particle system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX", meta = (ToolTip = "投射物飞行轨迹 Niagara 粒子特效系统。"))
	TObjectPtr<UNiagaraSystem> FlightTrailSystem = nullptr;

	/** Optional socket name on the projectile mesh for trail attachment. If None or missing, attaches to mesh root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX", meta = (ToolTip = "飞行拖尾特效挂接在投射物网格体上的插槽名称；留空则挂接至网格体根部。"))
	FName FlightTrailSocketName = NAME_None;

	/** Maximum duration in seconds to wait for a detached flight trail to naturally finish before destroying the projectile actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX", meta = (ClampMin = "0.01", UIMin = "0.1", ToolTip = "投射物命中或销毁后等待分离拖尾自然播放完毕的最大超时时间（秒）。"))
	float FlightTrailFinishTimeoutSeconds = 0.35f;
};
