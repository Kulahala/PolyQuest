#include "AbilitySystem/Abilities/EnemySmallHitReactionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PolyQuest.h"

void UEnemySmallHitReactionRateWindowContext::OnRateWindowBegin(FGameplayEventData Payload)
{
	if (UEnemySmallHitReactionAbility* Ability = OwningAbility.Get())
	{
		if (Ability->CurrentActivationToken == Token)
		{
			Ability->OnRateWindowBegin(Payload);
		}
	}
}

void UEnemySmallHitReactionRateWindowContext::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (UEnemySmallHitReactionAbility* Ability = OwningAbility.Get())
	{
		if (Ability->CurrentActivationToken == Token)
		{
			Ability->OnRateWindowEnd(Payload);
		}
	}
}

UEnemySmallHitReactionAbility::UEnemySmallHitReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRetriggerInstancedAbility = true;

	SmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);
	SmallHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Small")), false);
	SmallHitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);

	AbilityTags.AddTag(SmallHitReactionAbilityTag);
	ActivationOwnedTags.AddTag(SmallHitReactingStateTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = SmallHitReactionEventTag;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

bool UEnemySmallHitReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UEnemySmallHitReactionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;

	ClearRateWindow(true);

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': ASC, living enemy, AnimInstance, complete four-way montages (Front, Back, Left, Right), and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector LocalImpactDirection = FVector::ZeroVector;
	if (TriggerEventData)
	{
		LocalImpactDirection = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, EnemyCharacter);
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
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': directional montage selection failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	++CurrentActivationToken;
	ActiveRateWindowContext = NewObject<UEnemySmallHitReactionRateWindowContext>(this);
	ActiveRateWindowContext->OwningAbility = this;
	ActiveRateWindowContext->Token = CurrentActivationToken;

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
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);
	if (!CreatedMontageTask || !RateWindowBeginTask || !RateWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = CreatedMontageTask;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy small hit reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask || !IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished())
	{
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SelectedMontage;

	RateWindowBeginTask->EventReceived.AddDynamic(ActiveRateWindowContext.Get(), &UEnemySmallHitReactionRateWindowContext::OnRateWindowBegin);
	RateWindowEndTask->EventReceived.AddDynamic(ActiveRateWindowContext.Get(), &UEnemySmallHitReactionRateWindowContext::OnRateWindowEnd);

	RateWindowBeginTask->ReadyForActivation();
	if (bEndAbilityRequested || !IsActive() || !RateWindowBeginTask || !RateWindowBeginTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RateWindowEndTask->ReadyForActivation();
	if (bEndAbilityRequested || !IsActive() || !RateWindowEndTask || !RateWindowEndTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);

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

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageActive = bTestBypassMontageActiveCheck || (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));
#else
	const bool bMontageActive = BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get());
#endif

	if (!BoundAnimInstance || !ActiveMontage || !bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(ActiveMontage.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (FAnimMontageInstance* Instance = BoundAnimInstance->GetActiveInstanceForMontage(ActiveMontage.Get()))
	{
		ActiveMontageInstanceID = Instance->GetInstanceID();
	}
#if WITH_DEV_AUTOMATION_TESTS
	else if (bTestBypassMontageActiveCheck)
	{
		ActiveMontageInstanceID = 1;
	}
#endif

	RateWindowLifecycle.BindAndCapture(this, BoundAnimInstance.Get(), ActiveMontage.Get(), RateWindowBeginEventTag, RateWindowEndEventTag);
}

void UEnemySmallHitReactionAbility::EndAbility(
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
	ClearRateWindow(true);

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
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

void UEnemySmallHitReactionAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void UEnemySmallHitReactionAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void UEnemySmallHitReactionAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

bool UEnemySmallHitReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontSmallHitReactionMontage;
	MontageSet.Back = BackSmallHitReactionMontage;
	MontageSet.Left = LeftSmallHitReactionMontage;
	MontageSet.Right = RightSmallHitReactionMontage;

	return CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && AnimInstance && MontageSet.IsComplete()
		&& SmallHitReactionAbilityTag.IsValid() && SmallHitReactionEventTag.IsValid() && SmallHitReactingStateTag.IsValid()
		&& StunnedStateTag.IsValid() && DeadStateTag.IsValid()
		&& RateWindowBeginEventTag.IsValid() && RateWindowEndEventTag.IsValid();
}

void UEnemySmallHitReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UEnemySmallHitReactionAbility::OnRateWindowBegin(const FGameplayEventData& Payload)
{
	if (bEndAbilityRequested || !IsActive())
	{
		return;
	}

	if (BoundAnimInstance && ActiveMontage && ActiveMontageInstanceID != INDEX_NONE)
	{
		const FAnimMontageInstance* CurrentInst = BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID);
#if WITH_DEV_AUTOMATION_TESTS
		const bool bInstanceValid = bTestBypassMontageActiveCheck || (CurrentInst && CurrentInst->Montage == ActiveMontage && !CurrentInst->IsStopped());
#else
		const bool bInstanceValid = (CurrentInst && CurrentInst->Montage == ActiveMontage && !CurrentInst->IsStopped());
#endif
		if (!bInstanceValid)
		{
			return;
		}
	}
	else
	{
		return;
	}

	RateWindowLifecycle.HandleBegin(Payload);
}

