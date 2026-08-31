#include "AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "PolyQuest.h"

UPlayerSmallHitReactionAbility::UPlayerSmallHitReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRetriggerInstancedAbility = true;

	SmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Small")), false);
	SmallHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Small")), false);
	SmallHitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);

	AbilityTags.AddTag(SmallHitReactionAbilityTag);
	ActivationOwnedTags.AddTag(SmallHitReactingStateTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = SmallHitReactionEventTag;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

bool UPlayerSmallHitReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UPlayerSmallHitReactionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCancelled);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player small hit reaction activation aborted for '%s': ASC, living player, AnimInstance, complete four-way montages (Front, Back, Left, Right), and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector LocalImpactDirection = FVector::ZeroVector;
	if (TriggerEventData)
	{
		LocalImpactDirection = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, PlayerCharacter);
	}

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontSmallHitReactionMontage;
	MontageSet.Back = BackSmallHitReactionMontage;
	MontageSet.Left = LeftSmallHitReactionMontage;
	MontageSet.Right = RightSmallHitReactionMontage;

	UAnimMontage* SelectedMontage = FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(
		LocalImpactDirection, MontageSet);

	if (!SelectedMontage)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player small hit reaction activation aborted for '%s': directional montage selection failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* CreatedMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		SelectedMontage,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		true);
	if (!CreatedMontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player small hit reaction activation aborted for '%s': failed to create a montage AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = CreatedMontageTask;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player small hit reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask || !IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished())
	{
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SelectedMontage;

	MontageTask->OnCompleted.AddDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCancelled);

	MontageTask->ReadyForActivation();

	if (bEndAbilityRequested)
	{
		return;
	}

	if (MontageTask.Get() != CreatedMontageTask || !IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished() || !CreatedMontageTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player small hit reaction activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(ActiveMontage.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
}

void UPlayerSmallHitReactionAbility::EndAbility(
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

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCancelled);
	}

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

	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerSmallHitReactionAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void UPlayerSmallHitReactionAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void UPlayerSmallHitReactionAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

bool UPlayerSmallHitReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	const bool bIsDead = CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontSmallHitReactionMontage;
	MontageSet.Back = BackSmallHitReactionMontage;
	MontageSet.Left = LeftSmallHitReactionMontage;
	MontageSet.Right = RightSmallHitReactionMontage;

	return CharacterASC && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed() && !bIsDead && AnimInstance && MontageSet.IsComplete()
		&& SmallHitReactionAbilityTag.IsValid() && SmallHitReactionEventTag.IsValid() && SmallHitReactingStateTag.IsValid()
		&& StunnedStateTag.IsValid() && DeadStateTag.IsValid();
}

void UPlayerSmallHitReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UPlayerSmallHitReactionAbility::TestBindTaskCallbacks(UAbilityTask_PlayMontageAndWait* InTask)
{
	if (InTask)
	{
		InTask->OnCompleted.AddDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCompleted);
		InTask->OnInterrupted.AddDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageInterrupted);
		InTask->OnCancelled.AddDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCancelled);
	}
}

void UPlayerSmallHitReactionAbility::TestUnbindTaskCallbacks(UAbilityTask_PlayMontageAndWait* InTask)
{
	if (InTask)
	{
		InTask->OnCompleted.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCompleted);
		InTask->OnInterrupted.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageInterrupted);
		InTask->OnCancelled.RemoveDynamic(this, &UPlayerSmallHitReactionAbility::OnMontageCancelled);
	}
}
#endif
