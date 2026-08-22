#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemyStanceBreakAbility::UEnemyStanceBreakAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	StanceBreakAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.StanceBreak")), false);
	StanceBreakEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.StanceBreak")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	EnemyHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false);
	EnemySmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);

	AbilityTags.AddTag(StanceBreakAbilityTag);
	ActivationOwnedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(StunnedStateTag);

	FAbilityTriggerData StanceBreakTrigger;
	StanceBreakTrigger.TriggerTag = StanceBreakEventTag;
	StanceBreakTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(StanceBreakTrigger);

	AbilitiesToCancel.AddTag(EnemyMeleeAbilityTag);
	AbilitiesToCancel.AddTag(EnemyHitReactionAbilityTag);
	AbilitiesToCancel.AddTag(EnemySmallHitReactionAbilityTag);
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
	bMovementLockedByStanceBreak = false;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break activation aborted for '%s': ASC, living enemy, AnimInstance, montage, and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, StanceBreakMontage);
	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break activation aborted for '%s': failed to create a montage AbilityTask."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy stance break activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = StanceBreakMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyStanceBreakAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyStanceBreakAbility::OnActiveMontageEnded);
	MontageTask->ReadyForActivation();

	// A zero-length or invalid authored Montage can synchronously reach teardown.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
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
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyStanceBreakAbility::OnActiveMontageEnded);
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

	ActiveMontage = nullptr;

	const bool bCanRestoreEnemy = EnemyCharacter && !EnemyCharacter->IsDead() && !EnemyCharacter->IsActorBeingDestroyed();
	if (bCanRestoreEnemy)
	{
		const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
		const bool bHitReactionStillOwnsMovement = CharacterASC && HitReactingStateTag.IsValid()
			&& CharacterASC->HasMatchingGameplayTag(HitReactingStateTag);
		// A failed stance Montage can leave the existing hit reaction active; let
		// that Ability recover its own movement lock after Stunned is released.
		if (bMovementLockedByStanceBreak && !bHitReactionStillOwnsMovement)
		{
			if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
			}
		}

		// Restore through the authored Instant GE path while the Stunned tag is
		// still owned; StateTree can resume only after Super removes that tag.
		if (!EnemyCharacter->RestorePoiseToMax())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy stance break ended for '%s' but Poise could not be restored; verify the authored recovery GameplayEffect."), *GetNameSafe(EnemyCharacter));
		}
	}
	bMovementLockedByStanceBreak = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemyStanceBreakAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
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

	return CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && EnemyCharacter->IsPoiseBroken()
		&& EnemyCharacter->HasValidPoiseRecoveryConfiguration() && AnimInstance && StanceBreakMontage
		&& StanceBreakAbilityTag.IsValid() && StanceBreakEventTag.IsValid() && StunnedStateTag.IsValid() && HitReactingStateTag.IsValid()
		&& EnemyMeleeAbilityTag.IsValid() && EnemyHitReactionAbilityTag.IsValid() && EnemySmallHitReactionAbilityTag.IsValid();
}

void UEnemyStanceBreakAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
