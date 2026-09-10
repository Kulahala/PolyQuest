#include "AbilitySystem/Abilities/LightAttackAbility.h"

#include "AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/ComboChainDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

namespace
{
	constexpr int32 MaxMotionWarpAllowedEntryIndex = 2;
}

ULightAttackAbility::ULightAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	DefenseCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
	TraceWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.Begin")), false);
	TraceWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.End")), false);
	PrimaryAttackPressedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Pressed")), false);
	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	ComboInputWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.InputWindow.Begin")), false);
	ComboInputWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.InputWindow.End")), false);
	ComboBranchWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")), false);
	ComboBranchWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.End")), false);
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
}

void ULightAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	ResetMeleeMotionWarpState();
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif
	bDodgeCancelable = false;
	bComboInputWindowOpen = false;
	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
	bComboTransitionInProgress = false;
	ActiveRateWindowCount = 0;
	bRateWindowApplied = false;
	ActiveEntryIndex = INDEX_NONE;
	ActiveEntryMontage = nullptr;
	BoundAnimInstance = nullptr;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(AvatarActor);
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !CostGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid()
		|| !TraceWindowBeginEventTag.IsValid() || !TraceWindowEndEventTag.IsValid()
		|| !PrimaryAttackPressedEventTag.IsValid() || !PrimaryAttackInputTag.IsValid() || !ComboInputWindowBeginEventTag.IsValid()
		|| !ComboInputWindowEndEventTag.IsValid() || !ComboBranchWindowBeginEventTag.IsValid() || !ComboBranchWindowEndEventTag.IsValid()
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid()
		|| !ValidateComboDefinition())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': ASC, player, AnimInstance, valid ComboDefinition, cost effect, damage effect, Stamina regeneration delay effect, and required gameplay tags are required."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	PrimaryAttackPressedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, PrimaryAttackPressedEventTag, nullptr, false, true);
	ComboInputWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboInputWindowBeginEventTag, nullptr, false, true);
	ComboInputWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboInputWindowEndEventTag, nullptr, false, true);
	ComboBranchWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboBranchWindowBeginEventTag, nullptr, false, true);
	ComboBranchWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboBranchWindowEndEventTag, nullptr, false, true);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);

	if (!TraceWindowBeginTask || !TraceWindowEndTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask || !PrimaryAttackPressedTask || !ComboInputWindowBeginTask
		|| !ComboInputWindowEndTask || !ComboBranchWindowBeginTask || !ComboBranchWindowEndTask || !RateWindowBeginTask || !RateWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': failed to create a GameplayEvent AbilityTask."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TraceWindowBeginTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnTraceWindowBegin);
	TraceWindowEndTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnTraceWindowEnd);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnDodgeCancelWindowEnd);
	PrimaryAttackPressedTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnPrimaryAttackPressed);
	ComboInputWindowBeginTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnComboInputWindowBegin);
	ComboInputWindowEndTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnComboInputWindowEnd);
	ComboBranchWindowBeginTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnComboBranchWindowBegin);
	ComboBranchWindowEndTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnComboBranchWindowEnd);
	RateWindowBeginTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnRateWindowBegin);
	RateWindowEndTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnRateWindowEnd);

	BoundAnimInstance = AnimInstance;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &ULightAttackAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &ULightAttackAbility::OnActiveMontageEnded);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Light attack activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->ApplyLockAwareActionFacing();

	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	PrimaryAttackPressedTask->ReadyForActivation();
	ComboInputWindowBeginTask->ReadyForActivation();
	ComboInputWindowEndTask->ReadyForActivation();
	ComboBranchWindowBeginTask->ReadyForActivation();
	ComboBranchWindowEndTask->ReadyForActivation();
	RateWindowBeginTask->ReadyForActivation();
	RateWindowEndTask->ReadyForActivation();

	if (!StartComboEntry(0))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': failed to start ComboDefinition entry 0."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void ULightAttackAbility::EndAbility(
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
	ResetMeleeMotionWarpState();
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif
	SetDodgeCancelable(false);
	CloseTraceWindow();
	RestoreBaselineMontageRate();

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &ULightAttackAbility::OnActiveMontageEnded);
		if (ActiveEntryMontage && BoundAnimInstance->Montage_IsActive(ActiveEntryMontage))
		{
			BoundAnimInstance->Montage_Stop(0.0f, ActiveEntryMontage);
		}
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
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

	if (PrimaryAttackPressedTask)
	{
		PrimaryAttackPressedTask->EndTask();
		PrimaryAttackPressedTask = nullptr;
	}

	if (ComboInputWindowBeginTask)
	{
		ComboInputWindowBeginTask->EndTask();
		ComboInputWindowBeginTask = nullptr;
	}

	if (ComboInputWindowEndTask)
	{
		ComboInputWindowEndTask->EndTask();
		ComboInputWindowEndTask = nullptr;
	}

	if (ComboBranchWindowBeginTask)
	{
		ComboBranchWindowBeginTask->EndTask();
		ComboBranchWindowBeginTask = nullptr;
	}

	if (ComboBranchWindowEndTask)
	{
		ComboBranchWindowEndTask->EndTask();
		ComboBranchWindowEndTask = nullptr;
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

	bComboInputWindowOpen = false;
	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
	bComboTransitionInProgress = false;
	ActiveRateWindowCount = 0;
	bRateWindowApplied = false;
	ActiveEntryIndex = INDEX_NONE;
	ActiveEntryMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULightAttackAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveEntryMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void ULightAttackAbility::OnTraceWindowBegin(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
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

void ULightAttackAbility::OnTraceWindowEnd(FGameplayEventData Payload)
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

void ULightAttackAbility::OnDodgeCancelWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(true);
	}
}

