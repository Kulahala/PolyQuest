#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

void UEnemyStanceBreakExecutionContext::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (UEnemyStanceBreakAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageEnded(Montage, bInterrupted, Token);
	}
}

UEnemyStanceBreakAbility::UEnemyStanceBreakAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	StanceBreakAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.StanceBreak")), false);
	StanceBreakEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.StanceBreak")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	VictimLockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	EnemyHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false);
	EnemySmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);
	EnemyLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false);
	TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
	FacingBlockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Block.Facing")), false);

	AbilityTags.AddTag(StanceBreakAbilityTag);
	if (TeardownOnUnpossessTag.IsValid())
	{
		AbilityTags.AddTag(TeardownOnUnpossessTag);
	}

	ActivationOwnedTags.AddTag(StunnedStateTag);
	if (FacingBlockedStateTag.IsValid())
	{
		ActivationOwnedTags.AddTag(FacingBlockedStateTag);
	}
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(StunnedStateTag);
	if (VictimLockedStateTag.IsValid())
	{
		ActivationBlockedTags.AddTag(VictimLockedStateTag);
	}

	FAbilityTriggerData StanceBreakTrigger;
	StanceBreakTrigger.TriggerTag = StanceBreakEventTag;
	StanceBreakTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(StanceBreakTrigger);

	AbilitiesToCancel.AddTag(EnemyMeleeAbilityTag);
	AbilitiesToCancel.AddTag(EnemyHitReactionAbilityTag);
	AbilitiesToCancel.AddTag(EnemySmallHitReactionAbilityTag);
	AbilitiesToCancel.AddTag(EnemyLaunchReactionAbilityTag);
}

bool UEnemyStanceBreakAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UEnemyStanceBreakAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	++CurrentActivationToken;
	InvalidateCallbackContext();
	bMovementLockedByStanceBreak = false;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	ActiveMontageInstanceID = INDEX_NONE;

#if WITH_DEV_AUTOMATION_TESTS
	if (const UEnemyStanceBreakAbility* CDO = Cast<UEnemyStanceBreakAbility>(GetClass()->GetDefaultObject()))
	{
		if (CDO->bTestBypassMontageActiveCheck)
		{
			SetTestBypassMontageActiveCheck(true);
		}
		if (CDO->StanceBreakMontage && !StanceBreakMontage)
		{
			StanceBreakMontage = CDO->StanceBreakMontage;
		}
		if (CDO->BoundAnimInstance && !BoundAnimInstance)
		{
			BoundAnimInstance = CDO->BoundAnimInstance;
		}
	}
#endif

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!AnimInstance && BoundAnimInstance)
	{
		AnimInstance = BoundAnimInstance.Get();
	}
#endif
	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break activation aborted for '%s': ASC, living enemy, AnimInstance, montage, and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveContext = NewObject<UEnemyStanceBreakExecutionContext>(this);
	if (!ActiveContext)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break activation aborted for '%s': failed to create activation context."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActiveContext->OwningAbility = this;
	ActiveContext->Token = CurrentActivationToken;
	MontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this, NAME_None, StanceBreakMontage, 1.0f, NAME_None,
		1.0f, // AnimRootMotionTranslationScale
		0.0f, // StartTimeSeconds
		true, // bAllowInterruptAfterBlendOut; business teardown waits for global OnMontageEnded
		EActionMontageCancelPolicy::None); // CancelPolicy
	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break activation aborted for '%s': failed to create required AbilityTasks."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UAbilityTask_PlayActionMontage* CreatedMontageTask = MontageTask.Get();
#if WITH_DEV_AUTOMATION_TESTS
	MontageTask->SetTestBypassMontageActiveCheck(bTestBypassMontageActiveCheck);
#endif

	const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (bEndAbilityRequested || !IsActive() || MontageTask.Get() != CreatedMontageTask)
	{
		return;
	}
	if (!bCommitted)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy stance break activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = StanceBreakMontage;
	BoundAnimInstance->OnMontageEnded.AddUniqueDynamic(ActiveContext, &UEnemyStanceBreakExecutionContext::OnMontageEnded);

	MontageTask->ReadyForActivation();
	if (bEndAbilityRequested || !IsActive() || MontageTask.Get() != CreatedMontageTask)
	{
		return;
	}
	if (!IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished() || !CreatedMontageTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FAnimMontageInstance* StartedInstance = BoundAnimInstance->GetActiveInstanceForMontage(ActiveMontage.Get());
	ActiveMontageInstanceID = StartedInstance ? StartedInstance->GetInstanceID() : INDEX_NONE;

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageActive = bTestBypassMontageActiveCheck || (BoundAnimInstance && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));
#else
	const bool bMontageActive = BoundAnimInstance && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get());
