#include "Tests/TestJumpGameplayEffects.h"

#include "AbilitySystem/CharacterAttributeSet.h"

UTestJumpCostGE::UTestJumpCostGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetStaminaAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.0f));
	Modifiers.Add(ModInfo);
}

UTestJumpRegenDelayGE::UTestJumpRegenDelayGE()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetStaminaRegenRateMultiplierAttribute();
	ModInfo.ModifierOp = EGameplayModOp::MultiplyAdditive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.0f));
	Modifiers.Add(ModInfo);
}

UTestJumpExhaustionAbility::UTestJumpExhaustionAbility()
{
	CostGameplayEffectClass = UTestJumpCostGE::StaticClass();
	StaminaRegenDelayGameplayEffectClass = UTestJumpRegenDelayGE::StaticClass();
}
