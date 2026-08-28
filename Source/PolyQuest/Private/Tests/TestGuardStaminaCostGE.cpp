#include "Tests/TestGuardStaminaCostGE.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "GameplayTagContainer.h"

UTestGuardStaminaCostGE::UTestGuardStaminaCostGE()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UCharacterAttributeSet::GetStaminaAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Stamina.GuardDamage")), false);
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);
}
