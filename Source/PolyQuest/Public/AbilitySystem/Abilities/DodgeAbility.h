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
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	TSubclassOf<UGameplayEffect> InvulnerabilityGameplayEffectClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

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
	FGameplayTag MeleeSkillAbilityTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag InvulnerabilityBeginEventTag;
	FGameplayTag InvulnerabilityEndEventTag;
	FActiveGameplayEffectHandle InvulnerabilityEffectHandle;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnInvulnerabilityBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnInvulnerabilityEnd(FGameplayEventData Payload);

	void EndFromMontage(bool bWasCancelled);
	void ClearInvulnerabilityEffect();
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
};
