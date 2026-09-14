#include "AbilitySystem/Abilities/DodgeAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
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
	CancelableByDodgeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	PlayerLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Launch")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	InvulnerabilityBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Dodge.Invulnerability.Begin")), false);
	InvulnerabilityEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Dodge.Invulnerability.End")), false);
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
	const bool bIsHitReacting = HitReactingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(HitReactingStateTag);
	const bool bCanCancelDodge = DodgeCancelableStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DodgeCancelableStateTag);

	if (bIsAttacking || bIsDodging || bIsHitReacting)
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

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !DodgeMontage || !CostGameplayEffectClass
		|| !StaminaRegenDelayGameplayEffectClass || !InvulnerabilityGameplayEffectClass || !PrimaryAttackAbilityTag.IsValid()
		|| !LightAttackAbilityTag.IsValid() || !ChargedAttackAbilityTag.IsValid() || !SprintAttackAbilityTag.IsValid() || !CancelableByDodgeAbilityTag.IsValid()
		|| !PlayerLaunchReactionAbilityTag.IsValid()
		|| !AttackingStateTag.IsValid() || !DodgingStateTag.IsValid() || !HitReactingStateTag.IsValid() || !DodgeCancelableStateTag.IsValid()
		|| !InvulnerabilityBeginEventTag.IsValid() || !InvulnerabilityEndEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': ASC, player, AnimInstance, montage, cost, regeneration delay, invulnerability effect, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayActionMontage* CreatedMontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this, NAME_None, DodgeMontage, 1.0f, NAME_None,
		1.0f, // AnimRootMotionTranslationScale
		0.0f, // StartTimeSeconds
		false, // bAllowInterruptAfterBlendOut
		EActionMontageCancelPolicy::DodgeOnly); // CancelPolicy
	MontageTask = CreatedMontageTask;
	InvulnerabilityBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InvulnerabilityBeginEventTag, nullptr, false, true);
	InvulnerabilityEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InvulnerabilityEndEventTag, nullptr, false, true);
	if (!MontageTask || !InvulnerabilityBeginTask || !InvulnerabilityEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bCommitSucceeded = CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask) return;
	if (!bCommitSucceeded)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Dodge activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = DodgeMontage;

	const bool bCanCancelAction = AbilitySystemComponent->HasMatchingGameplayTag(DodgeCancelableStateTag);
	FGameplayTagContainer AbilityTagsToCancel;
	AbilityTagsToCancel.AddTag(PrimaryAttackAbilityTag);
	if (bCanCancelAction)
	{
		AbilityTagsToCancel.AddTag(LightAttackAbilityTag);
		AbilityTagsToCancel.AddTag(ChargedAttackAbilityTag);
		AbilityTagsToCancel.AddTag(SprintAttackAbilityTag);
		if (CancelableByDodgeAbilityTag.IsValid())
		{
			AbilityTagsToCancel.AddTag(CancelableByDodgeAbilityTag);
		}
		if (PlayerLaunchReactionAbilityTag.IsValid())
		{
			AbilityTagsToCancel.AddTag(PlayerLaunchReactionAbilityTag);
		}
	}
	AbilitySystemComponent->CancelAbilities(&AbilityTagsToCancel, nullptr, this);
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask) return;

	PlayerCharacter->ApplyDodgeFacing();

	MontageTask->OnCompleted.AddDynamic(this, &UDodgeAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UDodgeAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UDodgeAbility::OnMontageCancelled);
	MontageTask->OnFailed.AddDynamic(this, &UDodgeAbility::OnMontageCancelled);

	InvulnerabilityBeginTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnInvulnerabilityBegin);
	InvulnerabilityEndTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnInvulnerabilityEnd);

	InvulnerabilityBeginTask->ReadyForActivation();
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask) return;
	InvulnerabilityEndTask->ReadyForActivation();
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask) return;
	MontageTask->ReadyForActivation();

	// Montage startup can synchronously invoke the bound end delegate and clear all transient state.
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask)
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

	if (BoundAnimInstance)
	{
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->OnFailed.RemoveAll(this);
		MontageTask->OnCompleted.RemoveAll(this);
		MontageTask->OnInterrupted.RemoveAll(this);
		MontageTask->OnCancelled.RemoveAll(this);
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

#if WITH_DEV_AUTOMATION_TESTS
const FAbilityMontageRateWindowLifecycle& UDodgeAbility::GetTestRateWindowLifecycle() const
{
	static const FAbilityMontageRateWindowLifecycle EmptyLifecycle;
	return MontageTask ? MontageTask->GetRateWindowLifecycle() : EmptyLifecycle;
}

FAbilityMontageRateWindowLifecycle& UDodgeAbility::GetTestRateWindowLifecycle_Mutable()
{
	check(MontageTask);
	return MontageTask->GetRateWindowLifecycle_Mutable();
}

int32 UDodgeAbility::GetTestRateWindowMontageInstanceID() const
{
	return MontageTask ? MontageTask->GetBoundMontageInstanceID() : INDEX_NONE;
}

bool UDodgeAbility::HasTestRateWindowTasks() const
{
	return MontageTask && MontageTask->IsActive() && !MontageTask->IsTerminated();
}

bool UDodgeAbility::GetTestDodgeCancelable() const
{
	return MontageTask && MontageTask->HasContributedDodgeTag();
}
#endif
