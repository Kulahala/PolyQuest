#include "AbilitySystem/Abilities/ChargedAttackAbility.h"

#include "AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

UChargedAttackAbility::UChargedAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false));
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false));
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Resource.Stamina.RegenBlocked")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	ChargedReleaseHandoffEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.ReleaseHandoff")), false);
	HoldReadyEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.HoldReady")), false);
	TraceWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.Begin")), false);
	TraceWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.End")), false);
	ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	DamageDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage.Charged")), false);
	PoiseDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Charged")), false);

	FAbilityTriggerData ChargedReleaseHandoffTrigger;
	ChargedReleaseHandoffTrigger.TriggerTag = ChargedReleaseHandoffEventTag;
	ChargedReleaseHandoffTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(ChargedReleaseHandoffTrigger);
}

bool UChargedAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const bool bPrimaryInputHeld = PlayerCharacter && PrimaryAttackInputTag.IsValid() && PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag);
	const bool bReleasedPrimaryHandoff = SourceTags && PrimaryAttackInputTag.IsValid() && SourceTags->HasTagExact(PrimaryAttackInputTag);
	return PlayerCharacter && (bPrimaryInputHeld || bReleasedPrimaryHandoff);
}

bool UChargedAttackAbility::ShouldAbilityRespondToEvent(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return IsChargedReleaseHandoffEvent(Payload, AvatarActor);
}

void UChargedAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
	TestStartChargeFeedbackCallCount = 0;
	TestCleanupChargeFeedbackCallCount = 0;
	TestFullCallbackCount = 0;
	TestRecordedChargePhase = -1.0f;
	TestDelayDuration = -1.0f;
	TestAttachParent = nullptr;
	TestAttachSocketName = NAME_None;
	bTestChargeVFXActive = false;
#endif
	bEndAbilityRequested = false;
	bChargingStateApplied = false;
	bMontagePausedAtHoldReady = false;
	bReleaseStarted = false;
	DamageMultiplier = 1.0f;
	PoiseDamageMagnitude = 0.0f;
	ActiveMontage = nullptr;
	BoundAnimInstance = nullptr;
	ResetMeleeMotionWarpState();

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (PlayerCharacter)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const bool bReleasedPrimaryHandoff = IsChargedReleaseHandoffEvent(TriggerEventData, PlayerCharacter);

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !ChargedAttackMontage
		|| !CostGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass || !PrimaryAttackInputTag.IsValid()
		|| !InputReleasedEventTag.IsValid() || !InputCanceledEventTag.IsValid() || !ChargedReleaseHandoffEventTag.IsValid() || !HoldReadyEventTag.IsValid()
		|| !TraceWindowBeginEventTag.IsValid() || !TraceWindowEndEventTag.IsValid()
		|| !ChargingStateTag.IsValid() || !DamageDataTag.IsValid() || MinimumChargeDuration > MaximumChargeDuration
		|| MaximumDamageMultiplier < 1.0f || MinimumPoiseDamage <= 0.0f || MaximumPoiseDamage < MinimumPoiseDamage
		|| !PoiseDataTag.IsValid() || (!bReleasedPrimaryHandoff && !PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag)))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': held input, montage, cost/damage/regen effects, valid timing values, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this,
		NAME_None,
		ChargedAttackMontage,
		1.0f,
		NAME_None,
		1.0f, // AnimRootMotionTranslationScale
		0.0f, // StartTimeSeconds
		false, // bAllowInterruptAfterBlendOut
		EActionMontageCancelPolicy::DodgeAndDefense);
	HoldReadyTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HoldReadyEventTag, nullptr, false, true);
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	InputReleasedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputReleasedEventTag, nullptr, false, true);
	InputCanceledTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputCanceledEventTag, nullptr, false, true);

	if (!MontageTask || !HoldReadyTask || !TraceWindowBeginTask || !TraceWindowEndTask || !InputReleasedTask || !InputCanceledTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = ChargedAttackMontage;
	MontageTask->OnCompleted.AddDynamic(this, &UChargedAttackAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UChargedAttackAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UChargedAttackAbility::OnMontageCancelled);
	MontageTask->OnFailed.AddDynamic(this, &UChargedAttackAbility::OnMontageFailed);

	HoldReadyTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnHoldReady);
	TraceWindowBeginTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnTraceWindowBegin);
	TraceWindowEndTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnTraceWindowEnd);
	InputReleasedTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnInputReleased);
	InputCanceledTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnInputCanceled);

	PlayerCharacter->ApplyLockAwareActionFacing();

	HoldReadyTask->ReadyForActivation();
	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	InputReleasedTask->ReadyForActivation();
	InputCanceledTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// A zero-length or otherwise immediately completed Montage can synchronously run EndAbility.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!IsValid(BoundAnimInstance) || !IsValid(ActiveMontage) || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(ChargedAttackMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);

	if (bReleasedPrimaryHandoff)
	{
		BeginRelease(TriggerEventData->EventMagnitude);
	}
	else
	{
		SetCharging(true);
		if (bChargingStateApplied)
		{
			StartChargeFeedback();
		}
	}
}

