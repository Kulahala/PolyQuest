#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "GameFramework/Character.h"
#include "PolyQuest.h"

UAbilityTask_PlayActionMontage::UAbilityTask_PlayActionMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
}

UAbilityTask_PlayActionMontage* UAbilityTask_PlayActionMontage::PlayActionMontage(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	UAnimMontage* MontageToPlay,
	float Rate,
	FName StartSection,
	float AnimRootMotionTranslationScale,
	float StartTimeSeconds,
	bool bAllowInterruptAfterBlendOut)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UAbilityTask_PlayActionMontage* MyObj = NewAbilityTask<UAbilityTask_PlayActionMontage>(OwningAbility, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->Rate = Rate;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->StartTimeSeconds = StartTimeSeconds;
	MyObj->bAllowInterruptAfterBlendOut = bAllowInterruptAfterBlendOut;
	return MyObj;
}

void UAbilityTask_PlayActionMontage::Activate()
{
	if (!Ability)
	{
		return;
	}

	bTerminated = false;
	bAllowInterruptAfterBlendOutState = bAllowInterruptAfterBlendOut;

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);

	if (!ASC || !ActorInfo || !AnimInstance || !Avatar || !MontageToPlay
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid()
		|| !FMath::IsFinite(Rate) || Rate <= 0.0f)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("PlayActionMontage [%s]: Activation aborted on Ability %s due to invalid setup."),
			*InstanceName.ToString(), *GetNameSafe(Ability));
		CleanupTask(false);
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFailed.Broadcast();
		}
		EndTask();
		return;
	}

	// 1. Subscribe to RateWindow GameplayEvents on ASC
	BindRateWindowEvents(ASC);

	// 2. Subscribe to Ability cancellation
	InterruptedHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UAbilityTask_PlayActionMontage::OnGameplayAbilityCancelled);

	// 3. Play Montage via ASC
	const float Duration = ASC->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), MontageToPlay, Rate, StartSection, StartTimeSeconds);

	// Synchronous reentrancy check 1: PlayMontage can synchronously fire callbacks
	if (bTerminated || !Ability || !Ability->IsActive() || !ShouldBroadcastAbilityTaskDelegates())
	{
		CleanupTask(false);
		return;
	}

	if (Duration <= 0.0f)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("PlayActionMontage [%s]: ASC::PlayMontage failed for montage %s on Ability %s."),
			*InstanceName.ToString(), *GetNameSafe(MontageToPlay), *GetNameSafe(Ability));
		CleanupTask(false);
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFailed.Broadcast();
		}
		EndTask();
		return;
	}

	// 4. Confirm real active montage instance
	FAnimMontageInstance* CurrentInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
	if (!CurrentInstance || CurrentInstance->IsStopped())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("PlayActionMontage [%s]: montage %s started but active instance not found on AnimInstance %s."),
			*InstanceName.ToString(), *GetNameSafe(MontageToPlay), *GetNameSafe(AnimInstance));
		CleanupTask(false);
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFailed.Broadcast();
		}
		EndTask();
		return;
	}

	BoundAnimInstance = AnimInstance;
	BoundMontageInstanceID = CurrentInstance->GetInstanceID();

	// 5. Capture baseline rate and bind Lifecycle
	RateWindowLifecycle.BindAndCapture(Ability, AnimInstance, MontageToPlay, RateWindowBeginEventTag, RateWindowEndEventTag);
	if (!RateWindowLifecycle.IsBound())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("PlayActionMontage [%s]: RateWindowLifecycle failed to bind montage %s."),
			*InstanceName.ToString(), *GetNameSafe(MontageToPlay));
		CleanupTask(true);
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFailed.Broadcast();
		}
		EndTask();
		return;
	}

	// 6. Bind instance delegates
	BlendedInDelegate.BindUObject(this, &UAbilityTask_PlayActionMontage::OnMontageBlendedIn);
	AnimInstance->Montage_SetBlendedInDelegate(BlendedInDelegate, MontageToPlay);

	BlendingOutDelegate.BindUObject(this, &UAbilityTask_PlayActionMontage::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

	MontageEndedDelegate.BindUObject(this, &UAbilityTask_PlayActionMontage::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

	// 7. Root motion scale
	ACharacter* Character = Cast<ACharacter>(Avatar);
	if (Character && (Character->GetLocalRole() == ROLE_Authority ||
		(Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
	{
		Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
		bAppliedRootMotionScale = true;
	}

	SetWaitingOnAvatar();
}

void UAbilityTask_PlayActionMontage::ExternalCancel()
{
	if (bTerminated)
	{
		Super::ExternalCancel();
		return;
	}

	CleanupTask(true);
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}
	Super::ExternalCancel();
}

FString UAbilityTask_PlayActionMontage::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
		if (AnimInstance)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay) ? ToRawPtr(MontageToPlay) : AnimInstance->GetCurrentActiveMontage();
		}
	}
	return FString::Printf(TEXT("PlayActionMontage. Montage: %s (InstanceID: %d, Playing: %s)"),
		*GetNameSafe(MontageToPlay), BoundMontageInstanceID, *GetNameSafe(PlayingMontage));
}