void ULightAttackAbility::OnDodgeCancelWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(false);
	}
}

void ULightAttackAbility::OnPrimaryAttackPressed(FGameplayEventData Payload)
{
	if (!IsPrimaryAttackInputEvent(Payload) || bComboTransitionInProgress || !ComboDefinition || ActiveEntryIndex == INDEX_NONE
		|| ActiveEntryIndex + 1 >= ComboDefinition->GetEntryCount() || (!bComboInputWindowOpen && !bComboBranchWindowOpen) || bContinuationBuffered)
	{
		return;
	}

	bContinuationBuffered = true;
	UE_LOG(LogPolyQuest, Verbose, TEXT("LightAttack.Combo: buffered primary input for entry %d."), ActiveEntryIndex + 1);

	if (bComboBranchWindowOpen)
	{
		TryConsumeBufferedComboContinuation();
	}
}

void ULightAttackAbility::OnComboInputWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		bComboInputWindowOpen = true;
	}
}

void ULightAttackAbility::OnComboInputWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		bComboInputWindowOpen = false;
	}
}

void ULightAttackAbility::OnComboBranchWindowBegin(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	bComboBranchWindowOpen = true;
	TryConsumeBufferedComboContinuation();
}

void ULightAttackAbility::OnComboBranchWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
}

void ULightAttackAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

bool ULightAttackAbility::ValidateComboDefinition() const
{
	if (!ComboDefinition || ComboDefinition->GetEntryCount() <= 0)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack ComboDefinition is missing or empty."));
		return false;
	}

	TSet<const UAnimMontage*> SeenMontages;
	for (int32 EntryIndex = 0; EntryIndex < ComboDefinition->GetEntryCount(); ++EntryIndex)
	{
		const FComboChainEntry* Entry = ComboDefinition->GetEntry(EntryIndex);
		const UAnimMontage* EntryMontage = Entry ? Entry->Montage.Get() : nullptr;
		if (!EntryMontage)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Light attack ComboDefinition '%s' entry %d has no Montage."), *GetNameSafe(ComboDefinition), EntryIndex);
			return false;
		}

		if (SeenMontages.Contains(EntryMontage))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Light attack ComboDefinition '%s' reuses Montage '%s'. Each entry must have a unique complete Montage."), *GetNameSafe(ComboDefinition), *GetNameSafe(EntryMontage));
			return false;
		}

		SeenMontages.Add(EntryMontage);
	}

	return true;
}

