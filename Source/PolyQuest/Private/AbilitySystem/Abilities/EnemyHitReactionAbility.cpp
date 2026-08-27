#include "AbilitySystem/Abilities/EnemyHitReactionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemyHitReactionAbility::UEnemyHitReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	HitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false);
	HitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Big")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	EnemySmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);

	AbilityTags.AddTag(HitReactionAbilityTag);
	ActivationOwnedTags.AddTag(HitReactingStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));
	ActivationBlockedTags.AddTag(HitReactingStateTag);
	ActivationBlockedTags.AddTag(HyperArmorStateTag);

	FAbilityTriggerData HitReactionTrigger;
	HitReactionTrigger.TriggerTag = HitReactionEventTag;
	HitReactionTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(HitReactionTrigger);

	AbilitiesToCancel.AddTag(EnemyMeleeAbilityTag);
	AbilitiesToCancel.AddTag(EnemySmallHitReactionAbilityTag);
}

bool UEnemyHitReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UEnemyHitReactionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;
	bLedgeSettingModified = false;
	bMovementModeDelegateBound = false;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	BoundEnemyCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !MovementComponent || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy hit reaction activation aborted for '%s': ASC, living enemy, AnimInstance, complete four-way montages (Front, Back, Left, Right), grounded movement, and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (TriggerEventData)
	{
		ImpactDirectionSnapshot = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, EnemyCharacter);
	}

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontHitReactionMontage;
	MontageSet.Back = BackHitReactionMontage;
	MontageSet.Left = LeftHitReactionMontage;
	MontageSet.Right = RightHitReactionMontage;

	UAnimMontage* SelectedMontage = FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(
		ImpactDirectionSnapshot, MontageSet);

	if (!SelectedMontage)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy hit reaction activation aborted for '%s': directional montage selection failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SelectedMontage);
	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy hit reaction activation aborted for '%s': failed to create a montage AbilityTask."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy hit reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SelectedMontage;
	BoundEnemyCharacter = EnemyCharacter;

	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyHitReactionAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyHitReactionAbility::OnActiveMontageEnded);
	MontageTask->ReadyForActivation();

	// Zero-length or invalid authored Montages can synchronously reach the unified teardown.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy hit reaction activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(ActiveMontage.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. Stop AI navigation movement if available
	if (AAIController* AIController = EnemyCharacter->GetController<AAIController>())
	{
		AIController->StopMovement();
	}

	// 2. Stop current velocity
	MovementComponent->StopMovementImmediately();

	// 3. Snapshot and disable ledge walk-off
	bSavedCanWalkOffLedges = MovementComponent->bCanWalkOffLedges;
	MovementComponent->bCanWalkOffLedges = false;
	bLedgeSettingModified = true;

	// 4. Bind MovementModeChangedDelegate for falling teardown
	EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyHitReactionAbility::OnMovementModeChanged);
	EnemyCharacter->MovementModeChangedDelegate.AddDynamic(this, &UEnemyHitReactionAbility::OnMovementModeChanged);
	bMovementModeDelegateBound = true;

	// 5. Cancel enemy melee and small hit reaction after montage is confirmed active
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);
}

void UEnemyHitReactionAbility::EndAbility(
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
	AEnemyCharacter* EnemyCharacter = BoundEnemyCharacter.IsValid() ? BoundEnemyCharacter.Get() : Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());

	if (bMovementModeDelegateBound && EnemyCharacter)
	{
		EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyHitReactionAbility::OnMovementModeChanged);
		bMovementModeDelegateBound = false;
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyHitReactionAbility::OnActiveMontageEnded);
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

	if (bLedgeSettingModified && EnemyCharacter && !EnemyCharacter->IsActorBeingDestroyed())
	{
		if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
		{
			MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
		}
		bLedgeSettingModified = false;
	}

	BoundEnemyCharacter.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemyHitReactionAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void UEnemyHitReactionAbility::OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (bEndAbilityRequested)
	{
		return;
	}

	if (Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling())
	{
		EndFromMontage(true);
	}
}

bool UEnemyHitReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontHitReactionMontage;
	MontageSet.Back = BackHitReactionMontage;
	MontageSet.Left = LeftHitReactionMontage;
	MontageSet.Right = RightHitReactionMontage;

	return CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && AnimInstance && MontageSet.IsComplete()
		&& MovementComponent && MovementComponent->IsMovingOnGround()
		&& HitReactionAbilityTag.IsValid() && HitReactionEventTag.IsValid() && HitReactingStateTag.IsValid() && StunnedStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& EnemyMeleeAbilityTag.IsValid() && EnemySmallHitReactionAbilityTag.IsValid() && AbilitiesToCancel.Num() == 2;
}

void UEnemyHitReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
