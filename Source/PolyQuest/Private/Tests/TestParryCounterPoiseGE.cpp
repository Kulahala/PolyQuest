#include "Tests/TestParryCounterPoiseGE.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "GameplayTagContainer.h"

UTestParryCounterPoiseGE::UTestParryCounterPoiseGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetPoiseAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Parry")), false);
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);
}