bool ULightAttackAbility::StartComboEntry(int32 EntryIndex)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	UWorld* World = PlayerCharacter->GetWorld();
	if (bEndAbilityRequested || !CurrentActorInfo || !World)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
		return false;
	}

	const FComboChainEntry* Entry = ComboDefinition ? ComboDefinition->GetEntry(EntryIndex) : nullptr;
	UAnimMontage* EntryMontage = Entry ? Entry->Montage.Get() : nullptr;
	if (!IsValid(BoundAnimInstance) || !IsValid(EntryMontage))
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
		return false;
	}

	UAbilityTask_PlayMontageAndWait* NewMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, EntryMontage);
	if (!NewMontageTask)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
		return false;
	}

	const int32 PreviousEntryIndex = ActiveEntryIndex;
	UAnimMontage* PreviousEntryMontage = ActiveEntryMontage.Get();
	UAbilityTask_PlayMontageAndWait* PreviousMontageTask = MontageTask.Get();

	// Set the new identity before playback interrupts the prior montage.
	RestoreBaselineMontageRate();
	SetDodgeCancelable(false);
	CloseTraceWindow();
	bComboInputWindowOpen = false;
	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
	ActiveEntryIndex = EntryIndex;
	ActiveEntryMontage = EntryMontage;
	MontageTask = NewMontageTask;

	PlayerCharacter->ClearMeleeMotionWarpTargets();
	if (Entry && Entry->bUseMotionWarping && EntryIndex >= 0 && EntryIndex <= MaxMotionWarpAllowedEntryIndex)
	{
		TryApplyMeleeMotionWarpTarget(PlayerCharacter, *Entry);
	}
	else if (Entry && Entry->bUseMotionWarping && EntryIndex > MaxMotionWarpAllowedEntryIndex)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Light attack entry %d requested Motion Warping but exceeds maximum allowed entry index (%d); fail-closed."), EntryIndex, MaxMotionWarpAllowedEntryIndex);
	}

	NewMontageTask->ReadyForActivation();

	// 1. Synchronously ended ability or destroyed context check
	if (bEndAbilityRequested || !IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed()
		|| !IsValid(BoundAnimInstance) || !IsValid(EntryMontage) || !IsValid(NewMontageTask)
		|| MontageTask.Get() != NewMontageTask)
	{
		// Ability has already ended or context was invalidated; EndAbility() has performed canonical teardown.
		// We must NOT resurrect previous montage task or entry identity.
		if (IsValid(NewMontageTask) && !NewMontageTask->IsFinished())
		{
			NewMontageTask->EndTask();
		}
		if (IsValid(PlayerCharacter) && !PlayerCharacter->IsActorBeingDestroyed())
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
		}
		return false;
	}

	// 2. Task activation state validation on valid active ability
	const bool bTaskActive = NewMontageTask->IsActive() && !NewMontageTask->IsFinished();
	if (!bTaskActive)
	{
		if (IsValid(NewMontageTask) && !NewMontageTask->IsFinished())
		{
			NewMontageTask->EndTask();
		}
		MontageTask = PreviousMontageTask;
		ActiveEntryIndex = PreviousEntryIndex;
		ActiveEntryMontage = PreviousEntryMontage;
		if (IsValid(PlayerCharacter) && !PlayerCharacter->IsActorBeingDestroyed())
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
		}
		return false;
	}

	// 3. Montage playback active check (bypassable strictly for headless test environments)
#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageActive = bTestBypassMontageActiveCheck || BoundAnimInstance->Montage_IsActive(EntryMontage);
#else
	const bool bMontageActive = BoundAnimInstance->Montage_IsActive(EntryMontage);
#endif

	if (!bMontageActive)
	{
		if (IsValid(NewMontageTask) && !NewMontageTask->IsFinished())
		{
			NewMontageTask->EndTask();
		}
		MontageTask = PreviousMontageTask;
		ActiveEntryIndex = PreviousEntryIndex;
		ActiveEntryMontage = PreviousEntryMontage;
		if (IsValid(PlayerCharacter) && !PlayerCharacter->IsActorBeingDestroyed())
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
		}
		return false;
	}

	if (EntryIndex == 0)
	{
		if (PlayerCharacter)
		{
			PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
		}
	}

	UE_LOG(LogPolyQuest, Verbose, TEXT("LightAttack.Combo: started entry %d with montage '%s'."), EntryIndex + 1, *GetNameSafe(EntryMontage));
	return true;
}