void UAbilityTask_PlayActionMontage::OnDestroy(bool AbilityEnded)
{
	// EndTask() passes false even when called from the owning Ability's EndAbility.
	// This task still owns playback; cleanup must stop only its authorized instance.
	CleanupTask(true);
	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_PlayActionMontage::OnMontageBlendedIn(UAnimMontage* Montage)
{
	if (bTerminated)
	{
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendedIn.Broadcast();
	}
}

void UAbilityTask_PlayActionMontage::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (bTerminated)
	{
		return;
	}

	const bool bPlayingThisMontage = (Montage == MontageToPlay) && Ability && Ability->GetCurrentMontage() == MontageToPlay;
	if (bPlayingThisMontage && CanResetRootMotionScale())
	{
		ResetRootMotionScale();

		if (bInterrupted || !bAllowInterruptAfterBlendOutState)
		{
			if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
			{
				ASC->ClearAnimatingAbility(Ability);
			}
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (bInterrupted)
		{
			bAllowInterruptAfterBlendOutState = false;
			CleanupTask(false);
			OnInterrupted.Broadcast();
			EndTask();
		}
		else
		{
			OnBlendOut.Broadcast();
		}
	}
}

void UAbilityTask_PlayActionMontage::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bTerminated)
	{
		return;
	}

	// Verify identity: only if another live montage instance has taken over, do not treat old end as active completion
	if (const UAnimInstance* AnimInst = BoundAnimInstance.Get())
	{
		if (const FAnimMontageInstance* CurrentActiveInst = AnimInst->GetActiveInstanceForMontage(MontageToPlay))
		{
			if (BoundMontageInstanceID != INDEX_NONE && CurrentActiveInst->GetInstanceID() != BoundMontageInstanceID)
			{
				CleanupTask(false);
				EndTask();
				return;
			}
		}
	}

	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			CleanupTask(false);
			OnCompleted.Broadcast();
		}
		else
		{
			CleanupTask(false);
		}
	}
	else if (bAllowInterruptAfterBlendOutState)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			CleanupTask(false);
			OnInterrupted.Broadcast();
		}
		else
		{
			CleanupTask(false);
		}
	}
	else
	{
		CleanupTask(false);
	}

	EndTask();
}

void UAbilityTask_PlayActionMontage::OnGameplayAbilityCancelled()
{
	if (bTerminated)
	{
		return;
	}

	const bool bAllowInterrupt = bAllowInterruptAfterBlendOutState;
	const bool bStopped = CleanupTask(true);

	if (bStopped || bAllowInterrupt)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			bAllowInterruptAfterBlendOutState = false;
			OnInterrupted.Broadcast();
		}
	}

	EndTask();
}

void UAbilityTask_PlayActionMontage::BindRateWindowEvents(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	UnbindRateWindowEvents();

	if (RateWindowBeginEventTag.IsValid())
	{
		RateWindowBeginHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(RateWindowBeginEventTag).AddUObject(
			this, &UAbilityTask_PlayActionMontage::OnRateWindowBeginReceived);
	}
	if (RateWindowEndEventTag.IsValid())
	{
		RateWindowEndHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(RateWindowEndEventTag).AddUObject(
			this, &UAbilityTask_PlayActionMontage::OnRateWindowEndReceived);
	}
}

