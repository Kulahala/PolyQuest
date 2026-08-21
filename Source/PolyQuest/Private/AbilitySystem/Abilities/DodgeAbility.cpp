#include "AbilitySystem/Abilities/DodgeAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

UDodgeAbility::UDodgeAbility()
{
	bRetriggerInstancedAbility = true;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Dodge")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	LightAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false);
	ChargedAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false);
	SprintAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false);
	MeleeSkillAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Skill.Melee")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	InvulnerabilityBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Dodge.Invulnerability.Begin")), false);
	InvulnerabilityEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Dodge.Invulnerability.End")), false);
	CancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	CancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
}

bool UDodgeAbility::CanActivateAbility(
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
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	if (!AbilitySystemComponent || !MovementComponent || !MovementComponent->IsMovingOnGround())
	{
		return false;
	}

	const bool bIsAttacking = AttackingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(AttackingStateTag);
	const bool bIsDodging = DodgingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DodgingStateTag);
	const bool bCanCancelDodge = DodgeCancelableStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DodgeCancelableStateTag);

	if (bIsAttacking || bIsDodging)
	{
		return bCanCancelDodge;
	}

	return true;
}

void UDodgeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	InvulnerabilityEffectHandle.Invalidate();
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	bDodgeCancelable = false;
	bRateWindowApplied = false;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !DodgeMontage || !CostGameplayEffectClass
		|| !StaminaRegenDelayGameplayEffectClass || !InvulnerabilityGameplayEffectClass || !PrimaryAttackAbilityTag.IsValid()
		|| !LightAttackAbilityTag.IsValid() || !ChargedAttackAbilityTag.IsValid() || !SprintAttackAbilityTag.IsValid() || !MeleeSkillAbilityTag.IsValid()
		|| !AttackingStateTag.IsValid() || !DodgingStateTag.IsValid() || !DodgeCancelableStateTag.IsValid()
		|| !InvulnerabilityBeginEventTag.IsValid() || !InvulnerabilityEndEventTag.IsValid()
		|| !CancelWindowBeginEventTag.IsValid() || !CancelWindowEndEventTag.IsValid()
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': ASC, player, AnimInstance, montage, cost, regeneration delay, invulnerability effect, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DodgeMontage);
	InvulnerabilityBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InvulnerabilityBeginEventTag, nullptr, false, true);
	InvulnerabilityEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InvulnerabilityEndEventTag, nullptr, false, true);
	CancelBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, CancelWindowBeginEventTag, nullptr, false, true);
	CancelEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, CancelWindowEndEventTag, nullptr, false, true);
	RateBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);
	if (!MontageTask || !InvulnerabilityBeginTask || !InvulnerabilityEndTask
		|| !CancelBeginTask || !CancelEndTask || !RateBeginTask || !RateEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Dodge activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = DodgeMontage;

	const bool bCanCancelAttack = AbilitySystemComponent->HasMatchingGameplayTag(DodgeCancelableStateTag);
	FGameplayTagContainer AbilityTagsToCancel;
	AbilityTagsToCancel.AddTag(PrimaryAttackAbilityTag);
	if (bCanCancelAttack)
	{
		AbilityTagsToCancel.AddTag(LightAttackAbilityTag);
		AbilityTagsToCancel.AddTag(ChargedAttackAbilityTag);
		AbilityTagsToCancel.AddTag(SprintAttackAbilityTag);
		if (MeleeSkillAbilityTag.IsValid())
		{
			AbilityTagsToCancel.AddTag(MeleeSkillAbilityTag);
		}
	}
	AbilitySystemComponent->CancelAbilities(&AbilityTagsToCancel, nullptr, this);

	PlayerCharacter->ApplyActionFacing();

	MontageTask->OnCompleted.AddDynamic(this, &UDodgeAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UDodgeAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UDodgeAbility::OnMontageCancelled);

	InvulnerabilityBeginTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnInvulnerabilityBegin);
	InvulnerabilityEndTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnInvulnerabilityEnd);
	CancelBeginTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnCancelWindowBegin);
	CancelEndTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnCancelWindowEnd);
	RateBeginTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnRateWindowBegin);
	RateEndTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnRateWindowEnd);

	InvulnerabilityBeginTask->ReadyForActivation();
	InvulnerabilityEndTask->ReadyForActivation();
	CancelBeginTask->ReadyForActivation();
	CancelEndTask->ReadyForActivation();
	RateBeginTask->ReadyForActivation();
	RateEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// Montage startup can synchronously invoke the bound end delegate and clear all transient state.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(DodgeMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(false);
}