void UChargedAttackAbility::EndAbility(
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
	SetCharging(false);
	CleanupChargeFeedback();
	CloseTraceWindow();

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}
	ResetMeleeMotionWarpState();

	BoundAnimInstance = nullptr;

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UChargedAttackAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UChargedAttackAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UChargedAttackAbility::OnMontageCancelled);
		MontageTask->OnFailed.RemoveDynamic(this, &UChargedAttackAbility::OnMontageFailed);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (HoldReadyTask)
	{
		HoldReadyTask->EndTask();
		HoldReadyTask = nullptr;
	}

	if (TraceWindowBeginTask)
	{
		TraceWindowBeginTask->EndTask();
		TraceWindowBeginTask = nullptr;
	}

	if (TraceWindowEndTask)
	{
		TraceWindowEndTask->EndTask();
		TraceWindowEndTask = nullptr;
	}

	if (InputReleasedTask)
	{
		InputReleasedTask->EndTask();
		InputReleasedTask = nullptr;
	}

	if (InputCanceledTask)
	{
		InputCanceledTask->EndTask();
		InputCanceledTask = nullptr;
	}

	bMontagePausedAtHoldReady = false;
	bReleaseStarted = false;
	DamageMultiplier = 1.0f;
	PoiseDamageMagnitude = 0.0f;
	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UChargedAttackAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void UChargedAttackAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void UChargedAttackAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

void UChargedAttackAbility::OnMontageFailed()
{
	EndFromMontage(true);
}

void UChargedAttackAbility::OnHoldReady(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload) || bReleaseStarted || bMontagePausedAtHoldReady)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		EndFromMontage(true);
		return;
	}

	if (MontageTask)
	{
		MontageTask->LatchCancelWindowsAcrossPause();
	}
	bMontagePausedAtHoldReady = true;
	BoundAnimInstance->Montage_Pause(ActiveMontage.Get());
}

void UChargedAttackAbility::OnTraceWindowBegin(FGameplayEventData Payload)
{
	if (!bReleaseStarted || !IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	const UAnimNotifyState_AttackTraceWindow* NotifyState = Cast<UAnimNotifyState_AttackTraceWindow>(Payload.OptionalObject2);
	if (!NotifyState)
	{
		return;
	}

	if (TraceWindowTask && ActiveTraceNotifyState.IsValid() && ActiveTraceNotifyState.Get() != NotifyState)
	{
		CloseTraceWindow();
	}

	ActiveTraceNotifyState = NotifyState;
	OpenTraceWindow(NotifyState->GetTraceSourceNames());
}

void UChargedAttackAbility::OnTraceWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	const UAnimNotifyState_AttackTraceWindow* NotifyState = Cast<UAnimNotifyState_AttackTraceWindow>(Payload.OptionalObject2);
	if (!NotifyState || NotifyState != ActiveTraceNotifyState.Get())
	{
		return;
	}

	CloseTraceWindow();
}

void UChargedAttackAbility::OnInputReleased(FGameplayEventData Payload)
{
	if (IsPrimaryAttackInputEvent(Payload))
	{
		BeginRelease(Payload.EventMagnitude);
	}
}

void UChargedAttackAbility::OnInputCanceled(FGameplayEventData Payload)
{
	if (IsPrimaryAttackInputEvent(Payload))
	{
		EndFromMontage(true);
	}
}

void UChargedAttackAbility::BeginRelease(float HeldDuration)
{
	if (bEndAbilityRequested || bReleaseStarted || !CurrentActorInfo)
	{
		return;
	}

	SetCharging(false);
	CleanupChargeFeedback();

	if (!CheckCost(CurrentSpecHandle, CurrentActorInfo, nullptr))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Charged attack release rejected for '%s' because its Stamina cost cannot be paid."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		EndFromMontage(true);
		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack release failed to commit for '%s'."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		EndFromMontage(true);
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageActive = IsValid(BoundAnimInstance) && IsValid(ActiveMontage)
		&& (bTestBypassMontageActiveCheck || BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));
