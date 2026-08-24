#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "StaminaActionAbility.generated.h"

class UGameplayEffect;

/**
 * Shared lifecycle for actions that commit an authored Stamina cost and, by default, delay its recovery.
 */
UCLASS(Abstract)
class POLYQUEST_API UStaminaActionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CommitAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/**
	 * Commits only the Stamina Cost for this activation. Reuses the overridden
	 * CheckCost through the engine's CommitAbilityCost and never commits a
	 * Cooldown; on success it writes bCostCommitted so the shared EndAbility
	 * applies the regeneration delay exactly once when the action requests one.
	 */
	bool CommitStaminaCostOnly(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr);

	/** Zero-cost movement-like actions may retain this lifecycle without delaying Stamina recovery. */
	virtual bool ShouldApplyStaminaRegenDelayOnEnd() const { return true; }

	/** Default effect applied after a committed Stamina action ends to delay periodic recovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenDelayGameplayEffectClass;

private:
	bool bCostCommitted = false;
};
