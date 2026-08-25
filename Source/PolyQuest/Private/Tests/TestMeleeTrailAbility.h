#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TestMeleeTrailAbility.generated.h"

class UAbilityTask_MeleeTraceWindow;
class UGameplayEffect;

/**
 * Test-only Gameplay Ability that drives a real UAbilityTask_MeleeTraceWindow
 * for native automation testing.
 */
UCLASS()
class UTestMeleeTrailAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTestMeleeTrailAbility();

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

	void EndTestAbility();

	UAbilityTask_MeleeTraceWindow* GetActiveTraceWindowTask() const { return TraceWindowTask; }

	void HandleTestTraceWindowBegin(const class UAnimNotifyState_AttackTraceWindow* NotifyState);
	void HandleTestTraceWindowEnd(const class UAnimNotifyState_AttackTraceWindow* NotifyState);
	const class UAnimNotifyState_AttackTraceWindow* GetActiveTraceNotifyState() const { return ActiveTraceNotifyState.Get(); }

	UPROPERTY()
	TSubclassOf<UGameplayEffect> TestDamageEffectClass;

	UPROPERTY()
	TArray<FName> TestTraceSourceNames;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_MeleeTraceWindow> TraceWindowTask;

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;
};