#else
	const bool bMontageActive = IsValid(BoundAnimInstance) && IsValid(ActiveMontage)
		&& BoundAnimInstance->Montage_IsActive(ActiveMontage.Get());
#endif

	if (!bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack release failed for '%s': active montage '%s' is unavailable."), *GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(ActiveMontage));
		EndFromMontage(true);
		return;
	}

	const float ChargeRange = MaximumChargeDuration - MinimumChargeDuration;
	const float ChargeAlpha = ChargeRange > KINDA_SMALL_NUMBER
		? FMath::Clamp((HeldDuration - MinimumChargeDuration) / ChargeRange, 0.0f, 1.0f)
		: 1.0f;
	DamageMultiplier = FMath::Lerp(1.0f, MaximumDamageMultiplier, ChargeAlpha);
	PoiseDamageMagnitude = -FMath::Lerp(MinimumPoiseDamage, MaximumPoiseDamage, ChargeAlpha);
	bReleaseStarted = true;

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		TryApplyMeleeMotionWarpTarget(PlayerCharacter);
	}

	if (bMontagePausedAtHoldReady)
	{
		UAbilityTask_PlayActionMontage* ReleasingTask = MontageTask.Get();
		if (ReleasingTask)
		{
			ReleasingTask->UnlatchCancelWindowsAfterPause();
		}
		// Removing cancel tags can synchronously end this ability and clear the montage.
		if (bEndAbilityRequested || MontageTask != ReleasingTask || !IsValid(BoundAnimInstance) || !IsValid(ActiveMontage))
		{
			return;
		}
		BoundAnimInstance->Montage_Resume(ActiveMontage.Get());
		bMontagePausedAtHoldReady = false;
	}
}

void UChargedAttackAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

bool UChargedAttackAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bEndAbilityRequested || !ActiveMontage || !AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return false;
	}

	const UObject* PayloadObject = Payload.OptionalObject.Get();
	if (!PayloadObject)
	{
		return false;
	}

	if (PayloadObject == ActiveMontage.Get())
	{
		return true;
	}

	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PayloadObject))
	{
		for (const FSlotAnimationTrack& Track : ActiveMontage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (Segment.GetAnimReference() == Sequence)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool UChargedAttackAbility::IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.InstigatorTags.HasTagExact(PrimaryAttackInputTag);
}

bool UChargedAttackAbility::IsChargedReleaseHandoffEvent(const FGameplayEventData* Payload, const AActor* AvatarActor) const
{
	return Payload && AvatarActor && ChargedReleaseHandoffEventTag.IsValid()
		&& Payload->EventTag.MatchesTagExact(ChargedReleaseHandoffEventTag)
		&& Payload->Instigator == AvatarActor && Payload->Target == AvatarActor
		&& Payload->InstigatorTags.HasTagExact(PrimaryAttackInputTag);
}

void UChargedAttackAbility::OpenTraceWindow(const TArray<FName>& InTraceSourceNames)
{
	if (bEndAbilityRequested || !bReleaseStarted)
	{
		return;
	}

	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	SetByCallerMagnitudes.Add(DamageDataTag, -BaseDamage * DamageMultiplier);
	SetByCallerMagnitudes.Add(PoiseDataTag, PoiseDamageMagnitude);

	FMeleeTraceWindowLifecycle::OpenOrKeepMagnitudes(
		this,
		TraceWindowTask,
		DamageGameplayEffectClass,
		GetAbilityLevel(),
		SetByCallerMagnitudes,
		InTraceSourceNames);
}

void UChargedAttackAbility::CloseTraceWindow()
{
	FMeleeTraceWindowLifecycle::CloseAndClear(TraceWindowTask, ActiveTraceNotifyState);
}

void UChargedAttackAbility::SetCharging(bool bShouldCharge)
{
	if (bShouldCharge)
	{
		if (bEndAbilityRequested || bChargingStateApplied)
		{
			return;
		}

		if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystemComponent->AddLooseGameplayTag(ChargingStateTag);
			bChargingStateApplied = true;
		}
		return;
	}

	if (!bChargingStateApplied)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(ChargingStateTag);
	}

	bChargingStateApplied = false;
}



