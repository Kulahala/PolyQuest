#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "SprintAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

/**
 * A Root Motion attack that may begin only from a real active Sprint state.
 * It owns one Stamina transaction and one Notify-timed weapon trace window.
 */
UCLASS()
class POLYQUEST_API USprintAttackAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	USprintAttackAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack", meta = (ToolTip = "冲刺攻击动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> SprintAttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack", meta = (ToolTip = "冲刺攻击判定命中时施加的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Motion Warping", meta = (ToolTip = "是否启用冲刺攻击近战 Root Motion 接触位移辅助（Motion Warping）。"))
	bool bUseMotionWarping = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Motion Warping", meta = (ToolTip = "Motion Warping 目标名称，需与动画 Montage 中 AnimNotifyState_MotionWarping 的 TargetName 一致。"))
	FName WarpTargetName = FName(TEXT("MeleeContact"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Motion Warping", meta = (ClampMin = "0.0", ToolTip = "触发距离下限（cm），人与目标中心距离低于此值时不触发接触位移修正。"))
	float MinTriggerDistance = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Motion Warping", meta = (ClampMin = "0.0", ToolTip = "攻击停距（cm），角色与目标接触点之间的水平期望间距。"))
	float WarpStopDistance = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Motion Warping", meta = (ClampMin = "0.0", ToolTip = "触发距离上限（cm），超出此范围不执行接触位移修正。"))
	float MaxTriggerDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Motion Warping", meta = (ClampMin = "0.0", ClampMax = "180.0", ToolTip = "最大有效修正夹角（度），超过此角度判定为偏角过大不予修正。"))
	float MaxWarpAngleDegrees = 60.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_MeleeTraceWindow> TraceWindowTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FGameplayTag SprintStateTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag MovementInputBlockedTag;
	FGameplayTag JumpInputBlockedTag;
	FGameplayTag StaminaRegenBlockedTag;
	FGameplayTag TraceWindowBeginEventTag;
	FGameplayTag TraceWindowEndEventTag;
	FGameplayTag DodgeCancelWindowBeginEventTag;
	FGameplayTag DodgeCancelWindowEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag DefenseCancelableStateTag;
	bool bDodgeCancelable = false;
	bool bRuntimeActionTagsApplied = false;
	bool bRateWindowApplied = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnTraceWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);

	void EndFromMontage(bool bWasCancelled);
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	void OpenTraceWindow(const TArray<FName>& InTraceSourceNames);
	void CloseTraceWindow();
	void SetDodgeCancelable(bool bShouldBeCancelable);
	void SetRuntimeActionTags(bool bShouldApply);
	void RestoreBaselineMontageRate();
	void TryApplyMeleeMotionWarpTarget(class APlayerCharacter* PlayerCharacter);
	void ResetMeleeMotionWarpState();

	FMeleeMotionWarpSnapshot MeleeMotionWarpSnapshot;

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;

#if WITH_DEV_AUTOMATION_TESTS
public:
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	void SetTestActiveMontage(UAnimMontage* InMontage) { ActiveMontage = InMontage; }
	void SetTestBypassMontageActiveCheck(bool bBypass) { bTestBypassMontageActiveCheck = bBypass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	void Test_TryApplyMeleeMotionWarpTarget(APlayerCharacter* InPlayer) { TryApplyMeleeMotionWarpTarget(InPlayer); }
	void TestResetMeleeMotionWarpState() { ResetMeleeMotionWarpState(); }
	bool HasTestMeleeMotionWarpSnapshot() const { return MeleeMotionWarpSnapshot.CapturedTarget.IsValid(); }
	bool HasTestMeleeMotionWarpCaptureAttempted() const { return MeleeMotionWarpSnapshot.bAttemptedCapture; }
	FVector GetTestMeleeMotionWarpCapturedLocation() const { return MeleeMotionWarpSnapshot.CapturedTargetLocation; }
	bool GetTestMeleeMotionWarpCapturedOnGround() const { return MeleeMotionWarpSnapshot.bCapturedTargetOnGround; }
	void SetTestMotionWarpConfig(bool bInUseWarp, FName InTargetName, float InMinDist, float InStopDist, float InMaxDist, float InMaxAngle)
	{
		bUseMotionWarping = bInUseWarp;
		WarpTargetName = InTargetName;
		MinTriggerDistance = InMinDist;
		WarpStopDistance = InStopDist;
		MaxTriggerDistance = InMaxDist;
		MaxWarpAngleDegrees = InMaxAngle;
	}
	void SetTestEndAbilityRequested(bool bRequested) { bEndAbilityRequested = bRequested; }
	bool Test_IsRuntimeActionTagsApplied() const { return bRuntimeActionTagsApplied; }
	bool Test_IsDodgeCancelable() const { return bDodgeCancelable; }

private:
	bool bTestBypassMontageActiveCheck = false;
#endif
};