void UAbilityTask_PlayActionMontage::UnbindRateWindowEvents()
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC)
	{
		if (RateWindowBeginHandle.IsValid() && RateWindowBeginEventTag.IsValid())
		{
			ASC->GenericGameplayEventCallbacks.FindOrAdd(RateWindowBeginEventTag).Remove(RateWindowBeginHandle);
			RateWindowBeginHandle.Reset();
		}
		if (RateWindowEndHandle.IsValid() && RateWindowEndEventTag.IsValid())
		{
			ASC->GenericGameplayEventCallbacks.FindOrAdd(RateWindowEndEventTag).Remove(RateWindowEndHandle);
			RateWindowEndHandle.Reset();
		}
	}
}

bool UAbilityTask_PlayActionMontage::ValidateRateWindowEventSource(const FGameplayEventData& Payload) const
{
	if (Payload.TargetData.Num() == 0 || !Payload.TargetData.IsValid(0))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("PlayActionMontage [%s]: RateWindow event rejected due to missing TargetData on Ability %s, Montage %s."),
			*InstanceName.ToString(), *GetNameSafe(Ability), *GetNameSafe(MontageToPlay));
		return false;
	}

	const FGameplayAbilityTargetData* BaseData = Payload.TargetData.Get(0);
	if (!BaseData || BaseData->GetScriptStruct() != FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct())
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("PlayActionMontage [%s]: RateWindow event rejected due to invalid TargetData type on Ability %s, Montage %s."),
			*InstanceName.ToString(), *GetNameSafe(Ability), *GetNameSafe(MontageToPlay));
		return false;
	}

	const FGameplayAbilityTargetData_MontageRateWindowSource* SourceData = static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(BaseData);
	if (!SourceData->AnimInstance.IsValid() || SourceData->AnimInstance.Get() != BoundAnimInstance.Get())
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("PlayActionMontage [%s]: RateWindow event rejected: AnimInstance mismatch (%s vs %s) on Ability %s, Montage %s."),
			*InstanceName.ToString(),
			*GetNameSafe(SourceData->AnimInstance.Get()), *GetNameSafe(BoundAnimInstance.Get()),
			*GetNameSafe(Ability), *GetNameSafe(MontageToPlay));
		return false;
	}

	if (SourceData->MontageInstanceID == INDEX_NONE || BoundMontageInstanceID == INDEX_NONE || SourceData->MontageInstanceID != BoundMontageInstanceID)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("PlayActionMontage [%s]: RateWindow event rejected: invalid or mismatched ID (SourceID=%d vs BoundID=%d) on Ability %s, Montage %s."),
			*InstanceName.ToString(),
			SourceData->MontageInstanceID, BoundMontageInstanceID,
			*GetNameSafe(Ability), *GetNameSafe(MontageToPlay));
		return false;
	}

	return true;
}

void UAbilityTask_PlayActionMontage::OnRateWindowBeginReceived(const FGameplayEventData* Payload)
{
	if (bTerminated || !IsActive() || !Payload)
	{
		return;
	}

	if (!ValidateRateWindowEventSource(*Payload))
	{
		return;
	}

	RateWindowLifecycle.HandleBegin(*Payload);
}

void UAbilityTask_PlayActionMontage::OnRateWindowEndReceived(const FGameplayEventData* Payload)
{
	if (bTerminated || !IsActive() || !Payload)
	{
		return;
	}

	if (!ValidateRateWindowEventSource(*Payload))
	{
		return;
	}

	RateWindowLifecycle.HandleEnd(*Payload);
}

