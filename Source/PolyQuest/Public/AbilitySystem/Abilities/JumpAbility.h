#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "JumpAbility.generated.h"

class UGameplayEffect;

/** One-shot GAS Jump that commits its authored zero-cost GE without delaying Stamina recovery. */
UCLASS()
class POLYQUEST_API UJumpAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	UJumpAbility();

	/** Keeps the authored zero-cost GE validation while allowing Jump at zero Stamina during Exhaustion. */
	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

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

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	bool DoesTestApplyStaminaRegenDelayOnEnd() const { return ShouldApplyStaminaRegenDelayOnEnd(); }
#endif

protected:
	virtual bool ShouldApplyStaminaRegenDelayOnEnd() const override { return false; }

	/** Applied from takeoff until landing when Jump starts from an active Sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ToolTip = "从冲刺状态起跳后在滞空期间维持水平冲刺速度的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> SprintJumpAirSpeedGameplayEffectClass;
};
