#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/Player/PlayerCharacter.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

UPrimaryAttackAbility::UPrimaryAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	LightAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false);
	ChargedAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false);
	ChargedReleaseHandoffEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.ReleaseHandoff")), false);
}

bool UPrimaryAttackAbility::CanActivateAbility(
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
	return PlayerCharacter && PrimaryAttackInputTag.IsValid() && PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag);
}

void UPrimaryAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bInputResolved = false;
	bEndAbilityRequested = false;

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || ChargeThresholdSeconds < 0.0f || !PrimaryAttackInputTag.IsValid() || !InputReleasedEventTag.IsValid()
		|| !InputCanceledEventTag.IsValid() || !LightAttackAbilityTag.IsValid() || !ChargedAttackAbilityTag.IsValid() || !ChargedReleaseHandoffEventTag.IsValid()
		|| !PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Primary attack arbitration aborted for '%s': a held primary input, non-negative threshold, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ChargeThresholdTask = UAbilityTask_WaitDelay::WaitDelay(this, ChargeThresholdSeconds);
	InputReleasedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputReleasedEventTag, nullptr, false, true);
	InputCanceledTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputCanceledEventTag, nullptr, false, true);
	if (!ChargeThresholdTask || !InputReleasedTask || !InputCanceledTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Primary attack arbitration aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ChargeThresholdTask->OnFinish.AddDynamic(this, &UPrimaryAttackAbility::OnChargeThresholdReached);
	InputReleasedTask->EventReceived.AddDynamic(this, &UPrimaryAttackAbility::OnInputReleased);
	InputCanceledTask->EventReceived.AddDynamic(this, &UPrimaryAttackAbility::OnInputCanceled);

	InputReleasedTask->ReadyForActivation();
	InputCanceledTask->ReadyForActivation();
	ChargeThresholdTask->ReadyForActivation();
}

void UPrimaryAttackAbility::EndAbility(
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

	if (ChargeThresholdTask)
	{
		ChargeThresholdTask->EndTask();
		ChargeThresholdTask = nullptr;
	}

	if (InputReleasedTask)
	{
		InputReleasedTask->EndTask();
		InputReleasedTask = nullptr;
	}

	if (InputCanceledTask)
	{
		InputCanceledTask->EndTask();
		InputCanceledTask = nullptr;
	}

	bInputResolved = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPrimaryAttackAbility::OnChargeThresholdReached()
{
	if (bEndAbilityRequested || bInputResolved)
	{
		return;
	}

	const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || !PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag))
	{
		bInputResolved = true;
		EndFromArbitration(true);
		return;
	}

	bInputResolved = true;
	RequestAbility(ChargedAttackAbilityTag);
	EndFromArbitration(false);
}

void UPrimaryAttackAbility::OnInputReleased(FGameplayEventData Payload)
{
	if (!IsPrimaryAttackInputEvent(Payload) || bInputResolved)
	{
		return;
	}

	bInputResolved = true;
	if (Payload.EventMagnitude < ChargeThresholdSeconds)
	{
		RequestAbility(LightAttackAbilityTag);
	}
	else
	{
		RequestChargedAttackFromRelease(Payload);
	}

	EndFromArbitration(false);
}

void UPrimaryAttackAbility::OnInputCanceled(FGameplayEventData Payload)
{
	if (!IsPrimaryAttackInputEvent(Payload) || bInputResolved)
	{
		return;
	}

	bInputResolved = true;
	EndFromArbitration(true);
}

void UPrimaryAttackAbility::EndFromArbitration(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UPrimaryAttackAbility::RequestAbility(FGameplayTag AbilityTag) const
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer RequestedAbilityTags;
	RequestedAbilityTags.AddTag(AbilityTag);
	const bool bActivated = AbilitySystemComponent->TryActivateAbilitiesByTag(RequestedAbilityTags);
	UE_LOG(LogPolyQuest, Verbose, TEXT("PrimaryAttack: owner='%s', requested='%s', activated=%s."), *GetNameSafe(GetAvatarActorFromActorInfo()), *AbilityTag.ToString(), bActivated ? TEXT("true") : TEXT("false"));
}

void UPrimaryAttackAbility::RequestChargedAttackFromRelease(const FGameplayEventData& Payload) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !ChargedReleaseHandoffEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("PrimaryAttack: cannot hand off a released primary input to Charged Attack without an avatar and valid handoff tag."));
		return;
	}

	FGameplayEventData HandoffPayload = Payload;
	HandoffPayload.EventTag = ChargedReleaseHandoffEventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AvatarActor, ChargedReleaseHandoffEventTag, HandoffPayload);
	UE_LOG(LogPolyQuest, Verbose, TEXT("PrimaryAttack: owner='%s', handed off released primary input to Charged Attack at held=%.3f."), *GetNameSafe(AvatarActor), Payload.EventMagnitude);
}

bool UPrimaryAttackAbility::IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.InstigatorTags.HasTagExact(PrimaryAttackInputTag);
}
