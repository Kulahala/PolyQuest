#include "AbilitySystem/Abilities/StaminaActionAbility.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

bool UStaminaActionAbility::CheckCost(
	const FGameplayAbilitySpecHandle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	if (AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) > 0.0f)
	{
		return true;
	}

	if (OptionalRelevantTags)
	{
		const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
		if (ExhaustedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(ExhaustedTag);
		}
	}

	return false;
}

bool UStaminaActionAbility::CommitStaminaCostOnly(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	bCostCommitted = false;

	if (!StaminaRegenDelayGameplayEffectClass)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Stamina action '%s' cannot commit without a Stamina regeneration delay GameplayEffect."), *GetNameSafe(this));
		return false;
	}

	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}

	bCostCommitted = true;
	return true;
}

bool UStaminaActionAbility::CommitAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	bCostCommitted = false;

	if (!StaminaRegenDelayGameplayEffectClass)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Stamina action '%s' cannot commit without a Stamina regeneration delay GameplayEffect."), *GetNameSafe(this));
		return false;
	}

	if (!Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}

	bCostCommitted = true;
	return true;
}

void UStaminaActionAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bCostCommitted)
	{
		bCostCommitted = false;

		UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
		const UGameplayEffect* RegenDelayEffect = StaminaRegenDelayGameplayEffectClass
			? StaminaRegenDelayGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
			: nullptr;
		if (AbilitySystemComponent && RegenDelayEffect)
		{
			AbilitySystemComponent->ApplyGameplayEffectToSelf(
				RegenDelayEffect,
				GetAbilityLevel(),
				AbilitySystemComponent->MakeEffectContext());
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