bool ULightAttackAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && ActiveEntryMontage && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.OptionalObject.Get() == ActiveEntryMontage.Get();
}

bool ULightAttackAbility::IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.InstigatorTags.HasTagExact(PrimaryAttackInputTag);
}

void ULightAttackAbility::TryConsumeBufferedComboContinuation()
{
	if (bEndAbilityRequested || bComboTransitionInProgress || !bContinuationBuffered || !bComboBranchWindowOpen || !ComboDefinition
		|| ActiveEntryIndex == INDEX_NONE)
	{
		return;
	}

	const int32 NextEntryIndex = ActiveEntryIndex + 1;
	if (!ComboDefinition->GetEntry(NextEntryIndex))
	{
		bContinuationBuffered = false;
		return;
	}

	bContinuationBuffered = false;
	if (!CurrentActorInfo || !CheckCost(CurrentSpecHandle, CurrentActorInfo, nullptr))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("LightAttack.Combo: entry %d was rejected because its Stamina cost cannot be paid."), NextEntryIndex + 1);
		return;
	}

	bComboTransitionInProgress = true;
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr))
	{
		bComboTransitionInProgress = false;
		UE_LOG(LogPolyQuest, Warning, TEXT("LightAttack.Combo: failed to commit the Stamina cost for entry %d."), NextEntryIndex + 1);
		return;
	}

	if (!StartComboEntry(NextEntryIndex))
	{
		bComboTransitionInProgress = false;
		UE_LOG(LogPolyQuest, Warning, TEXT("LightAttack.Combo: failed to start entry %d after committing its cost."), NextEntryIndex + 1);
		EndFromMontage(true);
		return;
	}

	bComboTransitionInProgress = false;
}

void ULightAttackAbility::OnRateWindowBegin(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload) || Payload.EventMagnitude <= 0.0f)
	{
		return;
	}

	if (BoundAnimInstance && ActiveEntryMontage && BoundAnimInstance->Montage_IsActive(ActiveEntryMontage.Get()))
	{
		// Last-one-wins: latest rate window overrides current play rate for rhythmic cadence.
		BoundAnimInstance->Montage_SetPlayRate(ActiveEntryMontage.Get(), Payload.EventMagnitude);
		ActiveRateWindowCount++;
		bRateWindowApplied = true;
	}
}

void ULightAttackAbility::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	if (ActiveRateWindowCount > 0)
	{
		ActiveRateWindowCount--;
	}

	if (ActiveRateWindowCount == 0)
	{
		RestoreBaselineMontageRate();
	}
}

void ULightAttackAbility::RestoreBaselineMontageRate()
{
	ActiveRateWindowCount = 0;
	if (!bRateWindowApplied)
	{
		return;
	}

	bRateWindowApplied = false;
	if (BoundAnimInstance && ActiveEntryMontage && BoundAnimInstance->Montage_IsActive(ActiveEntryMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveEntryMontage.Get(), 1.0f);
	}
}

void ULightAttackAbility::SetDodgeCancelable(bool bShouldBeCancelable)
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

void ULightAttackAbility::OpenTraceWindow(const TArray<FName>& InTraceSourceNames)
{
	if (bEndAbilityRequested)
	{
		return;
	}

	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		this,
		TraceWindowTask,
		DamageGameplayEffectClass,
		GetAbilityLevel(),
		FGameplayTag(),
		0.0f,
		0.0f,
		InTraceSourceNames);
}

void ULightAttackAbility::CloseTraceWindow()
{
	FMeleeTraceWindowLifecycle::CloseAndClear(TraceWindowTask, ActiveTraceNotifyState);
}

