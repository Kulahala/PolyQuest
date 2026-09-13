#include "Tests/TestManagedMontageAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/UnrealType.h"

FGameplayAbilityTargetDataHandle FManagedMontageTestHelpers::MakeRateWindowTargetData(UAnimInstance* AnimInstance, int32 MontageInstanceID)
{
	FGameplayAbilityTargetData_MontageRateWindowSource* Data = new FGameplayAbilityTargetData_MontageRateWindowSource();
	Data->AnimInstance = AnimInstance;
	Data->MontageInstanceID = MontageInstanceID;
	return FGameplayAbilityTargetDataHandle(Data);
}

FGameplayEventData FManagedMontageTestHelpers::MakeRateWindowEventData(
	const FGameplayTag& EventTag,
	UAnimInstance* AnimInstance,
	int32 MontageInstanceID,
	float Rate,
	AActor* AvatarActor,
	UObject* Animation,
	const UObject* RateNotify)
{
	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.EventMagnitude = Rate;

	AActor* EffectiveActor = AvatarActor;
	if (!EffectiveActor && AnimInstance)
	{
		EffectiveActor = AnimInstance->GetOwningActor();
	}
	EventData.Instigator = EffectiveActor;
	EventData.Target = EffectiveActor;
	EventData.OptionalObject = Animation;
	EventData.OptionalObject2 = RateNotify;

	EventData.TargetData = MakeRateWindowTargetData(AnimInstance, MontageInstanceID);
	return EventData;
}

FGameplayAbilityTargetDataHandle FManagedMontageTestHelpers::MakeCancelWindowTargetData(UAnimInstance* AnimInstance, int32 MontageInstanceID, bool bReachedEnd)
{
	FGameplayAbilityTargetData_MontageRateWindowSource* Data = new FGameplayAbilityTargetData_MontageRateWindowSource();
	Data->AnimInstance = AnimInstance;
	Data->MontageInstanceID = MontageInstanceID;
	Data->bReachedEnd = bReachedEnd;
	return FGameplayAbilityTargetDataHandle(Data);
}

FGameplayEventData FManagedMontageTestHelpers::MakeCancelWindowEventData(
	const FGameplayTag& EventTag,
	UAnimInstance* AnimInstance,
	int32 MontageInstanceID,
	AActor* AvatarActor,
	UObject* Animation,
	const UObject* NotifyState,
	bool bReachedEnd)
{
	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.EventMagnitude = 1.0f;

	AActor* EffectiveActor = AvatarActor;
	if (!EffectiveActor && AnimInstance)
	{
		EffectiveActor = AnimInstance->GetOwningActor();
	}
	EventData.Instigator = EffectiveActor;
	EventData.Target = EffectiveActor;
	EventData.OptionalObject = Animation;
	EventData.OptionalObject2 = NotifyState;

	EventData.TargetData = MakeCancelWindowTargetData(AnimInstance, MontageInstanceID, bReachedEnd);
	return EventData;
}

