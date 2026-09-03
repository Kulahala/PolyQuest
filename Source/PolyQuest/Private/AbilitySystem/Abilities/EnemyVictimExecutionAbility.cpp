#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AI/EnemyAIController.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemyVictimExecutionAbility::UEnemyVictimExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bLaunchNonLethalOnRelease = true;

	VictimAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Victim")), false);
	FrontRequestEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	BackstabRequestEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
	ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	VictimStartEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	VictimLockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	InvulnerableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeathPendingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	BlockMovementTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	BlockJumpTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
	StanceBreakAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.StanceBreak")), false);

	AbilityTags.AddTag(VictimAbilityTag);
	AbilityTags.AddTag(TeardownOnUnpossessTag);

	ActivationOwnedTags.AddTag(VictimLockedStateTag);
	ActivationOwnedTags.AddTag(InvulnerableStateTag);
	ActivationOwnedTags.AddTag(StunnedStateTag);
	ActivationOwnedTags.AddTag(BlockMovementTag);
	ActivationOwnedTags.AddTag(BlockJumpTag);

	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(VictimLockedStateTag);
	ActivationBlockedTags.AddTag(DeathPendingStateTag);

	FAbilityTriggerData FrontTrigger;
	FrontTrigger.TriggerTag = FrontRequestEventTag;
	FrontTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(FrontTrigger);

	FAbilityTriggerData BackstabTrigger;
	BackstabTrigger.TriggerTag = BackstabRequestEventTag;
	BackstabTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(BackstabTrigger);

	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false));
	AbilitiesToCancel.AddTag(StanceBreakAbilityTag);
}

bool UEnemyVictimExecutionAbility::CanActivateAbility(
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

	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	if (!ActualDeathPendingTag.IsValid())
	{
		return false;
	}

	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsDeathPending() || EnemyCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			return false;
		}
		if (ASC->HasMatchingGameplayTag(ActualDeathPendingTag))
		{
			return false;
		}
	}

	return true;
}

bool UEnemyVictimExecutionAbility::ValidateExecutionRequest(
	const FGameplayEventData* TriggerEventData,
	AEnemyCharacter* EnemyCharacter) const
{
	if (!TriggerEventData || !EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsDeathPending() || EnemyCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	const UAbilitySystemComponent* CharacterASC = EnemyCharacter->GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return false;
	}

	if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return false;
	}

	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	if (ActualDeathPendingTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ActualDeathPendingTag))
	{
		return false;
	}

	const AActor* InstigatorActor = TriggerEventData->Instigator.Get();
	if (!InstigatorActor || InstigatorActor->IsActorBeingDestroyed() || InstigatorActor->GetWorld() != EnemyCharacter->GetWorld())
	{
		return false;
	}

	if (TriggerEventData->Target != EnemyCharacter)
	{
		return false;
	}

	const UExecutionLockContext* Context = Cast<UExecutionLockContext>(TriggerEventData->OptionalObject.Get());
	if (!Context || !Context->IsActive() || Context->IsVictimAccepted())
	{
		return false;
	}

	if (Context->GetTargetActor() != EnemyCharacter || Context->GetSourceActor() != InstigatorActor)
	{
		return false;
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const UAbilitySystemComponent* SourceASC = Context->GetSourceASC();
	const UGameplayAbility* SourceAbility = Context->GetSourceAbility();
	if (!SourceASC || (DeadTag.IsValid() && SourceASC->HasMatchingGameplayTag(DeadTag)) || !SourceAbility || !SourceAbility->IsActive() || Context->GetRequestTag() != TriggerEventData->EventTag)
	{
		return false;
	}

	const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	const FGameplayTag BackstabReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);

	if (TriggerEventData->EventTag == FrontReqTag)
	{
		const UCharacterAttributeSet* AttribSet = CharacterASC->GetSet<UCharacterAttributeSet>();
		const bool bPoiseZero = AttribSet && FMath::IsNearlyZero(AttribSet->GetPoise(), KINDA_SMALL_NUMBER);
		if (!bPoiseZero)
		{
			return false;
		}

		bool bHasActiveStanceBreak = false;
		for (const FGameplayAbilitySpec& Spec : CharacterASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UEnemyStanceBreakAbility>() && Spec.IsActive())
			{
				bHasActiveStanceBreak = true;
				break;
			}
		}

		if (!bHasActiveStanceBreak)
		{
			return false;
		}
	}
	else if (TriggerEventData->EventTag == BackstabReqTag)
	{
		for (const FGameplayAbilitySpec& Spec : CharacterASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UEnemyStanceBreakAbility>() && Spec.IsActive())
			{
				return false;
			}
		}

		if (StunnedTag.IsValid() && CharacterASC->GetTagCount(StunnedTag) > 1)
		{
			return false;
		}

		if (VictimLockedTag.IsValid() && CharacterASC->GetTagCount(VictimLockedTag) > 1)
		{
			return false;
		}

		if (InvulnerableTag.IsValid() && CharacterASC->GetTagCount(InvulnerableTag) > 1)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}

void UEnemyVictimExecutionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityInProgress = false;
	bInAuthorizedHitScope = false;

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();

	const bool bPreviousPending = bDeathPending || (EnemyCharacter && EnemyCharacter->IsDeathPending());
	if (!bPreviousPending)
	{
		bDeathPending = false;
		bAddedDeathPendingTag = false;
	}

	if (!ValidateExecutionRequest(TriggerEventData, EnemyCharacter) || !CharacterASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EnemyCharacter->SetExecutionVictimAbility(this);

	ActiveExecutionContext = Cast<UExecutionLockContext>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));
	if (!ActiveExecutionContext)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	bHandoffFromStanceBreak = (TriggerEventData->EventTag == FrontReqTag);

	// 1. Stop physical movement immediately
	if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		bMovementLockedByVictim = true;
	}

	// 2. Lock AI Controller
	if (AEnemyAIController* AIController = Cast<AEnemyAIController>(EnemyCharacter->GetController()))
	{
		AIController->BeginExecutionLock();
		bLockedAI = true;
	}

	// 3. Cancel competing enemy actions
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);

	// 4. Accept victim lock in context synchronously
	const bool bAccepted = ActiveExecutionContext->AcceptVictim(
		this,
		EnemyCharacter,
		CharacterASC,
		TriggerEventData->EventTag,
		ActiveExecutionContext->GetSourceActivationToken());

	if (!bAccepted)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 5. Listen for Release event
	const FGameplayTag ReleaseTag = ReleaseEventTag.IsValid() ? ReleaseEventTag : FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	if (!ReleaseTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ReleaseTag, nullptr, false, false);
	if (!WaitReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitReleaseTask->EventReceived.AddDynamic(this, &UEnemyVictimExecutionAbility::OnReleaseReceived);
	WaitReleaseTask->ReadyForActivation();

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestInvalidateWaitReleaseTaskAfterReady)
	{
		WaitReleaseTask = nullptr;
	}
#endif

	if (!IsActive() || !ActiveExecutionContext || !WaitReleaseTask || !WaitReleaseTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 6. Cache pending victim montage and listen for VictimStart event to drive presentation
	PendingVictimMontage = bHandoffFromStanceBreak ? FrontExecutionVictimMontage : BackstabExecutionVictimMontage;
	bVictimPresentationStarted = false;

	const FGameplayTag VictimStartTag = VictimStartEventTag.IsValid()
		? VictimStartEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	if (!VictimStartTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitVictimStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, VictimStartTag, nullptr, false, false);
	if (!WaitVictimStartTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitVictimStartTask->EventReceived.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimStartReceived);
	WaitVictimStartTask->ReadyForActivation();

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestInvalidateWaitVictimStartTaskAfterReady)
	{
		WaitVictimStartTask = nullptr;
	}
#endif

	if (!IsActive() || !ActiveExecutionContext || !WaitVictimStartTask || !WaitVictimStartTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
}

