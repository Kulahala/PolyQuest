#include "AbilitySystem/Abilities/JumpAbility.h"

#include "Character/Player/PlayerCharacter.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

UJumpAbility::UJumpAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));
}

bool UJumpAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return PlayerCharacter && PlayerCharacter->CanJump();
}

void UJumpAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	const bool bWasSprinting = PlayerCharacter && PlayerCharacter->HasActiveSprint();
	const UGameplayEffect* SprintJumpAirSpeedEffect = bWasSprinting && SprintJumpAirSpeedGameplayEffectClass
		? SprintJumpAirSpeedGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!PlayerCharacter || !PlayerCharacter->CanJump() || !CostGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass
		|| (bWasSprinting && !SprintJumpAirSpeedEffect))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Jump activation aborted for '%s': a jumpable player, cost, Stamina regeneration delay, and Sprint Jump air-speed effect when Sprinting are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Stage the independently removable air-speed effect before spending Stamina so a failed commit leaves no gameplay residue.
	if (bWasSprinting && !PlayerCharacter->ApplySprintJumpAirSpeed(SprintJumpAirSpeedGameplayEffectClass))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Jump activation aborted for '%s': failed to prepare Sprint Jump air speed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (bWasSprinting)
		{
			PlayerCharacter->ClearSprintJumpAirSpeed();
		}

		UE_LOG(LogPolyQuest, Verbose, TEXT("Jump activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bWasSprinting)
	{
		PlayerCharacter->CancelSprintAbility();
	}

	PlayerCharacter->Jump();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