void UChargedAttackAbility::ResetMeleeMotionWarpState()
{
	MeleeMotionWarpSnapshot.Reset();
}

void UChargedAttackAbility::TryApplyMeleeMotionWarpTarget(APlayerCharacter* PlayerCharacter)
{
	FMeleeMotionWarpConfig WarpConfig;
	WarpConfig.bUseMotionWarping = bUseMotionWarping;
	WarpConfig.WarpTargetName = WarpTargetName;
	WarpConfig.MinTriggerDistance = MinTriggerDistance;
	WarpConfig.WarpStopDistance = WarpStopDistance;
	WarpConfig.MaxTriggerDistance = MaxTriggerDistance;
	WarpConfig.MaxWarpAngleDegrees = MaxWarpAngleDegrees;

	// 1. Validate basic motion warp configuration before attempting any target queries.
	// Illegal or disabled configurations do NOT consume the one-shot capture opportunity.
	if (!FMeleeMotionWarpingLifecycle::IsConfigValid(WarpConfig))
	{
		if (IsValid(PlayerCharacter) && !PlayerCharacter->IsActorBeingDestroyed())
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
		}
		return;
	}

	// 2. Consume the one-shot capture opportunity on the very first legal opt-in attempt,
	// BEFORE any Player, World, Controller, ASC, or Lock-On queries.
	const bool bIsFirstLegalCaptureAttempt = !MeleeMotionWarpSnapshot.bAttemptedCapture;
	if (bIsFirstLegalCaptureAttempt)
	{
		MeleeMotionWarpSnapshot.bAttemptedCapture = true;
	}

	// 3. Safety checks on Player and context
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed())
	{
		return;
	}

	UWorld* World = PlayerCharacter->GetWorld();
	const AController* Controller = PlayerCharacter->GetController();
	const UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo();
	if (!World || !IsValid(Controller) || Controller->IsActorBeingDestroyed()
		|| !IsValid(AbilityASC) || AbilityASC->GetOwnerActor() != PlayerCharacter || PlayerCharacter->GetAbilitySystemComponent() != AbilityASC)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
		return;
	}

	// 4. Perform snapshot capture if this was the first legal capture attempt
	if (bIsFirstLegalCaptureAttempt)
	{
		AEnemyCharacter* OriginalTarget = PlayerCharacter->GetLockedTarget();
		if (!IsValid(OriginalTarget) || OriginalTarget->IsActorBeingDestroyed() || OriginalTarget->IsDead() || OriginalTarget->GetWorld() != World)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver, TEXT("[MotionWarp] 未锁定目标 (LockOn)"));
			}
#endif
			PlayerCharacter->ClearMeleeMotionWarpTargets();
			return;
		}

		AEnemyCharacter* ValidatedTarget = PlayerCharacter->ResolveValidLockedTarget();
		if (!IsValid(ValidatedTarget) || ValidatedTarget != OriginalTarget || ValidatedTarget->IsActorBeingDestroyed() || ValidatedTarget->IsDead() || ValidatedTarget->GetWorld() != World)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("[MotionWarp] 目标验证失败或发生死亡切换"));
			}
