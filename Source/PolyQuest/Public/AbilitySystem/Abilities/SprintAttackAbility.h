#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "SprintAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

/**
 * A Root Motion attack that may begin only from a real active Sprint state.
 * It owns one Stamina transaction and one Notify-timed forward sweep.
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack")
	TObjectPtr<UAnimMontage> SprintAttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Trace", meta = (ClampMin = "0.0"))
	float TraceRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Trace")
	float TraceHeightOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint Attack|Trace", meta = (ClampMin = "0.0"))
	float TraceDistance = 150.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FGameplayTag SprintStateTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag MovementInputBlockedTag;
	FGameplayTag JumpInputBlockedTag;
	FGameplayTag StaminaRegenBlockedTag;
	FGameplayTag HitEventTag;
	FGameplayTag DodgeCancelWindowBeginEventTag;
	FGameplayTag DodgeCancelWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	bool bHitEventConsumed = false;
	bool bDodgeCancelable = false;
	bool bRuntimeActionTagsApplied = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowEnd(FGameplayEventData Payload);

	void EndFromMontage(bool bWasCancelled);
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	void PerformHitTrace();
	void SetDodgeCancelable(bool bShouldBeCancelable);
	void SetRuntimeActionTags(bool bShouldApply);
};
