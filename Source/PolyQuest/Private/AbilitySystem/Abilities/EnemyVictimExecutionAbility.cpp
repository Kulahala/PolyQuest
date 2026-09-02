#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AI/EnemyAIController.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemyVictimExecutionAbility::UEnemyVictimExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	VictimAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Victim")), false);
	FrontRequestEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	BackstabRequestEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
	ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
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

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();

	const bool bCommitDeath = bDeathPending || (EnemyCharacter && EnemyCharacter->IsDeathPending());
	bool bDeathCommittedSuccessfully = false;

	if (bCommitDeath && EnemyCharacter && CharacterASC)
	{
		const bool bCanFinalize = ActiveExecutionContext && ActiveExecutionContext->BeginFinalization(this);
		if (bCanFinalize)
		{
			bDeathCommittedSuccessfully = EnemyCharacter->CommitExecutionDeath(ActiveExecutionContext.Get());
			if (bDeathCommittedSuccessfully)
			{
				ActiveExecutionContext->CompleteFinalization(this);
			}
			else
			{
				ActiveExecutionContext->AbortFinalization(this);
			}
		}

		// Fallback: If execution commit failed or context was invalid, verify if Health is <= 0.
		// Never leave an enemy with Health <= 0 without Dead or DeathPending.
		const float CurrentHealth = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
		if (!bDeathCommittedSuccessfully && CurrentHealth <= 0.0f)
		{
			EnemyCharacter->SetDeadState();
			bDeathCommittedSuccessfully = EnemyCharacter->IsDead();
		}
	}
	else
	{
		if (bLockedAI)
		{
			if (EnemyCharacter)
			{
				if (AEnemyAIController* AIController = Cast<AEnemyAIController>(EnemyCharacter->GetController()))
				{
					AIController->EndExecutionLock();
				}
			}
			bLockedAI = false;
		}

		const bool bCanRestoreEnemy = EnemyCharacter && !EnemyCharacter->IsDead() && !EnemyCharacter->IsActorBeingDestroyed();
		if (bCanRestoreEnemy)
		{
			if (bMovementLockedByVictim)
			{
				if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
				{
					MovementComponent->SetMovementMode(MOVE_Walking);
				}
			}

			if (bHandoffFromStanceBreak)
			{
				if (!EnemyCharacter->RestorePoiseToMax())
				{
					UE_LOG(LogPolyQuest, Warning, TEXT("Victim execution ended for '%s' but Poise could not be restored."), *GetNameSafe(EnemyCharacter));
				}
			}
		}
	}

	// Only clean up DeathPending tag if Dead is confirmed written, or if non-lethal (not bCommitDeath).
	// If commit failed and enemy is still not dead despite Health <= 0, retain DeathPending tag.
	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);

	const bool bIsDeadConfirmed = EnemyCharacter ? EnemyCharacter->IsDead() : false;
	const bool bSafeToClearPendingTag = bIsDeadConfirmed || !bCommitDeath;

	if (bAddedDeathPendingTag && ActualDeathPendingTag.IsValid() && bSafeToClearPendingTag)
	{
		if (CharacterASC)
		{
			CharacterASC->RemoveLooseGameplayTag(ActualDeathPendingTag);
		}
		bAddedDeathPendingTag = false;
	}

	if (EnemyCharacter)
	{
		EnemyCharacter->ClearExecutionVictimAbility(this);
	}

	bMovementLockedByVictim = false;
	bHandoffFromStanceBreak = false;
	bLockedAI = false;

	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}

	if (ActiveExecutionContext)
	{
		ActiveExecutionContext->InvalidateSession();
		ActiveExecutionContext = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEndAbilityInProgress = false;
}