#endif
			PlayerCharacter->ClearMeleeMotionWarpTargets();
			return;
		}

		const FVector TargetLoc = ValidatedTarget->GetActorLocation();
		if (!FMath::IsFinite(TargetLoc.X) || !FMath::IsFinite(TargetLoc.Y) || !FMath::IsFinite(TargetLoc.Z))
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
			return;
		}

		const UCharacterMovementComponent* TargetMoveComp = ValidatedTarget->GetCharacterMovement();
		const bool bTargetOnGround = TargetMoveComp && TargetMoveComp->IsMovingOnGround();

		// Record successful snapshot
		MeleeMotionWarpSnapshot.CapturedTarget = ValidatedTarget;
		MeleeMotionWarpSnapshot.CapturedTargetLocation = TargetLoc;
		MeleeMotionWarpSnapshot.bCapturedTargetOnGround = bTargetOnGround;
	}

	// 5. Subsequent / current entry target validation
	if (!MeleeMotionWarpSnapshot.CapturedTarget.IsValid())
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
		return;
	}

	AEnemyCharacter* TargetActor = MeleeMotionWarpSnapshot.CapturedTarget.Get();
	if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed() || TargetActor->IsDead() || TargetActor->GetWorld() != World)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
		return;
	}

	// 6. Evaluate motion warp transform using cached target snapshot and current player transform/state
	const UCharacterMovementComponent* PlayerMoveComp = PlayerCharacter->GetCharacterMovement();
	const bool bPlayerOnGround = PlayerMoveComp && PlayerMoveComp->IsMovingOnGround();
	const FVector PlayerLoc = PlayerCharacter->GetActorLocation();
	const FVector PlayerForward = PlayerCharacter->GetActorForwardVector();
	const float Dist2D = FVector::Dist2D(PlayerLoc, MeleeMotionWarpSnapshot.CapturedTargetLocation);

	FTransform WarpTransform;
	if (FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
		PlayerLoc,
		PlayerForward,
		bPlayerOnGround,
		MeleeMotionWarpSnapshot.CapturedTargetLocation,
		MeleeMotionWarpSnapshot.bCapturedTargetOnGround,
		WarpConfig,
		WarpTransform))
	{
		const bool bSetSuccess = PlayerCharacter->SetMeleeMotionWarpTarget(WarpConfig.WarpTargetName, WarpTransform);
		if (!bSetSuccess)
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
			return;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (GEngine)
		{
			const float CorrectionDist = FVector::Dist2D(PlayerLoc, WarpTransform.GetLocation());
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
				FString::Printf(TEXT("[MotionWarp] 成功触发！目标=%s, 距离=%.1fcm, 停距=%.1fcm, 修正=%.1fcm"),
					*GetNameSafe(TargetActor), Dist2D, WarpConfig.WarpStopDistance, CorrectionDist));
		}
		UE_LOG(LogPolyQuest, Log, TEXT("[MotionWarp] Applied warp target '%s' on '%s' (Dist2D=%.1f)"),
			*WarpConfig.WarpTargetName.ToString(), *GetNameSafe(PlayerCharacter), Dist2D);
#endif
	}
	else
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
				FString::Printf(TEXT("[MotionWarp] 判定未通过 (距离=%.1fcm, 允许: %.1f~%.1fcm, 地面=%d/%d)"),
					Dist2D, WarpConfig.MinTriggerDistance, WarpConfig.MaxTriggerDistance,
					bPlayerOnGround ? 1 : 0, MeleeMotionWarpSnapshot.bCapturedTargetOnGround ? 1 : 0));
		}
#endif
	}
}

void UChargedAttackAbility::StartChargeFeedback()
{
	if (bEndAbilityRequested || bReleaseStarted || !bChargingStateApplied)
	{
		return;
	}

	if (!FMath::IsFinite(MaximumChargeDuration) || MaximumChargeDuration <= 0.0f)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack VFX aborted: MaximumChargeDuration (%.3f) is non-positive or non-finite."), MaximumChargeDuration);
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter)
	{
		return;
	}

	if (!ChargeVFXSystem)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (bTestChargeVFXTrackingEnabled)
		{
			TestStartChargeFeedbackCallCount++;
			bTestChargeVFXActive = false;
			TestRecordedChargePhase = -1.0f;
		}
#endif
		return;
	}

	UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	if (!EquipmentComp)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack VFX aborted for '%s': WeaponEquipmentComponent not found."), *GetNameSafe(PlayerCharacter));
		return;
	}

	USceneComponent* AttachParent = nullptr;
	FName AttachSocketName = NAME_None;
	if (!EquipmentComp->TryResolveMainHandChargeVFXAttachment(ChargeVFXTraceSourceName, AttachParent, AttachSocketName) || !IsValid(AttachParent))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack VFX aborted for '%s': failed to resolve attachment for trace source '%s'."),
			*GetNameSafe(PlayerCharacter), *ChargeVFXTraceSourceName.ToString());
		return;
	}

	const float HeldDuration = FMath::Max(0.0f, PlayerCharacter->GetCombatInputHeldDuration(PrimaryAttackInputTag));
	const float RemainingToFull = FMath::Max(0.0f, MaximumChargeDuration - HeldDuration);
	const bool bStartFull = (RemainingToFull <= KINDA_SMALL_NUMBER);
	const float InitialPhase = bStartFull ? 1.0f : 0.0f;

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestChargeVFXTrackingEnabled)
	{
		TestStartChargeFeedbackCallCount++;
		TestAttachParent = AttachParent;
		TestAttachSocketName = AttachSocketName;
		TestDelayDuration = bStartFull ? 0.0f : RemainingToFull;
		TestRecordedChargePhase = bTestForceSpawnNull ? -1.0f : InitialPhase;
		bTestChargeVFXActive = !bTestForceSpawnNull;
	}
	else
