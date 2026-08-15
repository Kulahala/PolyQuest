#include "AbilitySystem/CharacterAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagContainer.h"

namespace
{
	const FGameplayTag& GetDeadTag()
	{
		static const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		return DeadTag;
	}

	bool HasDeadStateTag(const UAbilitySystemComponent* AbilitySystemComponent)
	{
		const FGameplayTag& DeadTag = GetDeadTag();
		return AbilitySystemComponent && DeadTag.IsValid()
			&& AbilitySystemComponent->HasMatchingGameplayTag(DeadTag);
	}

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
	MoveSpeed = 500.0f;
}

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = HasDeadStateTag(GetOwningAbilitySystemComponent())
			? 0.0f
			: FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxHealth()));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxStamina()));
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UCharacterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = HasDeadStateTag(GetOwningAbilitySystemComponent())
			? 0.0f
			: FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxHealth()));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxStamina()));
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	UAbilitySystemComponent* AbilitySystemComponent = &Data.Target;
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(HasDeadStateTag(AbilitySystemComponent)
			? 0.0f
			: FMath::Clamp(GetHealth(), 0.0f, FMath::Max(0.0f, GetMaxHealth())));
		return;
	}

	if (Data.EvaluatedData.Attribute != GetStaminaAttribute())
	{
		return;
	}

	const FGameplayTag& ExhaustedTag = GetExhaustedTag();
	if (!AbilitySystemComponent || !ExhaustedTag.IsValid())
	{
		return;
	}

	// Stamina may execute multiple effects while clamped at zero. Set an exact
	// loose-tag count so repeated drains cannot leave Exhausted latched after recovery.
	AbilitySystemComponent->SetLooseGameplayTagCount(ExhaustedTag, GetStamina() <= 0.0f ? 1 : 0);
}
