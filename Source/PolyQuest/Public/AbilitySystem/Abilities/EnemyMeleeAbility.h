#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EnemyMeleeAbility.generated.h"

class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

/**
 * One server-authoritative enemy melee action. It owns only montage timing,
 * trace-window delivery, and teardown; targeting remains on the controller.
 */
UCLASS()
class POLYQUEST_API UEnemyMeleeAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyMeleeAbility();

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
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveDamageGameplayEffectClass;

	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag TraceWindowBeginEventTag;
	FGameplayTag TraceWindowEndEventTag;
	float ActiveCooldownAfterAttack = 0.0f;
	bool bAttackStarted = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnTraceWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowEnd(FGameplayEventData Payload);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	void EndFromMontage(bool bWasCancelled);
	void OpenTraceWindow();
	void CloseTraceWindow();
};
