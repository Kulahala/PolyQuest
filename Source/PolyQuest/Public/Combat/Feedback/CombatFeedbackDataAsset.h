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
 * Per-tier camera shake settings for Player hit reactions (received hit and attacker impact).
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FPlayerCombatFeedbackTierSettings
{
	GENERATED_BODY()

	/** Local camera shake played on the receiving player character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CameraShake", meta = (ToolTip = "玩家受击时触发的摄像机震屏效果类。"))
	TSubclassOf<UCameraShakeBase> ReceivedHitCameraShakeClass;

	/** Local camera shake played on the attacking player when inflicting this tier on an enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CameraShake", meta = (ToolTip = "玩家作为攻击方命中敌人时触发的摄像机震屏效果类。"))
	TSubclassOf<UCameraShakeBase> AttackerImpactCameraShakeClass;
};

/**
 * Per-tier hit-stop settings for Enemy hit reactions.
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FEnemyCombatFeedbackTierSettings
{
	GENERATED_BODY()

	/** Global hit-stop duration in seconds for this tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitStop", meta = (ClampMin = "0.0", Units = "Seconds", ToolTip = "命中顿帧持续时间（秒）。"))
	float ImpactHitStopDurationSeconds = 0.03f;

	/** Global time dilation applied during hit-stop ((0.0, 1.0]). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitStop", meta = (ClampMin = "0.001", ClampMax = "1.0", ToolTip = "命中顿帧期间的时间膨胀比例（(0.0, 1.0]）。"))
	float ImpactHitStopTimeDilation = 0.1f;
};

/**
 * Defense feedback settings (guard and parry sounds/hit-stop) owned by Player.
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FPlayerCombatFeedbackDefenseSettings
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
 * Execution feedback settings (attacker impact camera shake) owned by Player.
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FPlayerCombatFeedbackExecutionSettings
{
	GENERATED_BODY()

	/** Local camera shake played on the attacking player character upon successful execution impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ToolTip = "玩家处决命中成功时作为攻击方触发的摄像机震屏效果类。"))
	TSubclassOf<UCameraShakeBase> AttackerImpactCameraShakeClass;
};

/**
 * Execution feedback settings (impact hit-stop) for Enemy.
 */
USTRUCT(BlueprintType)
struct POLYQUEST_API FEnemyCombatFeedbackExecutionSettings
{
	GENERATED_BODY()

	/** Global hit-stop duration in seconds for execution impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "0.0", Units = "Seconds", ToolTip = "处决命中顿帧持续时间（秒）。"))
	float ImpactHitStopDurationSeconds = 0.05f;

	/** Global time dilation applied during execution hit-stop ((0.0, 1.0]). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "0.001", ClampMax = "1.0", ToolTip = "处决命中顿帧期间的时间膨胀比例（(0.0, 1.0]）。"))
	float ImpactHitStopTimeDilation = 0.03f;
};

/**
 * Common base profile consolidating shared hit-feedback assets and parameters (overlay flash).
 * Abstract root data asset for typed Player and Enemy feedback profiles.
 */
UCLASS(Abstract, BlueprintType)
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
};

/**
 * Authored profile consolidating player-specific combat feedback (shake, received hit sound, defense audio/hit-stop).
 */
UCLASS(BlueprintType)
class POLYQUEST_API UPlayerCombatFeedbackDataAsset : public UCombatFeedbackDataAsset
{
	GENERATED_BODY()

public:
	UPlayerCombatFeedbackDataAsset();

	/** Sound played when player receives non-lethal health damage from an enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio", meta = (ToolTip = "玩家受到非致命实际伤害时触发的受击音效。"))
	TObjectPtr<USoundBase> ReceivedHitSound;

	/** Feedback settings for Small reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "轻度受击（Small Tier）震屏配置。"))
	FPlayerCombatFeedbackTierSettings SmallTier;

	/** Feedback settings for Big reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "重度受击（Big Tier）震屏配置。"))
	FPlayerCombatFeedbackTierSettings BigTier;

	/** Feedback settings for Launch reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "击飞受击（Launch Tier）震屏配置。"))
	FPlayerCombatFeedbackTierSettings LaunchTier;

	/** Feedback settings for player defense (guard & parry). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "玩家格挡与弹反防御反馈配置。"))
	FPlayerCombatFeedbackDefenseSettings Defense;

	/** Feedback settings for player execution impact (attacker camera shake). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ToolTip = "玩家处决命中攻击方震屏配置。"))
	FPlayerCombatFeedbackExecutionSettings Execution;

	/** Returns a pointer to the player tier settings for the specified reaction tier, or nullptr if None/Invalid. */
	const FPlayerCombatFeedbackTierSettings* GetTierSettings(EHitReactionTier Tier) const;
};

/**
 * Authored profile consolidating enemy-specific combat feedback (impact sound, blood VFX, impact hit-stop).
 */
UCLASS(BlueprintType)
class POLYQUEST_API UEnemyCombatFeedbackDataAsset : public UCombatFeedbackDataAsset
{
	GENERATED_BODY()

public:
	UEnemyCombatFeedbackDataAsset();

	/** Shared flesh impact sound played on the enemy target location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ToolTip = "敌人被击中时播放的肉体打击音效资产。"))
	TObjectPtr<USoundBase> ImpactSound;

	/** Shared blood splatter Niagara system spawned at the enemy hit location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ToolTip = "敌人被击中时在受击点生成的血液飞溅粒子系统。"))
	TObjectPtr<UNiagaraSystem> ImpactBloodSystem;

	/** Feedback settings for Small reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "轻度受击（Small Tier）命中顿帧配置。"))
	FEnemyCombatFeedbackTierSettings SmallTier;

	/** Feedback settings for Big reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "重度受击（Big Tier）命中顿帧配置。"))
	FEnemyCombatFeedbackTierSettings BigTier;

	/** Feedback settings for Launch reaction tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers", meta = (ToolTip = "击飞受击（Launch Tier）命中顿帧配置。"))
	FEnemyCombatFeedbackTierSettings LaunchTier;

	/** Feedback settings for enemy execution impact (hit-stop). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ToolTip = "敌人受处决命中顿帧配置。"))
	FEnemyCombatFeedbackExecutionSettings Execution;

	/** Returns a pointer to the enemy tier settings for the specified reaction tier, or nullptr if None/Invalid. */
	const FEnemyCombatFeedbackTierSettings* GetTierSettings(EHitReactionTier Tier) const;
};