void UAbilityTask_PlayActionMontage::ResetRootMotionScale()
{
	if (!bAppliedRootMotionScale)
	{
		return;
	}
	bAppliedRootMotionScale = false;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character && (Character->GetLocalRole() == ROLE_Authority ||
		(Character->GetLocalRole() == ROLE_AutonomousProxy && Ability && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
	{
		Character->SetAnimRootMotionTranslationScale(1.f);
	}
}

bool UAbilityTask_PlayActionMontage::CanResetRootMotionScale() const
{
	if (!bAppliedRootMotionScale)
	{
		return false;
	}

	const UAnimInstance* AnimInst = BoundAnimInstance.Get();
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();

	// 1. 排他检查：是否有不同的 Montage 已经接管播放
	if (AnimInst)
	{
		const UAnimMontage* ActiveMontage = AnimInst->GetCurrentActiveMontage();
		if (ActiveMontage != nullptr && ActiveMontage != MontageToPlay)
		{
			return false;
		}
	}
	if (ASC)
	{
		const UAnimMontage* ASCMontage = ASC->GetCurrentMontage();
		if (ASCMontage != nullptr && ASCMontage != MontageToPlay)
		{
			return false;
		}
	}

	// 2. 排他检查：是否有同资产的不同实例已经接管播放
	if (AnimInst && MontageToPlay)
	{
		if (const FAnimMontageInstance* CurrentActiveInst = AnimInst->GetActiveInstanceForMontage(MontageToPlay))
		{
			if (BoundMontageInstanceID != INDEX_NONE && CurrentActiveInst->GetInstanceID() != BoundMontageInstanceID)
			{
				return false;
			}
		}
	}

	return true;
}

bool UAbilityTask_PlayActionMontage::CleanupTask(bool bStopMontage)
{
	if (bTerminated)
	{
		return false;
	}
	bTerminated = true;

	// 1. Unbind RateWindow events
	UnbindRateWindowEvents();

	// 2. Unbind cancellation delegate
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(InterruptedHandle);
		InterruptedHandle.Reset();
	}

	// 3. Authorization check
	UAnimInstance* AnimInst = BoundAnimInstance.Get();
#if WITH_DEV_AUTOMATION_TESTS
	const bool bHasAuthorizedInstance = bTestBypassMontageActiveCheck || FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(
		AnimInst, MontageToPlay, BoundMontageInstanceID);
#else
	const bool bHasAuthorizedInstance = FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(
		AnimInst, MontageToPlay, BoundMontageInstanceID);
#endif

	// 4. Restore play rate while still owning the instance
	if (bHasAuthorizedInstance && RateWindowLifecycle.IsBound())
	{
		RateWindowLifecycle.RestoreAndClear();
	}
	else
	{
		RateWindowLifecycle = FAbilityMontageRateWindowLifecycle();
	}

	// 5. Restore Root Motion scale only when authorized and not taken over
	if (CanResetRootMotionScale())
	{
		ResetRootMotionScale();
	}

	// 6. Unbind instance delegates if still our instance
	if (AnimInst && MontageToPlay)
	{
		if (FAnimMontageInstance* CurrentInst = AnimInst->GetActiveInstanceForMontage(MontageToPlay))
		{
			if (CurrentInst->GetInstanceID() == BoundMontageInstanceID)
			{
				CurrentInst->OnMontageBlendedInEnded.Unbind();
				CurrentInst->OnMontageBlendingOutStarted.Unbind();
				CurrentInst->OnMontageEnded.Unbind();
			}
		}
	}

	// 7. Stop montage if requested and authorized
	bool bStoppedMontage = false;
	if (bStopMontage && bHasAuthorizedInstance)
	{
		bStoppedMontage = StopPlayingMontage();
	}

	// 8. Clear references
	BoundAnimInstance.Reset();
	BoundMontageInstanceID = INDEX_NONE;
	bAppliedRootMotionScale = false;

	return bStoppedMontage;
}

bool UAbilityTask_PlayActionMontage::StopPlayingMontage()
{
	if (!Ability)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInst = ActorInfo->GetAnimInstance();
	if (!AnimInst || !MontageToPlay)
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestBypassMontageActiveCheck)
	{
		UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
		if (ASC && ASC->GetAnimatingAbility() == Ability && ASC->GetCurrentMontage() == MontageToPlay)
		{
			ASC->CurrentMontageStop(0.0f);
		}
		else
		{
			AnimInst->Montage_Stop(0.0f, MontageToPlay);
		}
		return true;
	}
#endif

	if (!FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(AnimInst, MontageToPlay, BoundMontageInstanceID))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && ASC->GetAnimatingAbility() == Ability && ASC->GetCurrentMontage() == MontageToPlay)
	{
		ASC->CurrentMontageStop(0.0f);
		return true;
	}

	return false;
}