bool ULightAttackAbility::EvaluateMeleeMotionWarpTransform(
	const FVector& PlayerLocation,
	const FVector& PlayerForwardVector,
	const bool bPlayerOnGround,
	const FVector& TargetLocation,
	const bool bTargetOnGround,
	const FComboChainEntry& EntryConfig,
	FTransform& OutWarpTransform)
{
	FMeleeMotionWarpConfig WarpConfig;
	WarpConfig.bUseMotionWarping = EntryConfig.bUseMotionWarping;
	WarpConfig.WarpTargetName = EntryConfig.WarpTargetName;
	WarpConfig.MinTriggerDistance = EntryConfig.MinTriggerDistance;
	WarpConfig.WarpStopDistance = EntryConfig.WarpStopDistance;
	WarpConfig.MaxTriggerDistance = EntryConfig.MaxTriggerDistance;
	WarpConfig.MaxWarpAngleDegrees = EntryConfig.MaxWarpAngleDegrees;

	return FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
		PlayerLocation,
		PlayerForwardVector,
		bPlayerOnGround,
		TargetLocation,
		bTargetOnGround,
		WarpConfig,
		OutWarpTransform);
}

void ULightAttackAbility::ResetMeleeMotionWarpState()
{
	MeleeMotionWarpSnapshot.Reset();
}

void ULightAttackAbility::TryApplyMeleeMotionWarpTarget(APlayerCharacter* PlayerCharacter, const FComboChainEntry& EntryConfig)
{
	FMeleeMotionWarpConfig WarpConfig;
	WarpConfig.bUseMotionWarping = EntryConfig.bUseMotionWarping;
	WarpConfig.WarpTargetName = EntryConfig.WarpTargetName;
	WarpConfig.MinTriggerDistance = EntryConfig.MinTriggerDistance;
	WarpConfig.WarpStopDistance = EntryConfig.WarpStopDistance;
	WarpConfig.MaxTriggerDistance = EntryConfig.MaxTriggerDistance;
	WarpConfig.MaxWarpAngleDegrees = EntryConfig.MaxWarpAngleDegrees;

	// 1. Validate basic entry motion warp configuration before attempting any target queries.
	// Illegal or disabled configurations do NOT consume the one-shot capture opportunity.
	if (!FMeleeMotionWarpingLifecycle::IsConfigValid(WarpConfig))
	{
		if (IsValid(PlayerCharacter) && !PlayerCharacter->IsActorBeingDestroyed())
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
		}
		return;
	}

	// 2. Consume the one-shot capture opportunity on the very first legal opt-in entry,
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

	// 3. Evaluate motion warp transform using cached target snapshot and current player transform/state
	const UCharacterMovementComponent* PlayerMoveComp = PlayerCharacter->GetCharacterMovement();
	const bool bPlayerOnGround = PlayerMoveComp && PlayerMoveComp->IsMovingOnGround();
	const FVector PlayerLoc = PlayerCharacter->GetActorLocation();
	const FVector PlayerForward = PlayerCharacter->GetActorForwardVector();
	const float Dist2D = FVector::Dist2D(PlayerLoc, MeleeMotionWarpSnapshot.CapturedTargetLocation);

	FTransform WarpTransform;
	if (EvaluateMeleeMotionWarpTransform(
		PlayerLoc,
		PlayerForward,
		bPlayerOnGround,
		MeleeMotionWarpSnapshot.CapturedTargetLocation,
		MeleeMotionWarpSnapshot.bCapturedTargetOnGround,
		EntryConfig,
		WarpTransform))
	{
		const bool bSetSuccess = PlayerCharacter->SetMeleeMotionWarpTarget(EntryConfig.WarpTargetName, WarpTransform);
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
					*GetNameSafe(TargetActor), Dist2D, EntryConfig.WarpStopDistance, CorrectionDist));
		}
		UE_LOG(LogPolyQuest, Log, TEXT("[MotionWarp] Applied warp target '%s' on '%s' (Dist2D=%.1f)"),
			*EntryConfig.WarpTargetName.ToString(), *GetNameSafe(PlayerCharacter), Dist2D);
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
					Dist2D, EntryConfig.MinTriggerDistance, EntryConfig.MaxTriggerDistance,
					bPlayerOnGround ? 1 : 0, MeleeMotionWarpSnapshot.bCapturedTargetOnGround ? 1 : 0));
		}
#endif
	}
}
