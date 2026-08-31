#include "AbilitySystem/Abilities/ChargedAttackAbility.h"

#include "AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
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
	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	DefenseCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
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
#endif
	bEndAbilityRequested = false;
	bDodgeCancelable = false;
	bChargingStateApplied = false;
	bMontagePausedAtHoldReady = false;
	bHoldCancelWindowLatchedAcrossPause = false;
	bReleaseStarted = false;
	bRateWindowApplied = false;
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
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid()
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid()
		|| !ChargingStateTag.IsValid() || !DamageDataTag.IsValid() || MinimumChargeDuration > MaximumChargeDuration
		|| MaximumDamageMultiplier < 1.0f || MinimumPoiseDamage <= 0.0f || MaximumPoiseDamage < MinimumPoiseDamage
		|| !PoiseDataTag.IsValid() || (!bReleasedPrimaryHandoff && !PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag)))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': held input, montage, cost/damage/regen effects, valid timing values, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ChargedAttackMontage);
	HoldReadyTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HoldReadyEventTag, nullptr, false, true);
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	InputReleasedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputReleasedEventTag, nullptr, false, true);
	InputCanceledTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputCanceledEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);

	if (!MontageTask || !HoldReadyTask || !TraceWindowBeginTask || !TraceWindowEndTask || !InputReleasedTask || !InputCanceledTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask
		|| !RateWindowBeginTask || !RateWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = ChargedAttackMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UChargedAttackAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UChargedAttackAbility::OnActiveMontageEnded);

	HoldReadyTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnHoldReady);
	TraceWindowBeginTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnTraceWindowBegin);
	TraceWindowEndTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnTraceWindowEnd);
	InputReleasedTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnInputReleased);
	InputCanceledTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnInputCanceled);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnDodgeCancelWindowEnd);
	RateWindowBeginTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnRateWindowBegin);
	RateWindowEndTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnRateWindowEnd);

	PlayerCharacter->ApplyLockAwareActionFacing();

	HoldReadyTask->ReadyForActivation();
	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	InputReleasedTask->ReadyForActivation();
	InputCanceledTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	RateWindowBeginTask->ReadyForActivation();
	RateWindowEndTask->ReadyForActivation();
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
	bHoldCancelWindowLatchedAcrossPause = false;
	SetCharging(false);
	SetDodgeCancelable(false);
	CloseTraceWindow();
	RestoreBaselineMontageRate();

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}
	ResetMeleeMotionWarpState();

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UChargedAttackAbility::OnActiveMontageEnded);
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

	if (DodgeCancelWindowBeginTask)
	{
		DodgeCancelWindowBeginTask->EndTask();
		DodgeCancelWindowBeginTask = nullptr;
	}

	if (DodgeCancelWindowEndTask)
	{
		DodgeCancelWindowEndTask->EndTask();
		DodgeCancelWindowEndTask = nullptr;
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

	bMontagePausedAtHoldReady = false;
	bReleaseStarted = false;
	bRateWindowApplied = false;
	DamageMultiplier = 1.0f;
	PoiseDamageMagnitude = 0.0f;
	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UChargedAttackAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
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

	bHoldCancelWindowLatchedAcrossPause = bDodgeCancelable;
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

void UChargedAttackAbility::OnDodgeCancelWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(true);
	}
}

void UChargedAttackAbility::OnDodgeCancelWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	if (bMontagePausedAtHoldReady && !bReleaseStarted && bHoldCancelWindowLatchedAcrossPause)
	{
		return;
	}

	bHoldCancelWindowLatchedAcrossPause = false;
	SetDodgeCancelable(false);
}

void UChargedAttackAbility::BeginRelease(float HeldDuration)
{
	if (bEndAbilityRequested || bReleaseStarted || !CurrentActorInfo)
	{
		return;
	}

	SetCharging(false);

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

void UChargedAttackAbility::OnRateWindowBegin(FGameplayEventData Payload)
{
	// Ignore duplicate Begin events; authored rate windows must not overlap.
	if (bRateWindowApplied || !IsGameplayEventFromActiveMontage(Payload) || Payload.EventMagnitude <= 0.0f)
	{
		return;
	}

	if (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveMontage.Get(), Payload.EventMagnitude);
		bRateWindowApplied = true;
	}
}

void UChargedAttackAbility::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	RestoreBaselineMontageRate();
}

void UChargedAttackAbility::RestoreBaselineMontageRate()
{
	if (!bRateWindowApplied)
	{
		return;
	}

	bRateWindowApplied = false;
	if (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveMontage.Get(), 1.0f);
	}
}

void UChargedAttackAbility::SetDodgeCancelable(bool bShouldBeCancelable)
{
	if (bShouldBeCancelable)
	{
		if (bEndAbilityRequested || bDodgeCancelable)
		{
			return;
		}

		UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
		if (!AbilitySystemComponent || !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid())
		{
			return;
		}

		AbilitySystemComponent->AddLooseGameplayTag(DodgeCancelableStateTag);
		AbilitySystemComponent->AddLooseGameplayTag(DefenseCancelableStateTag);
		bDodgeCancelable = true;
		return;
	}

	if (!bDodgeCancelable)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(DodgeCancelableStateTag);
		}
		if (DefenseCancelableStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(DefenseCancelableStateTag);
		}
	}

	bDodgeCancelable = false;
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
