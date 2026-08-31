#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "DodgeAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

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
	bool GetTestRateWindowApplied() const { return bRateWindowApplied; }
	bool GetTestRetriggerInstancedAbility() const { return bRetriggerInstancedAbility; }
	void SetTestActiveMontage(UAnimMontage* Montage) { ActiveMontage = Montage; }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInstance) { BoundAnimInstance = AnimInstance; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void TestOnCancelWindowBegin(const FGameplayEventData& Payload) { OnCancelWindowBegin(Payload); }
	void TestOnCancelWindowEnd(const FGameplayEventData& Payload) { OnCancelWindowEnd(Payload); }
	void TestOnRateWindowBegin(const FGameplayEventData& Payload) { OnRateWindowBegin(Payload); }
	void TestOnRateWindowEnd(const FGameplayEventData& Payload) { OnRateWindowEnd(Payload); }
	void TestSetDodgeCancelable(bool bShouldCancel) { SetDodgeCancelable(bShouldCancel); }
	void TestRestoreBaselineMontageRate() { RestoreBaselineMontageRate(); }
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
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateEndTask;

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
	bool bRateWindowApplied = false;

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

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);

	void EndFromMontage(bool bWasCancelled);
	void ClearInvulnerabilityEffect();
	void SetDodgeCancelable(bool bShouldCancel);
	void RestoreBaselineMontageRate();
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
};
