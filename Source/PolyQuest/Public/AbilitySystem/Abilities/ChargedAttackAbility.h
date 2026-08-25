#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "ChargedAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

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

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;

#if WITH_DEV_AUTOMATION_TESTS
public:
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void Test_SetBoundMontageForTest(UAnimMontage* Montage) { ActiveMontage = Montage; }
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
#endif
};
