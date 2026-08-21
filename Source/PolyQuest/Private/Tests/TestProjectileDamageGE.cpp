#include "Tests/TestProjectileDamageGE.h"

#include "AbilitySystem/CharacterAttributeSet.h"

UTestProjectileDamageGE::UTestProjectileDamageGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetHealthAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-25.0f));
	Modifiers.Add(ModInfo);
}
