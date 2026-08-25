// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/Melee/CombatTeamAgent.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "BaseCharacter.generated.h"

class AController;
class UAbilitySystemComponent;
class UCharacterAttributeSet;
class UGameplayAbility;
class UMeleeTraceSourceComponent;
class UMaterialInterface;
class UMeleeWeaponTrailComponent;
struct FOnAttributeChangeData;

UCLASS(Abstract)
class POLYQUEST_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface, public ICombatTeamAgent
{
	GENERATED_BODY()

public:
	ABaseCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FGameplayTag GetCombatTeamTag_Implementation() const override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns the single fixed-weapon sample provider used by active melee abilities. */
	UMeleeTraceSourceComponent* GetMeleeTraceSource() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestCombatTeamTag(const FGameplayTag& InTag) { CombatTeamTag = InTag; }
	void ConfigureTestHitFeedbackOverlay(UMaterialInterface* InOverlayMaterial, float InDurationSeconds);
	bool IsTestHitFeedbackOverlayActive() const { return bHitFeedbackOverlayActive; }
	bool HasTestHitFeedbackOverlayTimer() const { return HitFeedbackOverlayTimerHandle.IsValid(); }
	UMaterialInterface* GetTestActiveHitFeedbackOverlayMaterial() const { return ActiveHitFeedbackOverlayMaterial.Get(); }
#endif

protected:
	void InitializeAbilityActorInfo();
	void GrantStartupAbilities();
	void BindMoveSpeedAttribute();
	void UnbindMoveSpeedAttribute();
	void OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData);
	/** Keeps the fixed v1 WeaponMesh display fixture out of camera and physics collision. */
	void DisableFixedWeaponDisplayCollision();
	/** Applies the authored global mesh Overlay briefly without altering base material slots. */
	void TriggerHitFeedbackOverlay();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (ToolTip = "角色初始化或被 Controller 附身时默认授予的初始 Ability 列表。"))
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Invalid or exactly equal tags are intentionally treated as non-hostile by the narrow melee resolver. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Team", meta = (AllowPrivateAccess = "true", ToolTip = "角色所属战斗阵营 Gameplay Tag（如 Team.Player 或 Team.Enemy）。"))
	FGameplayTag CombatTeamTag;

	/** Translucent global Overlay applied for one short nonlethal hit-feedback flash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Feedback", meta = (AllowPrivateAccess = "true", ToolTip = "角色受击时使网格体短暂高亮闪烁的全局材质覆层（Overlay Material）。"))
	TObjectPtr<UMaterialInterface> HitFeedbackOverlayMaterial;

	/** Duration for the hit-feedback Overlay before the prior Overlay is restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Feedback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds", ToolTip = "受击材质高亮闪烁持续时间（秒）。"))
	float HitFeedbackOverlayDurationSeconds = 0.10f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMeleeTraceSourceComponent> MeleeTraceSource;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Feedback", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMeleeWeaponTrailComponent> MeleeWeaponTrail;

	void ClearHitFeedbackOverlay();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreviousHitFeedbackOverlayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ActiveHitFeedbackOverlayMaterial;

	TWeakObjectPtr<UAbilitySystemComponent> MoveSpeedBoundAbilitySystemComponent;
	FDelegateHandle MoveSpeedAttributeChangedHandle;
	FTimerHandle HitFeedbackOverlayTimerHandle;
	bool bHitFeedbackOverlayActive = false;
	bool bHasLoggedInvalidHitFeedbackOverlayConfiguration = false;
};