void UDodgeAbility::EndAbility(
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
	ClearInvulnerabilityEffect();
	SetDodgeCancelable(false);
	RestoreBaselineMontageRate();

	if (BoundAnimInstance)
	{
		if (ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, ActiveMontage.Get());
		}
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (InvulnerabilityBeginTask)
	{
		InvulnerabilityBeginTask->EndTask();
		InvulnerabilityBeginTask = nullptr;
	}

	if (InvulnerabilityEndTask)
	{
		InvulnerabilityEndTask->EndTask();
		InvulnerabilityEndTask = nullptr;
	}

	if (CancelBeginTask)
	{
		CancelBeginTask->EndTask();
		CancelBeginTask = nullptr;
	}

	if (CancelEndTask)
	{
		CancelEndTask->EndTask();
		CancelEndTask = nullptr;
	}

	if (RateBeginTask)
	{
		RateBeginTask->EndTask();
		RateBeginTask = nullptr;
	}

	if (RateEndTask)
	{
		RateEndTask->EndTask();
		RateEndTask = nullptr;
	}

	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UDodgeAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void UDodgeAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void UDodgeAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

void UDodgeAbility::OnInvulnerabilityBegin(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload) || InvulnerabilityEffectHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* InvulnerabilityEffect = InvulnerabilityGameplayEffectClass
		? InvulnerabilityGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!AbilitySystemComponent || !InvulnerabilityEffect)
	{
		return;
	}

	InvulnerabilityEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
		InvulnerabilityEffect,
		GetAbilityLevel(),
		AbilitySystemComponent->MakeEffectContext());

	if (!InvulnerabilityEffectHandle.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge invulnerability failed to apply for '%s'."), *GetNameSafe(GetAvatarActorFromActorInfo()));
	}
}

void UDodgeAbility::OnInvulnerabilityEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		ClearInvulnerabilityEffect();
	}
}

void UDodgeAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UDodgeAbility::ClearInvulnerabilityEffect()
{
	if (!InvulnerabilityEffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(InvulnerabilityEffectHandle);
	}

	InvulnerabilityEffectHandle.Invalidate();
}

void UDodgeAbility::OnCancelWindowBegin(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	SetDodgeCancelable(true);
}

void UDodgeAbility::OnCancelWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	SetDodgeCancelable(false);
}

void UDodgeAbility::SetDodgeCancelable(bool bShouldCancel)
{
	if (bDodgeCancelable == bShouldCancel)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return;
	}

	bDodgeCancelable = bShouldCancel;
	if (bDodgeCancelable)
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(DodgeCancelableStateTag);
		}
	}
	else
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(DodgeCancelableStateTag);
		}
	}
}

void UDodgeAbility::OnRateWindowBegin(FGameplayEventData Payload)
{
	if (bRateWindowApplied || !IsGameplayEventFromActiveMontage(Payload) || Payload.EventMagnitude <= 0.0f)
	{
		return;
	}

	if (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveMontage.Get(), Payload.EventMagnitude);
		bRateWindowApplied = true;
	}
}

void UDodgeAbility::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	RestoreBaselineMontageRate();
}

void UDodgeAbility::RestoreBaselineMontageRate()
{
	if (!bRateWindowApplied)
	{
		return;
	}

	if (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveMontage.Get(), 1.0f);
	}

	bRateWindowApplied = false;
}

bool UDodgeAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bEndAbilityRequested || !ActiveMontage || !AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return false;
	}

	const UObject* PayloadObject = Payload.OptionalObject.Get();
	if (!PayloadObject)
	{
		return false;
	}

	if (PayloadObject == ActiveMontage.Get())
	{
		return true;
	}

	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PayloadObject))
	{
		for (const FSlotAnimationTrack& Track : ActiveMontage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (Segment.GetAnimReference() == Sequence)
				{
					return true;
				}
			}
		}
	}

	return false;
}
