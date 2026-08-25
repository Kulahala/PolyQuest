#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PrimaryAttackAbility.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;

/**
 * Arbitrates one PrimaryAttack hold into either Light or Charged Attack.
 */
UCLASS()
class POLYQUEST_API UPrimaryAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPrimaryAttackAbility();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primary Attack", meta = (ClampMin = "0.0", ToolTip = "主攻击输入裁决阈值（秒）；短按释放判定为轻攻击，长按超过该阈值裁决为蓄力攻击。"))
	float ChargeThresholdSeconds = 0.2f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> ChargeThresholdTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InputReleasedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InputCanceledTask;

	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;
	FGameplayTag LightAttackAbilityTag;
	FGameplayTag ChargedAttackAbilityTag;
	FGameplayTag ChargedReleaseHandoffEventTag;
	bool bInputResolved = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnChargeThresholdReached();

	UFUNCTION()
	void OnInputReleased(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputCanceled(FGameplayEventData Payload);

	void EndFromArbitration(bool bWasCancelled);
	void RequestAbility(FGameplayTag AbilityTag) const;
	void RequestChargedAttackFromRelease(const FGameplayEventData& Payload) const;
	bool IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const;
};
