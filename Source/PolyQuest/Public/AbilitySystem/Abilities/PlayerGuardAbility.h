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
	bool TryGuardMeleeHit(AActor* AttackingActor, float GuardStaminaDamage);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> GuardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> GuardMoveSpeedGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> GuardStaminaRegenMultiplierGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> GuardStaminaCostGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Guard", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StaminaRegenDelayGameplayEffectClass;

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
	FGameplayTagContainer AttackAbilityTags;
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
};
