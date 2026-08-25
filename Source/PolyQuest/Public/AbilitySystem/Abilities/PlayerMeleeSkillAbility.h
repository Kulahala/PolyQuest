#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PlayerMeleeSkillAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;

/**
 * A reusable one-shot prepared-slot melee skill: one Montage, exactly one
 * Stamina-plus-Cooldown CommitAbility executed only after the montage is
 * confirmed active, and Notify-timed trace/dodge-cancel/rate windows. Skill
 * identity tags are authored on the Gameplay Ability's Ability Tags and its
 * Cooldown GE's Granted Tags; this class hardcodes no skill-identity tag and
 * references only shared state/event tags.
 */
UCLASS()
class POLYQUEST_API UPlayerMeleeSkillAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	UPlayerMeleeSkillAbility();

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
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Skill")
	TObjectPtr<UAnimMontage> SkillMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Skill")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

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

	FGameplayTag AttackingStateTag;
	FGameplayTag MovementInputBlockedTag;
	FGameplayTag JumpInputBlockedTag;
	FGameplayTag StaminaRegenBlockedTag;
	FGameplayTag TraceWindowBeginEventTag;
	FGameplayTag TraceWindowEndEventTag;
	FGameplayTag DodgeCancelWindowBeginEventTag;
	FGameplayTag DodgeCancelWindowEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag DefenseCancelableStateTag;
	FGameplayTag MeleeSkillAbilityTag;
	bool bDodgeCancelable = false;
	bool bRuntimeActionTagsApplied = false;
	bool bRateWindowApplied = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnTraceWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);

	void EndFromMontage(bool bWasCancelled);
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	void EnsureMeleeSkillCategoryTag();
	void OpenTraceWindow(const TArray<FName>& InTraceSourceNames);
	void CloseTraceWindow();
	void SetDodgeCancelable(bool bShouldBeCancelable);
	void SetRuntimeActionTags(bool bShouldApply);
	void RestoreBaselineMontageRate();

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;
};