#endif

	if (!BoundAnimInstance || !ActiveMontage || !bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(StanceBreakMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		bMovementLockedByStanceBreak = true;
	}

	// Stunned is already owned by this ability. Cancellation is delayed until
	// the Montage is visibly active so an invalid asset cannot interrupt combat.
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);
}

void UEnemyStanceBreakAbility::EndAbility(
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
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	InvalidateCallbackContext();
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	ActiveMontageInstanceID = INDEX_NONE;

	const bool bCanRestoreEnemy = EnemyCharacter && !EnemyCharacter->IsDead() && !EnemyCharacter->IsActorBeingDestroyed();
	if (bCanRestoreEnemy)
	{
		const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
		const bool bVictimLocked = CharacterASC && VictimLockedStateTag.IsValid()
			&& CharacterASC->HasMatchingGameplayTag(VictimLockedStateTag);
		const bool bHitReactionStillOwnsMovement = CharacterASC && HitReactingStateTag.IsValid()
			&& CharacterASC->HasMatchingGameplayTag(HitReactingStateTag);
		// A failed stance Montage can leave the existing hit reaction active; let
		// that Ability recover its own movement lock after Stunned is released.
		// If victim lock is active, victim ability takes ownership of the movement lock.
		if (bMovementLockedByStanceBreak && !bHitReactionStillOwnsMovement && !bVictimLocked)
		{
			if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
			}
		}

		// Restore through the authored Instant GE path while the Stunned tag is
		// still owned; StateTree can resume only after Super removes that tag.
		// If victim lock is active, victim ability takes ownership of restoring Poise on release.
		if (!bVictimLocked)
		{
			if (!EnemyCharacter->RestorePoiseToMax())
			{
				UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break ended for '%s' but Poise could not be restored; verify the authored recovery GameplayEffect."), *GetNameSafe(EnemyCharacter));
			}
		}
	}
	bMovementLockedByStanceBreak = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemyStanceBreakAbility::InvalidateCallbackContext()
{
	if (BoundAnimInstance && ActiveContext)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(ActiveContext, &UEnemyStanceBreakExecutionContext::OnMontageEnded);
	}
	if (ActiveContext)
	{
		ActiveContext->OwningAbility.Reset();
	}
	ActiveContext = nullptr;
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
}

void UEnemyStanceBreakAbility::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint32 InToken)
{
	if (InToken != CurrentActivationToken || bEndAbilityRequested || !IsActive() || Montage != ActiveMontage.Get() || ActiveMontageInstanceID == INDEX_NONE)
	{
		return;
	}

	const FAnimMontageInstance* CurrentInstance = BoundAnimInstance
		? BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID) : nullptr;
	if (CurrentInstance && CurrentInstance->IsValid())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

bool UEnemyStanceBreakAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!AnimInstance && BoundAnimInstance)
	{
		AnimInstance = BoundAnimInstance.Get();
	}
	if (!AnimInstance)
	{
		if (const UEnemyStanceBreakAbility* CDO = Cast<UEnemyStanceBreakAbility>(GetClass()->GetDefaultObject()))
		{
			if (CDO->BoundAnimInstance)
			{
				AnimInstance = CDO->BoundAnimInstance.Get();
			}
		}
	}
	const UAnimMontage* EffectiveMontage = StanceBreakMontage;
	if (!EffectiveMontage)
	{
		if (const UEnemyStanceBreakAbility* CDO = Cast<UEnemyStanceBreakAbility>(GetClass()->GetDefaultObject()))
		{
			EffectiveMontage = CDO->StanceBreakMontage;
		}
	}
#else
	const UAnimMontage* EffectiveMontage = StanceBreakMontage;
#endif

	return CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && EnemyCharacter->IsPoiseBroken()
		&& EnemyCharacter->HasValidPoiseRecoveryConfiguration() && AnimInstance && EffectiveMontage
		&& StanceBreakAbilityTag.IsValid() && StanceBreakEventTag.IsValid() && StunnedStateTag.IsValid() && HitReactingStateTag.IsValid()
		&& EnemyMeleeAbilityTag.IsValid() && EnemyHitReactionAbilityTag.IsValid() && EnemySmallHitReactionAbilityTag.IsValid()
		&& EnemyLaunchReactionAbilityTag.IsValid()
		&& TeardownOnUnpossessTag.IsValid() && FacingBlockedStateTag.IsValid() && AbilitiesToCancel.Num() == 4;
}

void UEnemyStanceBreakAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
