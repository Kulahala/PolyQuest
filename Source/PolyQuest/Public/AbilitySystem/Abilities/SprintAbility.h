#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "SprintAbility.generated.h"

class UGameplayEffect;
struct FOnAttributeChangeData;

/** Grounded movement ability that owns Sprint speed, periodic Stamina drain, and cleanup. */
UCLASS()
class POLYQUEST_API USprintAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USprintAbility();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint", meta = (ToolTip = "冲刺期间施加的移速加成 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> MoveSpeedGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint", meta = (ToolTip = "冲刺期间周期性消耗体力的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> StaminaDrainGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint", meta = (ToolTip = "冲刺结束或体力耗尽后延迟体力恢复的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> StaminaRegenDelayGameplayEffectClass;

private:
	FGameplayTag SprintStateTag;
	FActiveGameplayEffectHandle MoveSpeedEffectHandle;
	FActiveGameplayEffectHandle StaminaDrainEffectHandle;
	FDelegateHandle StaminaAttributeChangedHandle;
	bool bStaminaDrainCommitted = false;
	bool bEndAbilityRequested = false;

	void BindStaminaAttribute();
	void UnbindStaminaAttribute();
	void OnStaminaAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void ClearActiveEffect(FActiveGameplayEffectHandle& EffectHandle);
	void ApplyStaminaRegenDelay();
	void EndFromSprintState(bool bWasCancelled);
};
