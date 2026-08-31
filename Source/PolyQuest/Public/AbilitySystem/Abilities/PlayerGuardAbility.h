#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "PlayerGuardAbility.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;
class USoundBase;

/**
 * Held, grounded player Guard. The shared resolver asks this active Ability
 * whether one incoming melee contact was consumed by the authored guard data.
 */
UCLASS()
class POLYQUEST_API UPlayerGuardAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerGuardAbility();

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

	/** True only after the Guard Montage and both Duration effects are confirmed active. */
	bool IsGuardActive() const { return bGuardActive && !bEndAbilityRequested; }

	/** Applies one authored Stamina loss for a resolver-validated front-arc melee contact. */
	bool TryGuardMeleeHit(AActor* AttackingActor, float GuardStaminaDamage, const FHitResult& HitResult);

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestCurrentSpecHandle(const FGameplayAbilitySpecHandle InHandle) { CurrentSpecHandle = InHandle; }
	void SetTestGuardActive(bool bActive)
	{
		bGuardActive = bActive;
		if (bActive)
		{
			bEndAbilityRequested = false;
		}
	}
	void SetTestGuardStaminaCostGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { GuardStaminaCostGameplayEffectClass = InClass; }
	void SetTestGuardSuccessSound(USoundBase* InSound) { GuardSuccessSound = InSound; }
	void SetTestBypassAudioPlayback(const bool bBypass) { bTestBypassAudioPlayback = bBypass; }
	int32 GetTestGuardSuccessSoundDispatchCount() const { return TestGuardSuccessSoundDispatchCount; }
	FVector GetTestLastGuardSuccessSoundLocation() const { return TestLastGuardSuccessSoundLocation; }
	const FGameplayTagContainer& GetTestDefenseCancelableAbilityTags() const { return DefenseCancelableAbilityTags; }
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true", ToolTip = "持盾/武器防御姿态的循环动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> GuardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true", ToolTip = "防御期间施加的移动速度减速 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> GuardMoveSpeedGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true", ToolTip = "防御期间降低体力恢复速度的乘数 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> GuardStaminaRegenMultiplierGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true", ToolTip = "成功格挡攻击时扣除体力的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> GuardStaminaCostGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true", ToolTip = "格挡消耗体力后重置体力自然恢复延迟的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> StaminaRegenDelayGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard|Feedback", meta = (AllowPrivateAccess = "true", ToolTip = "格挡成功时播放的音效。"))
	TObjectPtr<USoundBase> GuardSuccessSound;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InputReleasedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InputCanceledTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FActiveGameplayEffectHandle GuardMoveSpeedEffectHandle;
	FActiveGameplayEffectHandle GuardStaminaRegenMultiplierEffectHandle;
	FGameplayTag GuardAbilityTag;
	FGameplayTag GuardingStateTag;
	FGameplayTag GuardInputTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag DefenseCancelableStateTag;
	FGameplayTag GuardStaminaDamageDataTag;
	FGameplayTag GuardBreakEventTag;
	FGameplayTag DeadStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTagContainer DefenseCancelableAbilityTags;
	bool bGuardActive = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnInputReleased(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputCanceled(FGameplayEventData Payload);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool ApplyGuardEffects();
	void ClearGuardEffects();
	void ApplyStaminaRegenDelay();
	bool IsGuardInputEvent(const FGameplayEventData& Payload) const;
	bool IsAttackerInGuardArc(const AActor* AttackingActor) const;
	void EndFromMontage(bool bWasCancelled);
	void TriggerGuardSuccessFeedback(const FHitResult& HitResult);

#if WITH_DEV_AUTOMATION_TESTS
	int32 TestGuardSuccessSoundDispatchCount = 0;
	FVector TestLastGuardSuccessSoundLocation = FVector::ZeroVector;
	bool bTestBypassAudioPlayback = false;
#endif
};