#endif
	{
		ChargeVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			ChargeVFXSystem,
			AttachParent,
			AttachSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true,
			false
		);

		if (ChargeVFXComponent)
		{
			static const FName ChargePhaseParamName(TEXT("User.ChargePhase"));
			ChargeVFXComponent->SetVariableFloat(ChargePhaseParamName, InitialPhase);
			ChargeVFXComponent->Activate(true);
		}
	}

	if (bStartFull)
	{
		return;
	}

	WaitDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RemainingToFull);
	if (WaitDelayTask)
	{
		WaitDelayTask->OnFinish.AddDynamic(this, &UChargedAttackAbility::OnChargeFullDelayFinished);
		WaitDelayTask->ReadyForActivation();

		// ReadyForActivation is a synchronous reentrancy boundary.
		if (bEndAbilityRequested || bReleaseStarted || !bChargingStateApplied)
		{
			return;
		}
	}
}

void UChargedAttackAbility::OnChargeFullDelayFinished()
{
	WaitDelayTask = nullptr;

	if (bEndAbilityRequested || bReleaseStarted || !bChargingStateApplied)
	{
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestChargeVFXTrackingEnabled)
	{
		TestFullCallbackCount++;
		if (bTestChargeVFXActive)
		{
			TestRecordedChargePhase = 1.0f;
		}
	}
#endif

	if (IsValid(ChargeVFXComponent) && ChargeVFXComponent->IsActive())
	{
		static const FName ChargePhaseParamName(TEXT("User.ChargePhase"));
		ChargeVFXComponent->SetVariableFloat(ChargePhaseParamName, 1.0f);
	}
}

void UChargedAttackAbility::CleanupChargeFeedback()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestChargeVFXTrackingEnabled)
	{
		TestCleanupChargeFeedbackCallCount++;
		bTestChargeVFXActive = false;
	}
#endif

	if (WaitDelayTask)
	{
		WaitDelayTask->OnFinish.RemoveAll(this);
		WaitDelayTask->EndTask();
		WaitDelayTask = nullptr;
	}

	if (ChargeVFXComponent)
	{
		ChargeVFXComponent->Deactivate();
		ChargeVFXComponent = nullptr;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UChargedAttackAbility::Test_SetBoundMontageForTest(UAnimMontage* Montage)
{
	ActiveMontage = Montage;
	if (Montage && Montage->Notifies.Num() == 0)
	{
		UAnimNotifyState_ActionDodgeCancelWindow* TestNotify = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Montage, TEXT("TestCancelNotify"));
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyName = FName(TEXT("TestCancelNotify"));
		Event.NotifyStateClass = TestNotify;
	}
	if (!MontageTask)
	{
		MontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
			this, NAME_None, Montage, 1.0f, NAME_None, 1.0f, 0.0f, false, EActionMontageCancelPolicy::DodgeAndDefense);
	}
	if (MontageTask)
	{
		MontageTask->SetTestTaskActive(true);
		MontageTask->SetTestBypassMontageActiveCheck(true);
		MontageTask->SetTestBoundMontageInstanceID(1001);
		if (CurrentActorInfo)
		{
			if (ACharacter* Char = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()))
			{
				UAnimInstance* AnimInst = Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr;
				if (!AnimInst && Char->GetMesh())
				{
					AnimInst = NewObject<UAnimInstance>(Char->GetMesh());
					Char->GetMesh()->AnimScriptInstance = AnimInst;
				}
				MontageTask->SetTestBoundAnimInstance(AnimInst);
			}
			if (CurrentActorInfo->AbilitySystemComponent.IsValid())
			{
				MontageTask->SetAbilitySystemComponent(CurrentActorInfo->AbilitySystemComponent.Get());
				MontageTask->TestBindCancelWindowEvents(CurrentActorInfo->AbilitySystemComponent.Get());
			}
		}
	}
}

