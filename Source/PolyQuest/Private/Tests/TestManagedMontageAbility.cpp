#include "Tests/TestManagedMontageAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

FGameplayAbilitySpecHandle FManagedMontageTestHelpers::ActivateExecutionStanceBreak(AEnemyCharacter* Enemy)
{
#if WITH_EDITOR
	UAbilitySystemComponent* ASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
	USkeletalMeshComponent* Mesh = Enemy ? Enemy->GetMesh() : nullptr;
	if (!ASC || !Mesh) return {};
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	if (!Anim)
	{
		Anim = NewObject<UAnimInstance>(Mesh);
		Anim->InitializeMontageOnly();
		Mesh->AnimScriptInstance = Anim;
	}
	// These execution fixtures own their in-memory skeletons. Preserve an existing
	// AnimInstance so subsequent victim presentation keeps the same animation owner.
	USkeleton* Skeleton = Anim->CurrentSkeleton;
	if (!Skeleton) Skeleton = NewObject<USkeleton>(Enemy);
	if (Skeleton->GetReferenceSkeleton().GetNum() == 0)
	{
		FReferenceSkeletonModifier Modifier(Skeleton);
		Modifier.Add(FMeshBoneInfo(TEXT("root"), TEXT("root"), INDEX_NONE), FTransform::Identity);
	}
	Anim->CurrentSkeleton = Skeleton;
	UAnimSequence* Sequence = NewObject<UAnimSequence>(Enemy);
	Sequence->SetSkeleton(Skeleton);
	IAnimationDataController& Controller = Sequence->GetController();
	Controller.InitializeModel();
	{
		IAnimationDataController::FScopedBracket Bracket(Controller, FText::FromString(TEXT("Execution stance prerequisite")), false);
		Controller.SetFrameRate(FFrameRate(30, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(30), false);
		const FName Root = Skeleton->GetReferenceSkeleton().GetBoneName(0);
		Controller.AddBoneCurve(Root, false);
		TArray<FVector3f> Positions; Positions.Init(FVector3f::ZeroVector, 31);
		TArray<FQuat4f> Rotations; Rotations.Init(FQuat4f::Identity, 31);
		TArray<FVector3f> Scales; Scales.Init(FVector3f::OneVector, 31);
		Controller.SetBoneTrackKeys(Root, Positions, Rotations, Scales, false);
		Controller.NotifyPopulated();
	}
	Sequence->WaitOnExistingCompression();
	UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.0f, 0.0f);
	if (!Montage) return {};
	ASC->RefreshAbilityActorInfo();
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy));
	UEnemyStanceBreakAbility* Ability = Cast<UEnemyStanceBreakAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	if (Ability) Ability->SetTestStanceBreakMontage(Montage);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
	if (Ability && ASC->TryActivateAbility(Handle) && Ability->IsActive()) return Handle;
	ASC->ClearAbility(Handle);
#endif
	return {};
}
#endif

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
	FString& OutDiagnosticReason,
	const FGameplayTagContainer& RequiredSourceTags)
{
	#if WITH_DEV_AUTOMATION_TESTS
	OutDiagnosticReason.Reset();
	const FString Context = FString::Printf(TEXT("Ability '%s', Montage '%s'"),
		*GetNameSafe(SourceAbility), *GetNameSafe(Montage));
	const auto Fail = [&](const FString& Reason)
	{
		OutDiagnosticReason = Context + TEXT(": ") + Reason;
		return false;
	};
	if (!IsValid(SourceAbility)) return Fail(TEXT("source ability is missing"));
	if (!IsValid(ActualTask)) return Fail(TEXT("missing standard montage Task"));
	if (!IsValid(Montage)) return Fail(TEXT("missing montage"));
	if (ActualTask->GetMontageToPlay() != Montage || ActualTask->GetTestOwningAbility() != SourceAbility)
		return Fail(TEXT("standard Task source/montage mismatch"));
	if (ActualTask->GetCancelPolicy() != ExpectedPolicy) return Fail(TEXT("cancel policy mismatch"));
	if (ExpectedPolicy == EActionMontageCancelPolicy::None) return true;

	const FGameplayTag DodgeMarker = FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Dodge"));
	const FGameplayTag DefenseMarker = FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Defense"));
	// Explicit legacy cancellation routes declare their required source tags at the call site;
	// the corresponding real GAS test proves the target's post-Commit cancellation behavior.
	if (!RequiredSourceTags.IsEmpty() && !SourceAbility->AbilityTags.HasAllExact(RequiredSourceTags))
		return Fail(TEXT("missing declared cancellation source tags: ") + RequiredSourceTags.ToStringSimple());
	if (RequiredSourceTags.IsEmpty() && !SourceAbility->AbilityTags.HasTagExact(DodgeMarker))
		return Fail(TEXT("missing Ability.Action.CancelableBy.Dodge"));
	if (RequiredSourceTags.IsEmpty() && ExpectedPolicy == EActionMontageCancelPolicy::DodgeAndDefense && !SourceAbility->AbilityTags.HasTagExact(DefenseMarker))
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
	#else
	OutDiagnosticReason = TEXT("Configuration inspection requires automation test support.");
	return false;
	#endif
}

UTestConfigurationOnlyActionAbility::UTestConfigurationOnlyActionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Dodge")));
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Defense")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.Attacking")));
}

void UTestConfigurationOnlyActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	MontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(this, NAME_None, Montage, 1.0f, NAME_None,
		/*AnimRootMotionTranslationScale=*/1.0f, /*StartTimeSeconds=*/0.0f, false, /*CancelPolicy=*/CancelPolicy);
	if (!MontageTask) { OnCancelled(); return; }
	UAbilityTask_PlayActionMontage* const CreatedTask = MontageTask;
	CreatedTask->OnCompleted.AddDynamic(this, &UTestConfigurationOnlyActionAbility::OnCompleted);
	CreatedTask->OnInterrupted.AddDynamic(this, &UTestConfigurationOnlyActionAbility::OnCancelled);
	CreatedTask->OnCancelled.AddDynamic(this, &UTestConfigurationOnlyActionAbility::OnCancelled);
	CreatedTask->OnFailed.AddDynamic(this, &UTestConfigurationOnlyActionAbility::OnCancelled);
	CreatedTask->ReadyForActivation();
	if (!IsActive() || MontageTask != CreatedTask) return;
	if (!CreatedTask->IsActive()) OnCancelled();
}

void UTestConfigurationOnlyActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive()) return;
	if (MontageTask)
	{
		UAbilityTask_PlayActionMontage* EndingTask = MontageTask;
		MontageTask = nullptr;
		EndingTask->OnCompleted.RemoveAll(this);
		EndingTask->OnInterrupted.RemoveAll(this);
		EndingTask->OnCancelled.RemoveAll(this);
		EndingTask->OnFailed.RemoveAll(this);
		EndingTask->EndTask();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTestConfigurationOnlyActionAbility::OnCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UTestConfigurationOnlyActionAbility::OnCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
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