void UEnemySmallHitReactionAbility::OnRateWindowEnd(const FGameplayEventData& Payload)
{
	if (bEndAbilityRequested || !IsActive())
	{
		return;
	}

	if (BoundAnimInstance && ActiveMontage && ActiveMontageInstanceID != INDEX_NONE)
	{
		const FAnimMontageInstance* CurrentInst = BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID);
#if WITH_DEV_AUTOMATION_TESTS
		const bool bInstanceValid = bTestBypassMontageActiveCheck || (CurrentInst && CurrentInst->Montage == ActiveMontage && !CurrentInst->IsStopped());
#else
		const bool bInstanceValid = (CurrentInst && CurrentInst->Montage == ActiveMontage && !CurrentInst->IsStopped());
#endif
		if (!bInstanceValid)
		{
			return;
		}
	}
	else
	{
		return;
	}

	RateWindowLifecycle.HandleEnd(Payload);
}

void UEnemySmallHitReactionAbility::ClearRateWindow(bool bRestoreRate)
{
	if (ActiveRateWindowContext)
	{
		ActiveRateWindowContext->OwningAbility.Reset();
		ActiveRateWindowContext->Token = 0;
		ActiveRateWindowContext = nullptr;
	}

	if (RateWindowBeginTask)
	{
		RateWindowBeginTask->EndTask();
		RateWindowBeginTask = nullptr;
	}

	if (RateWindowEndTask)
	{
		RateWindowEndTask->EndTask();
		RateWindowEndTask = nullptr;
	}

	if (bRestoreRate && RateWindowLifecycle.IsBound())
	{
		bool bInstanceValid = false;
		if (BoundAnimInstance && ActiveMontage && ActiveMontageInstanceID != INDEX_NONE)
		{
			const FAnimMontageInstance* CurrentInst = BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID);
#if WITH_DEV_AUTOMATION_TESTS
			bInstanceValid = bTestBypassMontageActiveCheck || (CurrentInst && CurrentInst->Montage == ActiveMontage && !CurrentInst->IsStopped());
#else
			bInstanceValid = (CurrentInst && CurrentInst->Montage == ActiveMontage && !CurrentInst->IsStopped());
#endif
		}

		if (bInstanceValid)
		{
			RateWindowLifecycle.RestoreAndClear();
		}
		else
		{
			RateWindowLifecycle = FAbilityMontageRateWindowLifecycle();
		}
	}
	else
	{
		RateWindowLifecycle = FAbilityMontageRateWindowLifecycle();
	}

#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif

	ActiveMontageInstanceID = INDEX_NONE;
}

#if WITH_DEV_AUTOMATION_TESTS
void UEnemySmallHitReactionAbility::TestBindTaskCallbacks(UAbilityTask_PlayMontageAndWait* InTask)
{
	if (InTask)
	{
		InTask->OnCompleted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		InTask->OnInterrupted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		InTask->OnCancelled.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
	}
}

void UEnemySmallHitReactionAbility::TestUnbindTaskCallbacks(UAbilityTask_PlayMontageAndWait* InTask)
{
	if (InTask)
	{
		InTask->OnCompleted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		InTask->OnInterrupted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		InTask->OnCancelled.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
	}
}
#endif