bool FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(
	const UGameplayAbility* SourceAbility,
	const UAnimMontage* Montage,
	const UAbilityTask_PlayActionMontage* ActualTask,
	EActionMontageCancelPolicy ExpectedPolicy,
	const TArray<const UGameplayAbility*>& TargetAbilities,
	FString& OutDiagnosticReason)
{
	OutDiagnosticReason.Reset();
	const FString Context = FString::Printf(TEXT("Ability '%s', Montage '%s'"),
		*GetNameSafe(SourceAbility), *GetNameSafe(Montage));
	const auto Fail = [&](const FString& Reason)
	{
		OutDiagnosticReason = Context + TEXT(": ") + Reason;
		return false;
	};
	if (!IsValid(SourceAbility)) return Fail(TEXT("source ability is missing"));
	if (ExpectedPolicy == EActionMontageCancelPolicy::None)
	{
		return !ActualTask || ActualTask->GetCancelPolicy() == ExpectedPolicy
			? true : Fail(TEXT("cancel policy mismatch: expected None"));
	}
	if (!ActualTask) return Fail(TEXT("missing standard montage Task"));
	if (ActualTask->GetCancelPolicy() != ExpectedPolicy) return Fail(TEXT("cancel policy mismatch"));
	if (!Montage) return Fail(TEXT("missing montage"));

	const FGameplayTag DodgeMarker = FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Dodge"));
	const FGameplayTag DefenseMarker = FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Defense"));
	if (!SourceAbility->AbilityTags.HasTagExact(DodgeMarker))
		return Fail(TEXT("missing Ability.Action.CancelableBy.Dodge"));
	if (ExpectedPolicy == EActionMontageCancelPolicy::DodgeAndDefense && !SourceAbility->AbilityTags.HasTagExact(DefenseMarker))
		return Fail(TEXT("missing Ability.Action.CancelableBy.Defense"));

	const auto HasCancelNotify = [](const UAnimSequenceBase* Animation)
	{
		if (!Animation) return false;
		for (const FAnimNotifyEvent& Event : Animation->Notifies)
		{
			if (Cast<UAnimNotifyState_ActionDodgeCancelWindow>(Event.NotifyStateClass)) return true;
		}
		return false;
	};
	bool bHasWindow = HasCancelNotify(Montage);
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			bHasWindow |= HasCancelNotify(Segment.GetAnimReference());
	if (!bHasWindow) return Fail(TEXT("montage and direct segments lack CancelWindow Notify"));

	// Offline configuration inspection only; never run in the montage task's activation path.
	const auto ReadTags = [](const UGameplayAbility* Ability, const FName PropertyName) -> const FGameplayTagContainer*
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), PropertyName);
		return Property && Property->Struct == FGameplayTagContainer::StaticStruct()
			? Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Ability) : nullptr;
	};
	const FGameplayTagContainer* SourceBlocks = ReadTags(SourceAbility, TEXT("BlockAbilitiesWithTag"));
	const FGameplayTagContainer* SourceOwned = ReadTags(SourceAbility, TEXT("ActivationOwnedTags"));
	if (!SourceBlocks || !SourceOwned) return Fail(TEXT("source tag properties unavailable"));
	FGameplayTagContainer WindowOwned = *SourceOwned;
	WindowOwned.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge")));
	if (ExpectedPolicy == EActionMontageCancelPolicy::DodgeAndDefense)
		WindowOwned.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense")));
	if (TargetAbilities.IsEmpty()) return Fail(TEXT("missing target ability list"));
	for (const UGameplayAbility* Target : TargetAbilities)
	{
		if (!IsValid(Target)) return Fail(TEXT("target ability is missing"));
		const FGameplayTagContainer* TargetBlocked = ReadTags(Target, TEXT("ActivationBlockedTags"));
		const FGameplayTagContainer* TargetEarlyCancel = ReadTags(Target, TEXT("CancelAbilitiesWithTag"));
		const FString TargetName = FString::Printf(TEXT("target '%s': "), *GetNameSafe(Target));
		if (!TargetBlocked || !TargetEarlyCancel) return Fail(TargetName + TEXT("tag properties unavailable"));
		if (Target->AbilityTags.HasAny(*SourceBlocks))
			return Fail(TargetName + TEXT("source BlockAbilitiesWithTag blocks activation"));
		if (WindowOwned.HasAny(*TargetBlocked))
			return Fail(TargetName + TEXT("ActivationBlockedTags conflict with source/window tags"));
		if (SourceAbility->AbilityTags.HasAny(*TargetEarlyCancel))
			return Fail(TargetName + TEXT("CancelAbilitiesWithTag cancels source before Commit"));
	}
	return true;
}

UTestManagedMontageAbility::UTestManagedMontageAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.Attacking"), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Input.Block.Movement"), false));
}

void UTestManagedMontageAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (TestMontage)
	{
		PlayActionMontage(TestMontage, TestRate, NAME_None, InitialRootMotionScale, 0.0f, bTestAllowInterruptAfterBlendOut, TestCancelPolicy);
	}
}

void UTestManagedMontageAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAbilityTask_PlayActionMontage* UTestManagedMontageAbility::PlayActionMontage(
	UAnimMontage* MontageToPlay,
	float Rate,
	FName StartSection,
	float AnimRootMotionTranslationScale,
	float StartTimeSeconds,
	bool bAllowInterruptAfterBlendOut,
	EActionMontageCancelPolicy CancelPolicy)
{
	MontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this,
		NAME_None,
		MontageToPlay,
		Rate,
		StartSection,
		AnimRootMotionTranslationScale,
		StartTimeSeconds,
		bAllowInterruptAfterBlendOut,
		CancelPolicy);

	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UTestManagedMontageAbility::OnMontageCompleted);
		MontageTask->OnBlendedIn.AddDynamic(this, &UTestManagedMontageAbility::OnMontageBlendedIn);
		MontageTask->OnBlendOut.AddDynamic(this, &UTestManagedMontageAbility::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UTestManagedMontageAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UTestManagedMontageAbility::OnMontageCancelled);
		MontageTask->OnFailed.AddDynamic(this, &UTestManagedMontageAbility::OnMontageFailed);
		MontageTask->ReadyForActivation();
	}

	return MontageTask;
}

void UTestManagedMontageAbility::OnMontageCompleted()
{
	bCompletedCalled = true;
	CompletedCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UTestManagedMontageAbility::OnMontageBlendedIn()
{
	bBlendedInCalled = true;
	BlendedInCallCount++;
}

void UTestManagedMontageAbility::OnMontageBlendOut()
{
	bBlendOutCalled = true;
	BlendOutCallCount++;
}

void UTestManagedMontageAbility::OnMontageInterrupted()
{
	bInterruptedCalled = true;
	InterruptedCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UTestManagedMontageAbility::OnMontageCancelled()
{
	bCancelledCalled = true;
	CancelledCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UTestManagedMontageAbility::OnMontageFailed()
{
	bFailedCalled = true;
	FailedCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
