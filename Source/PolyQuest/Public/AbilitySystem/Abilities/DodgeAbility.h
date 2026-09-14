#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "DodgeAbility.generated.h"

class UAbilityTask_PlayActionMontage;
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
	bool GetTestRetriggerInstancedAbility() const { return bRetriggerInstancedAbility; }
	void SetTestActiveMontage(UAnimMontage* Montage) { ActiveMontage = Montage; }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInstance) { BoundAnimInstance = AnimInstance; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	bool Test_IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const { return IsGameplayEventFromActiveMontage(Payload); }
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "翻滚/闪避动作动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "闪避无敌帧窗口期间施加的无敌状态 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> InvulnerabilityGameplayEffectClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayActionMontage> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InvulnerabilityBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InvulnerabilityEndTask;

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
	FActiveGameplayEffectHandle InvulnerabilityEffectHandle;
	bool bEndAbilityRequested = false;

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

	void EndFromMontage(bool bWasCancelled);
	void ClearInvulnerabilityEffect();
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;

#if WITH_DEV_AUTOMATION_TESTS
public:
	UAbilityTask_PlayActionMontage* GetTestMontageTask() const { return MontageTask.Get(); }
	void SetTestMontageTask(UAbilityTask_PlayActionMontage* Task) { MontageTask = Task; }
	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const;
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable();
	int32 GetTestRateWindowMontageInstanceID() const;
	bool HasTestRateWindowTasks() const;
	bool GetTestDodgeCancelable() const;
#endif
};
