#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "JumpAbility.generated.h"

class UGameplayEffect;

/** One-shot GAS Jump that commits Stamina before invoking native Character jumping. */
UCLASS()
class POLYQUEST_API UJumpAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	UJumpAbility();

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

protected:
	/** Applied from takeoff until landing when Jump starts from an active Sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	TSubclassOf<UGameplayEffect> SprintJumpAirSpeedGameplayEffectClass;
};
