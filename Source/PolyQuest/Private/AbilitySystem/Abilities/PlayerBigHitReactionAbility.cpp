#include "AbilitySystem/Abilities/PlayerBigHitReactionAbility.h"

#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UPlayerBigHitReactionAbility::UPlayerBigHitReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	BigHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Big")), false);
	BigHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Big")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	CancelableByDodgeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);

	AbilityTags.AddTag(BigHitReactionAbilityTag);
	if (CancelableByDodgeAbilityTag.IsValid())
	{
		AbilityTags.AddTag(CancelableByDodgeAbilityTag);
	}

	ActivationOwnedTags.AddTag(HitReactingStateTag);
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));

	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(HyperArmorStateTag);
	ActivationBlockedTags.AddTag(HitReactingStateTag);

	FAbilityTriggerData BigHitReactionTrigger;
	BigHitReactionTrigger.TriggerTag = BigHitReactionEventTag;
	BigHitReactionTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(BigHitReactionTrigger);

	const TArray<FName> TargetActionTagNames = {
		TEXT("Ability.Attack.Primary"),
		TEXT("Ability.Attack.Light"),
		TEXT("Ability.Attack.Charged"),
		TEXT("Ability.Attack.Sprint"),
		TEXT("Ability.Action.CancelableBy.Reaction"),
		TEXT("Ability.Dodge"),
		TEXT("Ability.Movement.Sprint"),
		TEXT("Ability.Movement.Jump"),
		TEXT("Ability.Defense.Guard"),
		TEXT("Ability.Defense.Parry"),
		TEXT("Ability.Reaction.Player.Small")
	};

	for (const FName& TagName : TargetActionTagNames)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (TagName != TEXT("Ability.Dodge"))
		{
			BlockAbilitiesWithTag.AddTag(Tag);
		}
		AbilitiesToCancel.AddTag(Tag);
	}
}

bool UPlayerBigHitReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UPlayerBigHitReactionAbility::ActivateAbility(
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
	ActiveMontageInstanceID = INDEX_NONE;
	BoundPlayerCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !MovementComponent || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player big hit reaction activation aborted for '%s': ASC, living player, AnimInstance, complete four-way montages (Front, Back, Left, Right), grounded movement, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (TriggerEventData)
	{
		ImpactDirectionSnapshot = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, PlayerCharacter);
	}

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontBigHitReactionMontage;
	MontageSet.Back = BackBigHitReactionMontage;
	MontageSet.Left = LeftBigHitReactionMontage;
	MontageSet.Right = RightBigHitReactionMontage;

	UAnimMontage* SelectedMontage = FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(
		ImpactDirectionSnapshot, MontageSet);

	if (!SelectedMontage)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player big hit reaction activation aborted for '%s': directional montage selection failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayActionMontage* CreatedMontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this, NAME_None, SelectedMontage, 1.0f, NAME_None,
		1.0f, // AnimRootMotionTranslationScale
		0.0f, // StartTimeSeconds
		true, // bAllowInterruptAfterBlendOut
		EActionMontageCancelPolicy::DodgeOnly); // CancelPolicy
	MontageTask = CreatedMontageTask;

	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player big hit reaction activation aborted for '%s': failed to create tasks."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bCommitSucceeded = CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask) return;
	if (!bCommitSucceeded)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player big hit reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SelectedMontage;
	BoundPlayerCharacter = PlayerCharacter;

	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerBigHitReactionAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerBigHitReactionAbility::OnActiveMontageEnded);
	CreatedMontageTask->OnFailed.AddDynamic(this, &UPlayerBigHitReactionAbility::OnMontageFailed);
	CreatedMontageTask->ReadyForActivation();

	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player big hit reaction activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(ActiveMontage.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FAnimMontageInstance* StartedInstance = BoundAnimInstance->GetActiveInstanceForMontage(ActiveMontage);
	ActiveMontageInstanceID = StartedInstance ? StartedInstance->GetInstanceID() : INDEX_NONE;

	// 1. Stop current velocity
	MovementComponent->StopMovementImmediately();

	// 2. Snapshot and disable ledge walk-off
	bSavedCanWalkOffLedges = MovementComponent->bCanWalkOffLedges;
	MovementComponent->bCanWalkOffLedges = false;
	bLedgeSettingModified = true;

	// 3. Bind MovementModeChangedDelegate for falling teardown
	PlayerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UPlayerBigHitReactionAbility::OnMovementModeChanged);
	PlayerCharacter->MovementModeChangedDelegate.AddDynamic(this, &UPlayerBigHitReactionAbility::OnMovementModeChanged);
	bMovementModeDelegateBound = true;

	// 4. Cancel pre-existing combat actions after Montage is confirmed active
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);
}

