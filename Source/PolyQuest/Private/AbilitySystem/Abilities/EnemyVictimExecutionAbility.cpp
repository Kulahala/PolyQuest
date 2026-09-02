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

	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	return true;
}

bool UEnemyVictimExecutionAbility::ValidateExecutionRequest(
	const FGameplayEventData* TriggerEventData,
	AEnemyCharacter* EnemyCharacter) const
{
	if (!TriggerEventData || !EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsActorBeingDestroyed())
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

	const UAbilitySystemComponent* CharacterASC = EnemyCharacter->GetAbilitySystemComponent();
	if (!CharacterASC)
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

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();

	if (!ValidateExecutionRequest(TriggerEventData, EnemyCharacter) || !CharacterASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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

void UEnemyVictimExecutionAbility::OnReleaseReceived(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	if (Payload.OptionalObject.Get() == ActiveExecutionContext.Get())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
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

	if (ActiveExecutionContext)
	{
		ActiveExecutionContext->InvalidateSession();
		ActiveExecutionContext = nullptr;
	}

	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);

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

	bMovementLockedByVictim = false;
	bHandoffFromStanceBreak = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEndAbilityInProgress = false;
}
