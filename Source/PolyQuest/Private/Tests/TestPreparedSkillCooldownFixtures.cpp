// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/TestPreparedSkillCooldownFixtures.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTagsManager.h"

UTestPreparedSkillCooldownGE::UTestPreparedSkillCooldownGE()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(DefaultTestDuration));

	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	if (TargetTagsComp)
	{
		FInheritedTagContainer TagContainer;
		const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Cooldown.Skill.Whirlwind")), false);
		if (CooldownTag.IsValid())
		{
			TagContainer.AddTag(CooldownTag);
		}
		TargetTagsComp->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TargetTagsComp);
	}
}

UTestPreparedSkillCooldownAbility::UTestPreparedSkillCooldownAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	CooldownGameplayEffectClass = UTestPreparedSkillCooldownGE::StaticClass();

	const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Skill.Whirlwind")), false);
	if (SkillTag.IsValid())
	{
		AbilityTags.AddTag(SkillTag);
	}
}

const FGameplayTagContainer* UTestPreparedSkillCooldownAbility::GetCooldownTags() const
{
	if (CachedCooldownTags.IsEmpty())
	{
		const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Cooldown.Skill.Whirlwind")), false);
		if (CooldownTag.IsValid())
		{
			CachedCooldownTags.AddTag(CooldownTag);
		}
	}
	return &CachedCooldownTags;
}

void UTestPreparedSkillCooldownAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	CommitAbility(Handle, ActorInfo, ActivationInfo);

	if (bAutoEndAbility)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UTestPreparedSkillCooldownAbility::SetTestCooldownDuration(float InDuration)
{
	// Helper for testing custom duration boundaries if needed
}