void UPlayerBigHitReactionAbility::EndAbility(
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

	APlayerCharacter* PlayerCharacter = BoundPlayerCharacter.IsValid() ? BoundPlayerCharacter.Get() : Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());

	if (bMovementModeDelegateBound && PlayerCharacter)
	{
		PlayerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UPlayerBigHitReactionAbility::OnMovementModeChanged);
		bMovementModeDelegateBound = false;
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerBigHitReactionAbility::OnActiveMontageEnded);
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->OnFailed.RemoveAll(this);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	ActiveMontage = nullptr;
	ActiveMontageInstanceID = INDEX_NONE;

	if (bLedgeSettingModified && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed())
	{
		if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
		{
			MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
		}
		bLedgeSettingModified = false;
	}

	BoundPlayerCharacter.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerBigHitReactionAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!IsActive() || bEndAbilityRequested || Montage != ActiveMontage.Get() || ActiveMontageInstanceID == INDEX_NONE) return;
	const FAnimMontageInstance* Instance = IsValid(BoundAnimInstance)
		? BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID) : nullptr;
	if (Instance && Instance->IsValid()) return;
	EndFromMontage(bInterrupted);
}

void UPlayerBigHitReactionAbility::OnMontageFailed()
{
	if (IsActive() && !bEndAbilityRequested) EndFromMontage(true);
}

void UPlayerBigHitReactionAbility::OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
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

bool UPlayerBigHitReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	const bool bIsDead = CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontBigHitReactionMontage;
	MontageSet.Back = BackBigHitReactionMontage;
	MontageSet.Left = LeftBigHitReactionMontage;
	MontageSet.Right = RightBigHitReactionMontage;

	return CharacterASC && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed() && !bIsDead && AnimInstance && MontageSet.IsComplete()
		&& MovementComponent && MovementComponent->IsMovingOnGround()
		&& BigHitReactionAbilityTag.IsValid() && BigHitReactionEventTag.IsValid() && HitReactingStateTag.IsValid()
		&& StunnedStateTag.IsValid() && DeadStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& CancelableByDodgeAbilityTag.IsValid()
		&& BlockAbilitiesWithTag.Num() == 10 && AbilitiesToCancel.Num() == 11;
}

void UPlayerBigHitReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
const FAbilityMontageRateWindowLifecycle& UPlayerBigHitReactionAbility::GetTestRateWindowLifecycle() const
{
	static const FAbilityMontageRateWindowLifecycle EmptyLifecycle;
	return MontageTask ? MontageTask->GetRateWindowLifecycle() : EmptyLifecycle;
}

FAbilityMontageRateWindowLifecycle& UPlayerBigHitReactionAbility::GetTestRateWindowLifecycle_Mutable()
{
	check(MontageTask);
	return MontageTask->GetRateWindowLifecycle_Mutable();
}

int32 UPlayerBigHitReactionAbility::GetTestRateWindowMontageInstanceID() const
{
	return MontageTask ? MontageTask->GetBoundMontageInstanceID() : INDEX_NONE;
}

bool UPlayerBigHitReactionAbility::HasTestRateWindowTasks() const
{
	return MontageTask && MontageTask->IsActive() && !MontageTask->IsTerminated();
}

bool UPlayerBigHitReactionAbility::GetTestDodgeCancelable() const
{
	return MontageTask && MontageTask->HasContributedDodgeTag();
}
#endif
