#include "Tests/TestMobileBowSprintAbility.h"

#include "GameplayTagContainer.h"

UTestMobileBowSprintAbility::UTestMobileBowSprintAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FGameplayTag SprintAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Sprint")), false);
	const FGameplayTag SprintStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);

	if (SprintAbilityTag.IsValid())
	{
		AbilityTags.AddTag(SprintAbilityTag);
	}
	if (SprintStateTag.IsValid())
	{
		ActivationOwnedTags.AddTag(SprintStateTag);
	}
}
