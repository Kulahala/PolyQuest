#include "Tests/TestMobileBowMoveSpeedGE.h"

#include "AbilitySystem/CharacterAttributeSet.h"

UTestMobileBowMoveSpeedGE::UTestMobileBowMoveSpeedGE()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetMoveSpeedAttribute();
	ModInfo.ModifierOp = EGameplayModOp::MultiplyAdditive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.6f));
	Modifiers.Add(ModInfo);
}
