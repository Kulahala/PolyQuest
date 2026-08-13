#include "AbilitySystem/Abilities/SprintAbility.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerCharacter.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

USprintAbility::USprintAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Sprint")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Resource.Stamina.RegenBlocked")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	SprintStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
}

bool USprintAbility::CanActivateAbility(
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

	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return AbilitySystemComponent && PlayerCharacter && PlayerCharacter->CanAttemptSprint()
		&& AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) > 0.0f;
}

void USprintAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	bStaminaDrainCommitted = false;
	MoveSpeedEffectHandle.Invalidate();
	StaminaDrainEffectHandle.Invalidate();
	UnbindStaminaAttribute();

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	const UGameplayEffect* MoveSpeedEffect = MoveSpeedGameplayEffectClass
		? MoveSpeedGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	const UGameplayEffect* StaminaDrainEffect = StaminaDrainGameplayEffectClass
		? StaminaDrainGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!AbilitySystemComponent || !PlayerCharacter || !MoveSpeedEffect || !StaminaDrainEffect || !StaminaRegenDelayGameplayEffectClass
		|| !SprintStateTag.IsValid() || !PlayerCharacter->CanAttemptSprint())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint activation aborted for '%s': ASC, player, configured effects, valid Sprint tag, and grounded Sprint input are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MoveSpeedEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
		MoveSpeedEffect,
		GetAbilityLevel(),
		AbilitySystemComponent->MakeEffectContext());
	if (!MoveSpeedEffectHandle.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint activation aborted for '%s': failed to apply its MoveSpeed GameplayEffect."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StaminaDrainEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
		StaminaDrainEffect,
		GetAbilityLevel(),
		AbilitySystemComponent->MakeEffectContext());
	if (!StaminaDrainEffectHandle.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint activation aborted for '%s': failed to apply its Stamina drain GameplayEffect."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bStaminaDrainCommitted = true;
	BindStaminaAttribute();
	if (AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f)
	{
		PlayerCharacter->MarkSprintRequiresReleaseAfterExhaustion();
		EndFromSprintState(true);
	}
}

void USprintAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bEndAbilityRequested)
	{
		return;
	}

	bEndAbilityRequested = true;
	UnbindStaminaAttribute();
	ClearActiveEffect(StaminaDrainEffectHandle);
	ClearActiveEffect(MoveSpeedEffectHandle);

	if (bStaminaDrainCommitted)
	{
		bStaminaDrainCommitted = false;
		ApplyStaminaRegenDelay();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USprintAbility::BindStaminaAttribute()
{
	if (StaminaAttributeChangedHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		StaminaAttributeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute())
			.AddUObject(this, &USprintAbility::OnStaminaAttributeChanged);
	}
}

void USprintAbility::UnbindStaminaAttribute()
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		if (StaminaAttributeChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute())
				.Remove(StaminaAttributeChangedHandle);
		}
	}

	StaminaAttributeChangedHandle.Reset();
}

void USprintAbility::OnStaminaAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (bEndAbilityRequested || ChangeData.NewValue > 0.0f)
	{
		return;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->MarkSprintRequiresReleaseAfterExhaustion();
	}

	EndFromSprintState(true);
}

void USprintAbility::ClearActiveEffect(FActiveGameplayEffectHandle& EffectHandle)
{
	if (!EffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
	}

	EffectHandle.Invalidate();
}

void USprintAbility::ApplyStaminaRegenDelay()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* RegenDelayEffect = StaminaRegenDelayGameplayEffectClass
		? StaminaRegenDelayGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (AbilitySystemComponent && RegenDelayEffect)
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(
			RegenDelayEffect,
			GetAbilityLevel(),
			AbilitySystemComponent->MakeEffectContext());
	}
}

void USprintAbility::EndFromSprintState(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