bool UEnemyVictimExecutionAbility::BeginAuthorizedHitScope()
{
	if (!IsActive() || bEndAbilityInProgress || bDeathPending)
	{
		return false;
	}

	bInAuthorizedHitScope = true;
	return true;
}

void UEnemyVictimExecutionAbility::EndAuthorizedHitScope()
{
	bInAuthorizedHitScope = false;
}

void UEnemyVictimExecutionAbility::NotifyLethalDamageReceived()
{
	bDeathPending = true;
	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);

	if (!bAddedDeathPendingTag && ActualDeathPendingTag.IsValid())
	{
		if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
		{
			CharacterASC->AddLooseGameplayTag(ActualDeathPendingTag);
			bAddedDeathPendingTag = true;
		}
	}
}

void UEnemyVictimExecutionAbility::OnVictimStartReceived(FGameplayEventData Payload)
{
	if (!IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	if (bVictimPresentationStarted)
	{
		return;
	}

	const FGameplayTag ExpectedVictimStartTag = VictimStartEventTag.IsValid()
		? VictimStartEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	if (!ExpectedVictimStartTag.IsValid() || Payload.EventTag != ExpectedVictimStartTag)
	{
		return;
	}

	UExecutionLockContext* Context = ActiveExecutionContext.Get();
	if (!Context || !Context->IsActive())
	{
		return;
	}

	if (Payload.OptionalObject.Get() != Context)
	{
		return;
	}

	if (Context->IsReleaseSent() || Context->IsVictimReleased())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* ContextSourceActor = Context->GetSourceActor();
	AActor* PayloadInstigator = const_cast<AActor*>(Payload.Instigator.Get());
	AActor* PayloadTarget = const_cast<AActor*>(Payload.Target.Get());

	if (!IsValid(AvatarActor) || !IsValid(ContextSourceActor) || !IsValid(PayloadInstigator) || !IsValid(PayloadTarget))
	{
		return;
	}

	if (Context->GetTargetActor() != AvatarActor || Context->GetVictimAbility() != this)
	{
		return;
	}

	if (PayloadInstigator != ContextSourceActor || PayloadTarget != AvatarActor)
	{
		return;
	}

	const UObject* AnimObj = Payload.OptionalObject2.Get();
	if (!AnimObj || (!AnimObj->IsA<UAnimMontage>() && !AnimObj->IsA<UAnimSequenceBase>()))
	{
		return;
	}

	bVictimPresentationStarted = true;

	UAnimMontage* MontageToPlay = PendingVictimMontage.Get();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(AvatarActor);
	if (MontageToPlay && EnemyCharacter && !EnemyCharacter->IsActorBeingDestroyed())
	{
		if (USkeletalMeshComponent* Mesh = EnemyCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				ActiveVictimMontage = MontageToPlay;
				VictimMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
					this,
					NAME_None,
					MontageToPlay,
					1.0f,
					NAME_None,
					false /* bStopWhenAbilityEnds = false */);
				if (VictimMontageTask)
				{
					VictimMontageTask->OnCompleted.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCompleted);
					VictimMontageTask->OnBlendOut.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageBlendOut);
					VictimMontageTask->OnInterrupted.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageInterrupted);
					VictimMontageTask->OnCancelled.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCancelled);
					VictimMontageTask->ReadyForActivation();
				}
			}
		}
	}
}

void UEnemyVictimExecutionAbility::OnVictimMontageCompleted()
{
	StopVictimMontagePresentation(true);
}

void UEnemyVictimExecutionAbility::OnVictimMontageBlendOut()
{
	StopVictimMontagePresentation(true);
}

void UEnemyVictimExecutionAbility::OnVictimMontageInterrupted()
{
	StopVictimMontagePresentation(false);
}

void UEnemyVictimExecutionAbility::OnVictimMontageCancelled()
{
	StopVictimMontagePresentation(false);
}

