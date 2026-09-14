#include "AbilitySystem/Abilities/PlayerGuardBreakAbility.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UPlayerGuardBreakAbility::UPlayerGuardBreakAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	GuardBreakAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.GuardBreak")), false);
	GuardBreakEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.GuardBreak")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	GuardAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false);
	SprintAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Sprint")), false);
	PlayerSmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Small")), false);
	PlayerBigHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Big")), false);
	PlayerLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Launch")), false);

	AbilityTags.AddTag(GuardBreakAbilityTag);
	ActivationOwnedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);

	FAbilityTriggerData GuardBreakTrigger;
	GuardBreakTrigger.TriggerTag = GuardBreakEventTag;
	GuardBreakTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(GuardBreakTrigger);

	AbilitiesToCancel.AddTag(GuardAbilityTag);
	AbilitiesToCancel.AddTag(SprintAbilityTag);
	AbilitiesToCancel.AddTag(PlayerSmallHitReactionAbilityTag);
	AbilitiesToCancel.AddTag(PlayerBigHitReactionAbilityTag);
	AbilitiesToCancel.AddTag(PlayerLaunchReactionAbilityTag);
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false));
}

bool UPlayerGuardBreakAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UPlayerGuardBreakAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	bMovementLockedByGuardBreak = false;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard Break activation aborted for '%s': exhausted player, AnimInstance, montage, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayActionMontage* CreatedMontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this,
		NAME_None,
		GuardBreakMontage,
		1.0f,
		NAME_None,
		1.0f, // AnimRootMotionTranslationScale
		0.0f, // StartTimeSeconds
		true, // bAllowInterruptAfterBlendOut
		EActionMontageCancelPolicy::None); // CancelPolicy
	MontageTask = CreatedMontageTask;
	if (!CreatedMontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard Break activation aborted for '%s': failed to create a montage AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player Guard Break activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = GuardBreakMontage;
	ActiveMontageInstanceID = INDEX_NONE;
	// Business teardown waits for the full blend-out; Task interruption reports its start.
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerGuardBreakAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerGuardBreakAbility::OnActiveMontageEnded);
	CreatedMontageTask->ReadyForActivation();

	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask)
	{
		return;
	}

	if (!IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished() || !CreatedMontageTask->IsActive()
		|| !BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard Break activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(GuardBreakMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMontageInstanceID = CreatedMontageTask->GetBoundMontageInstanceID();

	if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		bMovementLockedByGuardBreak = true;
	}

	PlayerCharacter->MarkGuardRequiresReleaseAfterGuardBreak();
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);
}

void UPlayerGuardBreakAbility::EndAbility(
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
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerGuardBreakAbility::OnActiveMontageEnded);
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	ActiveMontage = nullptr;
	ActiveMontageInstanceID = INDEX_NONE;
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	const bool bCanRestoreMovement = PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed()
		&& !(CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag));
	if (bMovementLockedByGuardBreak && bCanRestoreMovement)
	{
		if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}

	bMovementLockedByGuardBreak = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerGuardBreakAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!IsActive() || bEndAbilityRequested || !CurrentActorInfo || Montage != ActiveMontage.Get()
		|| ActiveMontageInstanceID == INDEX_NONE)
	{
		return;
	}

	// Global Ended can deliver a queued receipt from an older play of the same asset.
	const FAnimMontageInstance* CurrentInstance = BoundAnimInstance
		? BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID) : nullptr;
	if (CurrentInstance && CurrentInstance->IsValid())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

bool UPlayerGuardBreakAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	return CharacterASC && PlayerCharacter && AnimInstance && GuardBreakMontage
		&& CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f
		&& GuardBreakAbilityTag.IsValid() && GuardBreakEventTag.IsValid() && StunnedStateTag.IsValid() && DeadStateTag.IsValid()
		&& GuardAbilityTag.IsValid() && SprintAbilityTag.IsValid() && PlayerSmallHitReactionAbilityTag.IsValid()
		&& PlayerBigHitReactionAbilityTag.IsValid() && PlayerLaunchReactionAbilityTag.IsValid() && AbilitiesToCancel.Num() == 10;
}

void UPlayerGuardBreakAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
