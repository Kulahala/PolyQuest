#include "AbilitySystem/CharacterAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagContainer.h"

namespace
{
	const FGameplayTag& GetExhaustedTag()
	{
		static const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
		return ExhaustedTag;
	}
}

UCharacterAttributeSet::UCharacterAttributeSet()
{
	Health = 100.0f;
	MaxHealth = 100.0f;
	Stamina = 100.0f;
	MaxStamina = 100.0f;
}

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxStamina()));
	}
}

void UCharacterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxStamina()));
	}
}

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute != GetStaminaAttribute())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = &Data.Target;
	const FGameplayTag& ExhaustedTag = GetExhaustedTag();
	if (!AbilitySystemComponent || !ExhaustedTag.IsValid())
	{
		return;
	}

	if (GetStamina() <= 0.0f)
	{
		AbilitySystemComponent->AddLooseGameplayTag(ExhaustedTag);
	}
	else
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(ExhaustedTag);
	}
}