void UEnemyVictimExecutionAbility::StopVictimMontagePresentation(bool bIsNaturalCompletion)
{
	if (VictimMontageTask)
	{
		VictimMontageTask->OnCompleted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCompleted);
		VictimMontageTask->OnBlendOut.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageBlendOut);
		VictimMontageTask->OnInterrupted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageInterrupted);
		VictimMontageTask->OnCancelled.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCancelled);
		VictimMontageTask->EndTask();
		VictimMontageTask = nullptr;
	}

	if (!bIsNaturalCompletion)
	{
		if (AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (!EnemyCharacter->IsActorBeingDestroyed())
			{
				if (USkeletalMeshComponent* Mesh = EnemyCharacter->GetMesh())
				{
					if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
					{
						if (ActiveVictimMontage && AnimInstance->Montage_IsActive(ActiveVictimMontage) && !AnimInstance->Montage_GetIsStopped(ActiveVictimMontage))
						{
							AnimInstance->Montage_Stop(0.2f, ActiveVictimMontage);
						}
					}
				}
			}
		}
	}

	ActiveVictimMontage = nullptr;
}

void UEnemyVictimExecutionAbility::OnReleaseReceived(FGameplayEventData Payload)
{
	if (!IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	const FGameplayTag ExpectedReleaseTag = ReleaseEventTag.IsValid()
		? ReleaseEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	if (!ExpectedReleaseTag.IsValid() || Payload.EventTag != ExpectedReleaseTag)
	{
		return;
	}

	UExecutionLockContext* Context = ActiveExecutionContext.Get();
	if (!Context || !Context->IsActive())
	{
		return;
	}

	if (Payload.OptionalObject.Get() != Context)
	{
		return;
	}

	if (!Context->IsReleaseSent())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* ContextSourceActor = Context->GetSourceActor();
	AActor* PayloadInstigator = const_cast<AActor*>(Payload.Instigator.Get());
	AActor* PayloadTarget = const_cast<AActor*>(Payload.Target.Get());

	if (!IsValid(AvatarActor) || !IsValid(ContextSourceActor) || !IsValid(PayloadInstigator) || !IsValid(PayloadTarget))
	{
		return;
	}

	if (Context->GetTargetActor() != AvatarActor || Context->GetVictimAbility() != this)
	{
		return;
	}

	if (PayloadInstigator != ContextSourceActor || PayloadTarget != AvatarActor)
	{
		return;
	}

	Context->MarkVictimReleased(this);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, Context->WasReleaseCancelled());
}

void UEnemyVictimExecutionAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bEndAbilityInProgress)
	{
		return;
	}

	bEndAbilityInProgress = true;
	bInAuthorizedHitScope = false;

	AEnemyCharacter* CachedEnemy = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CachedASC = GetAbilitySystemComponentFromActorInfo();
	UExecutionLockContext* CachedContext = ActiveExecutionContext.Get();
	AActor* CachedSourceActor = CachedContext ? CachedContext->GetSourceActor() : nullptr;

	StopVictimMontagePresentation(false);

	const bool bCommitDeath = bDeathPending || (CachedEnemy && CachedEnemy->IsDeathPending());
	bool bDeathCommittedSuccessfully = false;

	const bool bIsConfirmedNonLethal = !bCommitDeath && !bWasCancelled && CachedContext &&
		(CachedContext->GetHitState() == EExecutionSessionHitState::NonLethal) &&
		(CachedContext->IsVictimReleased() || CachedContext->IsReleaseSent());

	const bool bHitResolved = CachedContext && (
		CachedContext->GetHitState() == EExecutionSessionHitState::NonLethal ||
		CachedContext->GetHitState() == EExecutionSessionHitState::DeathPending);
	const bool bConfirmedRelease = !bWasCancelled && CachedContext &&
		(CachedContext->IsVictimReleased() || CachedContext->IsReleaseSent());
	if (PendingVictimMontage.Get() != nullptr && bConfirmedRelease && bHitResolved && !bVictimPresentationStarted)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("EnemyVictimExecutionAbility for '%s' configured victim montage '%s' but received no VictimStart event before Release."),
			*GetNameSafe(CachedEnemy), *GetNameSafe(PendingVictimMontage.Get()));
	}

	if (bCommitDeath && CachedEnemy && CachedASC)
	{
		const bool bCanFinalize = CachedContext && CachedContext->BeginOutcomeFinalization(this);
		if (bCanFinalize)
		{
			bDeathCommittedSuccessfully = CachedEnemy->CommitExecutionDeath(CachedContext);
			if (bDeathCommittedSuccessfully)
			{
				CachedContext->CompleteOutcomeFinalization(this);
			}
			else
			{
				CachedContext->AbortOutcomeFinalization(this);
			}
		}

		// Fallback: If execution commit failed or context was invalid, verify if Health is <= 0.
		// Never leave an enemy with Health <= 0 without Dead or DeathPending.
		const float CurrentHealth = CachedASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
		if (!bDeathCommittedSuccessfully && CurrentHealth <= 0.0f)
		{
			CachedEnemy->SetDeadState();
			bDeathCommittedSuccessfully = CachedEnemy->IsDead();
		}
	}
	else
	{
		if (bLockedAI)
		{
			if (CachedEnemy)
			{
				if (AEnemyAIController* AIController = Cast<AEnemyAIController>(CachedEnemy->GetController()))
				{
					AIController->EndExecutionLock();
				}
			}
			bLockedAI = false;
		}

		const bool bCanRestoreEnemy = CachedEnemy && !CachedEnemy->IsDead() && !CachedEnemy->IsActorBeingDestroyed();
		if (bCanRestoreEnemy)
		{
			if (bMovementLockedByVictim)
			{
				if (UCharacterMovementComponent* MovementComponent = CachedEnemy->GetCharacterMovement())
				{
					MovementComponent->SetMovementMode(MOVE_Walking);
				}
			}

			if (bHandoffFromStanceBreak)
			{
				if (!CachedEnemy->RestorePoiseToMax())
				{
					UE_LOG(LogPolyQuest, Warning, TEXT("Victim execution ended for '%s' but Poise could not be restored."), *GetNameSafe(CachedEnemy));
				}
			}
		}
	}

	// Only clean up DeathPending tag if Dead is confirmed written, or if non-lethal (not bCommitDeath).
	// If commit failed and enemy is still not dead despite Health <= 0, retain DeathPending tag.
	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);

	const bool bIsDeadConfirmed = CachedEnemy ? CachedEnemy->IsDead() : false;
	const bool bSafeToClearPendingTag = bIsDeadConfirmed || !bCommitDeath;

	if (bAddedDeathPendingTag && ActualDeathPendingTag.IsValid() && bSafeToClearPendingTag)
	{
		if (CachedASC)
		{
			CachedASC->RemoveLooseGameplayTag(ActualDeathPendingTag);
		}
		bAddedDeathPendingTag = false;
	}

	if (CachedEnemy)
	{
		CachedEnemy->ClearExecutionVictimAbility(this);
	}

	bMovementLockedByVictim = false;
	bHandoffFromStanceBreak = false;
	bLockedAI = false;

	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}

	if (WaitVictimStartTask)
	{
		WaitVictimStartTask->EndTask();
		WaitVictimStartTask = nullptr;
	}

	PendingVictimMontage = nullptr;
	bVictimPresentationStarted = false;

	if (CachedContext && bWasCancelled)
	{
		CachedContext->InvalidateSession();
	}
	ActiveExecutionContext = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (bIsConfirmedNonLethal && bLaunchNonLethalOnRelease && CachedASC && CachedEnemy && !CachedEnemy->IsDead() && !CachedEnemy->IsActorBeingDestroyed())
	{
		const FGameplayTag LaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		if (LaunchReactionEventTag.IsValid())
		{
			FGameplayEventData LaunchPayload;
			LaunchPayload.EventTag = LaunchReactionEventTag;
			LaunchPayload.Instigator = CachedSourceActor;
			LaunchPayload.Target = CachedEnemy;
			CachedASC->HandleGameplayEvent(LaunchReactionEventTag, &LaunchPayload);
		}
	}

	bEndAbilityInProgress = false;
}
