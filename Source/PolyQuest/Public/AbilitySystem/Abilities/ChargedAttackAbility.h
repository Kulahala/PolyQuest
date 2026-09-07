#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "GameplayTagContainer.h"
#include "ChargedAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;

/**
 * Holds a root-motion attack at an authored pose, then releases one charged hit.
 */
UCLASS()
class POLYQUEST_API UChargedAttackAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	UChargedAttackAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack", meta = (ToolTip = "蓄力攻击动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> ChargedAttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack", meta = (ToolTip = "蓄力攻击命中时施加的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack", meta = (ClampMin = "0.0", ToolTip = "蓄力攻击基础伤害数值（未蓄力倍率 1.0 时）。"))
	float BaseDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Charge", meta = (ClampMin = "0.0", ToolTip = "蓄力时间下限（秒），松手蓄力时间低于此值按此基准计算。"))
	float MinimumChargeDuration = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Charge", meta = (ClampMin = "0.0", ToolTip = "达到最大蓄力伤害与削韧所需的最长蓄力时间（秒）。"))
	float MaximumChargeDuration = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Charge", meta = (ClampMin = "1.0", ToolTip = "满蓄力时的最大伤害倍率。"))
	float MaximumDamageMultiplier = 1.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Poise", meta = (ClampMin = "0.01", ToolTip = "最低蓄力时的削韧数值。"))
	float MinimumPoiseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Poise", meta = (ClampMin = "0.01", ToolTip = "满蓄力时的最大削韧数值。"))
	float MaximumPoiseDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Motion Warping", meta = (ToolTip = "是否启用蓄力攻击近战 Root Motion 接触位移辅助（Motion Warping）。"))
	bool bUseMotionWarping = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Motion Warping", meta = (ToolTip = "Motion Warping 目标名称，需与动画 Montage 中 AnimNotifyState_MotionWarping 的 TargetName 一致。"))
	FName WarpTargetName = FName(TEXT("MeleeContact"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Motion Warping", meta = (ClampMin = "0.0", ToolTip = "触发距离下限（cm），人与目标中心距离低于此值时不触发接触位移修正。"))
	float MinTriggerDistance = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Motion Warping", meta = (ClampMin = "0.0", ToolTip = "攻击停距（cm），角色与目标接触点之间的水平期望间距。"))
	float WarpStopDistance = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Motion Warping", meta = (ClampMin = "0.0", ToolTip = "触发距离上限（cm），超出此范围不执行接触位移修正。"))
	float MaxTriggerDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|Motion Warping", meta = (ClampMin = "0.0", ClampMax = "180.0", ToolTip = "最大有效修正夹角（度），超过此角度判定为偏角过大不予修正。"))
	float MaxWarpAngleDegrees = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|VFX", meta = (ToolTip = "蓄力期间附着的 Niagara 特效资产。"))
	TObjectPtr<UNiagaraSystem> ChargeVFXSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charged Attack|VFX", meta = (ToolTip = "角色自身骨骼插槽近战接触源名称覆盖（如 Weapon_L）。未设置（NAME_None）时使用武器默认接触源。"))
	FName ChargeVFXTraceSourceName = NAME_None;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HoldReadyTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_MeleeTraceWindow> TraceWindowTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InputReleasedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InputCanceledTask;

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

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ChargeVFXComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> WaitDelayTask;

	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;
	FGameplayTag ChargedReleaseHandoffEventTag;
	FGameplayTag HoldReadyEventTag;
	FGameplayTag TraceWindowBeginEventTag;
	FGameplayTag TraceWindowEndEventTag;
	FGameplayTag DodgeCancelWindowBeginEventTag;
	FGameplayTag DodgeCancelWindowEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag DefenseCancelableStateTag;
	FGameplayTag ChargingStateTag;
	FGameplayTag DamageDataTag;
	FGameplayTag PoiseDataTag;
	float DamageMultiplier = 1.0f;
	float PoiseDamageMagnitude = 0.0f;
	bool bDodgeCancelable = false;
	bool bChargingStateApplied = false;
	bool bMontagePausedAtHoldReady = false;
	bool bHoldCancelWindowLatchedAcrossPause = false;
	bool bReleaseStarted = false;
	bool bRateWindowApplied = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnHoldReady(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputReleased(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputCanceled(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnChargeFullDelayFinished();

	void StartChargeFeedback();
	void CleanupChargeFeedback();

	void BeginRelease(float HeldDuration);
	void EndFromMontage(bool bWasCancelled);
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	bool IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const;
	bool IsChargedReleaseHandoffEvent(const FGameplayEventData* Payload, const AActor* AvatarActor) const;
	void OpenTraceWindow(const TArray<FName>& InTraceSourceNames);
	void CloseTraceWindow();
	void SetCharging(bool bShouldCharge);
	void SetDodgeCancelable(bool bShouldBeCancelable);
	void RestoreBaselineMontageRate();
	void TryApplyMeleeMotionWarpTarget(class APlayerCharacter* PlayerCharacter);
	void ResetMeleeMotionWarpState();

	FMeleeMotionWarpSnapshot MeleeMotionWarpSnapshot;

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;

#if WITH_DEV_AUTOMATION_TESTS
public:
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void Test_SetBoundMontageForTest(UAnimMontage* Montage) { ActiveMontage = Montage; }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	void SetTestBypassMontageActiveCheck(bool bBypass) { bTestBypassMontageActiveCheck = bBypass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	void Test_OnDodgeCancelWindowBegin(const FGameplayEventData& Payload) { OnDodgeCancelWindowBegin(Payload); }
	void Test_OnDodgeCancelWindowEnd(const FGameplayEventData& Payload) { OnDodgeCancelWindowEnd(Payload); }
	void Test_SimulateHoldReady()
	{
		bHoldCancelWindowLatchedAcrossPause = bDodgeCancelable;
		bMontagePausedAtHoldReady = true;
	}
	void Test_BeginRelease(float HeldDuration)
	{
		bReleaseStarted = true;
		bMontagePausedAtHoldReady = false;
	}
	bool Test_IsHoldCancelWindowLatched() const { return bHoldCancelWindowLatchedAcrossPause; }
	bool Test_IsMontagePausedAtHoldReady() const { return bMontagePausedAtHoldReady; }
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
	void Test_SetAbilityActive(bool bActive)
	{
		bIsActive = bActive;
	}

	void SetTestChargeVFXTrackingEnabled(bool bEnable) { bTestChargeVFXTrackingEnabled = bEnable; }
	bool IsTestChargeVFXTrackingEnabled() const { return bTestChargeVFXTrackingEnabled; }
	void SetTestForceSpawnNull(bool bForce) { bTestForceSpawnNull = bForce; }
	bool IsTestForceSpawnNull() const { return bTestForceSpawnNull; }

	void SetTestChargeVFXSystem(UNiagaraSystem* InSystem) { ChargeVFXSystem = InSystem; }
	UNiagaraSystem* GetTestChargeVFXSystem() const { return ChargeVFXSystem; }
	void SetTestChargeVFXTraceSourceName(FName InSourceName) { ChargeVFXTraceSourceName = InSourceName; }
	FName GetTestChargeVFXTraceSourceName() const { return ChargeVFXTraceSourceName; }
	void SetTestMaximumChargeDuration(float InDuration) { MaximumChargeDuration = InDuration; }
	float GetTestMaximumChargeDuration() const { return MaximumChargeDuration; }

	UNiagaraComponent* GetTestChargeVFXComponent() const { return ChargeVFXComponent; }
	UAbilityTask_WaitDelay* GetTestWaitDelayTask() const { return WaitDelayTask; }

	int32 GetTestStartChargeFeedbackCallCount() const { return TestStartChargeFeedbackCallCount; }
	int32 GetTestCleanupChargeFeedbackCallCount() const { return TestCleanupChargeFeedbackCallCount; }
	int32 GetTestFullCallbackCount() const { return TestFullCallbackCount; }
	bool IsTestChargeVFXActive() const { return bTestChargeVFXActive; }
	float GetTestRecordedChargePhase() const { return TestRecordedChargePhase; }
	USceneComponent* GetTestAttachParent() const { return TestAttachParent.Get(); }
	FName GetTestAttachSocketName() const { return TestAttachSocketName; }
	float GetTestDelayDuration() const { return TestDelayDuration; }

	void Test_SimulateChargeFullDelayFinished() { OnChargeFullDelayFinished(); }
	void Test_StartChargeFeedback() { StartChargeFeedback(); }
	void Test_CleanupChargeFeedback() { CleanupChargeFeedback(); }
	bool Test_IsChargingStateApplied() const { return bChargingStateApplied; }
	void Test_SetChargingStateApplied(bool bApplied) { bChargingStateApplied = bApplied; }
	bool Test_IsReleaseStarted() const { return bReleaseStarted; }
	float Test_GetDamageMultiplier() const { return DamageMultiplier; }
	float Test_GetPoiseDamageMagnitude() const { return PoiseDamageMagnitude; }

private:
	bool bTestBypassMontageActiveCheck = false;
	bool bTestChargeVFXTrackingEnabled = false;
	bool bTestForceSpawnNull = false;
	bool bTestChargeVFXActive = false;
	int32 TestStartChargeFeedbackCallCount = 0;
	int32 TestCleanupChargeFeedbackCallCount = 0;
	int32 TestFullCallbackCount = 0;
	float TestRecordedChargePhase = -1.0f;
	float TestDelayDuration = -1.0f;
	TWeakObjectPtr<USceneComponent> TestAttachParent = nullptr;
	FName TestAttachSocketName = NAME_None;
#endif
};