void UChargedAttackAbility::Test_OnDodgeCancelWindowBegin(const FGameplayEventData& Payload)
{
	if (MontageTask)
	{
		if (!Payload.TargetData.IsValid(0) || !Payload.OptionalObject2)
		{
			FGameplayEventData AdaptedPayload = Payload;
			UAnimInstance* AnimInst = MontageTask->GetBoundAnimInstance();
			const int32 InstanceID = MontageTask->GetBoundMontageInstanceID();
			const UAnimNotifyState* Notify = Cast<UAnimNotifyState>(Payload.OptionalObject2.Get());
			if (!Notify && ActiveMontage && ActiveMontage->Notifies.Num() > 0)
			{
				Notify = ActiveMontage->Notifies[0].NotifyStateClass;
			}
			AdaptedPayload.OptionalObject2 = Notify;
			FGameplayAbilityTargetData_MontageRateWindowSource* SourceData = new FGameplayAbilityTargetData_MontageRateWindowSource();
			SourceData->AnimInstance = AnimInst;
			SourceData->MontageInstanceID = InstanceID;
			SourceData->bReachedEnd = false;
			AdaptedPayload.TargetData.Add(SourceData);
			MontageTask->TestInvokeCancelBegin(AdaptedPayload);
			return;
		}
		MontageTask->TestInvokeCancelBegin(Payload);
	}
}

void UChargedAttackAbility::Test_OnDodgeCancelWindowEnd(const FGameplayEventData& Payload)
{
	if (MontageTask)
	{
		if (!Payload.TargetData.IsValid(0) || !Payload.OptionalObject2)
		{
			FGameplayEventData AdaptedPayload = Payload;
			UAnimInstance* AnimInst = MontageTask->GetBoundAnimInstance();
			const int32 InstanceID = MontageTask->GetBoundMontageInstanceID();
			const UAnimNotifyState* Notify = Cast<UAnimNotifyState>(Payload.OptionalObject2.Get());
			if (!Notify && ActiveMontage && ActiveMontage->Notifies.Num() > 0)
			{
				Notify = ActiveMontage->Notifies[0].NotifyStateClass;
			}
			AdaptedPayload.OptionalObject2 = Notify;
			FGameplayAbilityTargetData_MontageRateWindowSource* SourceData = new FGameplayAbilityTargetData_MontageRateWindowSource();
			SourceData->AnimInstance = AnimInst;
			SourceData->MontageInstanceID = InstanceID;
			SourceData->bReachedEnd = !bMontagePausedAtHoldReady;
			AdaptedPayload.TargetData.Add(SourceData);
			MontageTask->TestInvokeCancelEnd(AdaptedPayload);
			return;
		}
		MontageTask->TestInvokeCancelEnd(Payload);
	}
}

void UChargedAttackAbility::Test_SimulateHoldReady()
{
	if (MontageTask)
	{
		MontageTask->LatchCancelWindowsAcrossPause();
	}
	bMontagePausedAtHoldReady = true;
}

void UChargedAttackAbility::Test_BeginRelease(float HeldDuration)
{
	if (MontageTask)
	{
		MontageTask->UnlatchCancelWindowsAfterPause();
	}
	bReleaseStarted = true;
	bMontagePausedAtHoldReady = false;
}

bool UChargedAttackAbility::Test_IsHoldCancelWindowLatched() const
{
	return MontageTask ? MontageTask->IsCancelWindowLatched() : false;
}

bool UChargedAttackAbility::Test_IsDodgeCancelable() const
{
	return MontageTask ? MontageTask->HasContributedDodgeTag() : false;
}

const FAbilityMontageRateWindowLifecycle& UChargedAttackAbility::GetTestRateWindowLifecycle() const
{
	static const FAbilityMontageRateWindowLifecycle EmptyLifecycle;
	return MontageTask ? MontageTask->GetRateWindowLifecycle() : EmptyLifecycle;
}

FAbilityMontageRateWindowLifecycle& UChargedAttackAbility::GetTestRateWindowLifecycle_Mutable()
{
	check(MontageTask != nullptr);
	return MontageTask->GetRateWindowLifecycle_Mutable();
}

int32 UChargedAttackAbility::GetTestRateWindowMontageInstanceID() const
{
	return MontageTask ? MontageTask->GetBoundMontageInstanceID() : INDEX_NONE;
}

bool UChargedAttackAbility::HasTestRateWindowTasks() const
{
	return MontageTask != nullptr && !MontageTask->IsTerminated();
}

UAbilityTask_PlayActionMontage* UChargedAttackAbility::GetTestMontageTask() const
{
	return MontageTask.Get();
}
#endif
