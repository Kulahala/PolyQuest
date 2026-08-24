#include "Tests/TestExhaustionMoveSpeedGE.h"

#include "AbilitySystem/CharacterAttributeSet.h"

UTestExhaustionMoveSpeedGE::UTestExhaustionMoveSpeedGE()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetMoveSpeedAttribute();
	ModInfo.ModifierOp = EGameplayModOp::MultiplyAdditive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.7f));
	Modifiers.Add(ModInfo);
}
