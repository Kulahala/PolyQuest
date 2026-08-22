#include "Tests/TestPoiseRecoveryGE.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "GameplayTagContainer.h"

UTestPoiseRecoveryGE::UTestPoiseRecoveryGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetPoiseAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Recovery")), false);
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);
}

UTestLaunchDamageGE_PoiseFirst::UTestLaunchDamageGE_PoiseFirst()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Index 0: Poise
	FGameplayModifierInfo PoiseMod;
	PoiseMod.Attribute = UCharacterAttributeSet::GetPoiseAttribute();
	PoiseMod.ModifierOp = EGameplayModOp::Additive;
	PoiseMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-100.0f));
	Modifiers.Add(PoiseMod);

	// Index 1: Health
	FGameplayModifierInfo HealthMod;
	HealthMod.Attribute = UCharacterAttributeSet::GetHealthAttribute();
	HealthMod.ModifierOp = EGameplayModOp::Additive;
	HealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-25.0f));
	Modifiers.Add(HealthMod);
}

UTestLaunchDamageGE_HealthFirst::UTestLaunchDamageGE_HealthFirst()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Index 0: Health
	FGameplayModifierInfo HealthMod;
	HealthMod.Attribute = UCharacterAttributeSet::GetHealthAttribute();
	HealthMod.ModifierOp = EGameplayModOp::Additive;
	HealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-25.0f));
	Modifiers.Add(HealthMod);

	// Index 1: Poise
	FGameplayModifierInfo PoiseMod;
	PoiseMod.Attribute = UCharacterAttributeSet::GetPoiseAttribute();
	PoiseMod.ModifierOp = EGameplayModOp::Additive;
	PoiseMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-100.0f));
	Modifiers.Add(PoiseMod);
}

UTestPoiseDamageOnlyGE::UTestPoiseDamageOnlyGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo PoiseMod;
	PoiseMod.Attribute = UCharacterAttributeSet::GetPoiseAttribute();
	PoiseMod.ModifierOp = EGameplayModOp::Additive;
	PoiseMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-100.0f));
	Modifiers.Add(PoiseMod);
}
