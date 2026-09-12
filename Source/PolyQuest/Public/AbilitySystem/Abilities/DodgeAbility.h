#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "DodgeAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

class UDodgeAbility;

/** Transient receiver scoped to one RateWindow playback binding. */
UCLASS(Transient)
class POLYQUEST_API UDodgeRateWindowContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UDodgeAbility> OwningAbility;
	uint32 Token = 0;

	UFUNCTION()
	void OnBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnEnd(FGameplayEventData Payload);
};

/**
 * Ground-only Root Motion Dodge with NotifyState-timed invulnerability.
 */
UCLASS()
class POLYQUEST_API UDodgeAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	UDodgeAbility();

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

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationOwnedTags() const { return ActivationOwnedTags; }
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	FGameplayTag GetTestDodgeCancelableStateTag() const { return DodgeCancelableStateTag; }
	FGameplayTag GetTestAttackingStateTag() const { return AttackingStateTag; }
	FGameplayTag GetTestDodgingStateTag() const { return DodgingStateTag; }
	FGameplayTag GetTestHitReactingStateTag() const { return HitReactingStateTag; }
	FGameplayTag GetTestPlayerLaunchReactionAbilityTag() const { return PlayerLaunchReactionAbilityTag; }
	bool GetTestDodgeCancelable() const { return bDodgeCancelable; }
	bool GetTestRetriggerInstancedAbility() const { return bRetriggerInstancedAbility; }
	void SetTestActiveMontage(UAnimMontage* Montage) { ActiveMontage = Montage; }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInstance) { BoundAnimInstance = AnimInstance; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void TestOnCancelWindowBegin(const FGameplayEventData& Payload) { OnCancelWindowBegin(Payload); }
	void TestOnCancelWindowEnd(const FGameplayEventData& Payload) { OnCancelWindowEnd(Payload); }
	void TestSetDodgeCancelable(bool bShouldCancel) { SetDodgeCancelable(bShouldCancel); }
	bool Test_IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const { return IsGameplayEventFromActiveMontage(Payload); }
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "翻滚/闪避动作动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "闪避无敌帧窗口期间施加的无敌状态 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> InvulnerabilityGameplayEffectClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InvulnerabilityBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InvulnerabilityEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FGameplayTag PrimaryAttackAbilityTag;
	FGameplayTag LightAttackAbilityTag;
	FGameplayTag ChargedAttackAbilityTag;
	FGameplayTag SprintAttackAbilityTag;
	FGameplayTag CancelableByDodgeAbilityTag;
	FGameplayTag PlayerLaunchReactionAbilityTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag DodgingStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag InvulnerabilityBeginEventTag;
	FGameplayTag InvulnerabilityEndEventTag;
	FGameplayTag CancelWindowBeginEventTag;
	FGameplayTag CancelWindowEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FActiveGameplayEffectHandle InvulnerabilityEffectHandle;
	bool bEndAbilityRequested = false;
	bool bDodgeCancelable = false;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnInvulnerabilityBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnInvulnerabilityEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnCancelWindowEnd(FGameplayEventData Payload);

	void OnRateWindowBegin(const FGameplayEventData& Payload);

	void OnRateWindowEnd(const FGameplayEventData& Payload);

	void EndFromMontage(bool bWasCancelled);
	void ClearInvulnerabilityEffect();
	void SetDodgeCancelable(bool bShouldCancel);
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;

private:
	friend class UDodgeRateWindowContext;

	UPROPERTY(Transient)
	TObjectPtr<UDodgeRateWindowContext> RateWindowContext;

	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;
	TWeakObjectPtr<UAnimInstance> RateWindowAnimInstance;
	TWeakObjectPtr<UAnimMontage> RateWindowMontage;
	uint32 RateWindowBindingToken = 0;
	int32 RateWindowMontageInstanceID = INDEX_NONE;

	bool BindRateWindow(UAnimInstance* AnimInstance, UAnimMontage* Montage);
	bool HasOwnedRateWindowMontageInstance() const;
	void ClearRateWindow();

#if WITH_DEV_AUTOMATION_TESTS
public:
	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	UDodgeRateWindowContext* GetTestRateWindowContext() const { return RateWindowContext.Get(); }
	int32 GetTestRateWindowMontageInstanceID() const { return RateWindowMontageInstanceID; }
	bool HasTestRateWindowTasks() const { return RateWindowBeginTask != nullptr || RateWindowEndTask != nullptr; }
	void TestClearRateWindow() { ClearRateWindow(); }
#endif
};
