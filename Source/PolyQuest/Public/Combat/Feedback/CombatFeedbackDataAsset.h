// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Reaction/HitReactionClassifier.h"
#include "CombatFeedbackDataAsset.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;

/**
 * Per-tier camera shake and hit-stop settings for hit reactions.
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FCombatFeedbackTierSettings
{
	GENERATED_BODY()

	/** Local camera shake played on the receiving character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CameraShake", meta = (ToolTip = "受击方受击时触发的摄像机震屏效果类。"))
	TSubclassOf<UCameraShakeBase> ReceivedHitCameraShakeClass;

	/** Local camera shake played on the attacking player when inflicting this tier on an enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CameraShake", meta = (ToolTip = "玩家作为攻击方命中敌人时触发的摄像机震屏效果类。"))
	TSubclassOf<UCameraShakeBase> AttackerImpactCameraShakeClass;

	/** Global hit-stop duration in seconds for this tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitStop", meta = (ClampMin = "0.0", Units = "Seconds", ToolTip = "命中顿帧持续时间（秒）。"))
	float ImpactHitStopDurationSeconds = 0.03f;

	/** Global time dilation applied during hit-stop ((0.0, 1.0]). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitStop", meta = (ClampMin = "0.001", ClampMax = "1.0", ToolTip = "命中顿帧期间的时间膨胀比例（(0.0, 1.0]）。"))
	float ImpactHitStopTimeDilation = 0.1f;
};

/**
 * Defense feedback settings (guard and parry sounds/hit-stop).
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FCombatFeedbackDefenseSettings
{
	GENERATED_BODY()

	/** Sound played when a guard successfully absorbs incoming damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "格挡成功时播放的音效。"))
	TObjectPtr<USoundBase> GuardSuccessSound;

	/** Sound played when a parry successfully deflects incoming attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "弹反成功时播放的音效。"))
	TObjectPtr<USoundBase> ParrySuccessSound;

	/** Global hit-stop duration in seconds when parry succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0", Units = "Seconds", ToolTip = "弹反成功造成的命中顿帧持续时间（秒）。"))
	float ParrySuccessHitStopDurationSeconds = 0.05f;

	/** Global time dilation during parry success hit-stop ((0.0, 1.0]). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.001", ClampMax = "1.0", ToolTip = "弹反成功期间的时间膨胀比例（(0.0, 1.0]）。"))
	float ParrySuccessHitStopTimeDilation = 0.03f;
};

/**
 * Authored profile consolidating feedback assets and parameters (overlay, audio, VFX, shake, hit-stop).
 */
UCLASS(BlueprintType)
class POLYQUEST_API UCombatFeedbackDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCombatFeedbackDataAsset();

	/** Translucent global Overlay applied for one short nonlethal hit-feedback flash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overlay", meta = (ToolTip = "角色受击高亮闪烁材质覆层（Overlay Material）。"))
	TObjectPtr<UMaterialInterface> HitFeedbackOverlayMaterial;

	/** Duration for the hit-feedback Overlay before the prior Overlay is restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overlay", meta = (ClampMin = "0.0", Units = "Seconds", ToolTip = "受击材质高亮闪烁持续时间（秒）。"))
	float HitFeedbackOverlayDurationSeconds = 0.10f;

	/** Sound played when receiving non-lethal health damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio", meta = (ToolTip = "角色受到非致命实际伤害时触发的受击音效。"))
	TObjectPtr<USoundBase> ReceivedHitSound;

	/** Shared flesh impact sound played on the target location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ToolTip = "被击中时播放的共享肉体打击音效资产。"))
	TObjectPtr<USoundBase> ImpactSound;

	/** Shared blood splatter Niagara system spawned at the hit location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ToolTip = "被击中时在受击点生成的血液飞溅粒子系统。"))
	TObjectPtr<UNiagaraSystem> ImpactBloodSystem;

	/** Feedback settings for Small reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "轻度受击（Small Tier）反馈配置。"))
	FCombatFeedbackTierSettings SmallTier;

	/** Feedback settings for Big reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "重度受击（Big Tier）反馈配置。"))
	FCombatFeedbackTierSettings BigTier;

	/** Feedback settings for Launch reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "击飞受击（Launch Tier）反馈配置。"))
	FCombatFeedbackTierSettings LaunchTier;

	/** Feedback settings for defense (guard & parry). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "格挡与弹反防御反馈配置。"))
	FCombatFeedbackDefenseSettings Defense;

	/** Returns a pointer to the tier settings for the specified reaction tier, or nullptr if None/Invalid. */
	const FCombatFeedbackTierSettings* GetTierSettings(EHitReactionTier Tier) const;
};
