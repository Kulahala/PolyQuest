#include "AbilitySystem/Abilities/LightAttackAbility.h"

#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/BaseCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/ComboChainDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

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
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	TraceWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.Begin")), false);
	TraceWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.End")), false);
	PrimaryAttackPressedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Pressed")), false);
	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	ComboInputWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.InputWindow.Begin")), false);
	ComboInputWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.InputWindow.End")), false);
	ComboBranchWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")), false);
	ComboBranchWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.End")), false);
}

void ULightAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	bDodgeCancelable = false;
	bComboInputWindowOpen = false;
	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
	bComboTransitionInProgress = false;
	ActiveEntryIndex = INDEX_NONE;
	ActiveEntryMontage = nullptr;
	BoundAnimInstance = nullptr;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(AvatarActor);
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !CostGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid()
		|| !TraceWindowBeginEventTag.IsValid() || !TraceWindowEndEventTag.IsValid()
		|| !PrimaryAttackPressedEventTag.IsValid() || !PrimaryAttackInputTag.IsValid() || !ComboInputWindowBeginEventTag.IsValid()
		|| !ComboInputWindowEndEventTag.IsValid() || !ComboBranchWindowBeginEventTag.IsValid() || !ComboBranchWindowEndEventTag.IsValid()
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

	if (!TraceWindowBeginTask || !TraceWindowEndTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask || !PrimaryAttackPressedTask || !ComboInputWindowBeginTask
		|| !ComboInputWindowEndTask || !ComboBranchWindowBeginTask || !ComboBranchWindowEndTask)
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

	BoundAnimInstance = AnimInstance;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &ULightAttackAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &ULightAttackAbility::OnActiveMontageEnded);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Light attack activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->ApplyActionFacing();

	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	PrimaryAttackPressedTask->ReadyForActivation();
	ComboInputWindowBeginTask->ReadyForActivation();
	ComboInputWindowEndTask->ReadyForActivation();
	ComboBranchWindowBeginTask->ReadyForActivation();
	ComboBranchWindowEndTask->ReadyForActivation();

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
	SetDodgeCancelable(false);
	CloseTraceWindow();

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

	bComboInputWindowOpen = false;
	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
	bComboTransitionInProgress = false;
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
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		OpenTraceWindow();
	}
}

void ULightAttackAbility::OnTraceWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		CloseTraceWindow();
	}
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
	const FComboChainEntry* Entry = ComboDefinition ? ComboDefinition->GetEntry(EntryIndex) : nullptr;
	UAnimMontage* EntryMontage = Entry ? Entry->Montage.Get() : nullptr;
	if (bEndAbilityRequested || !BoundAnimInstance || !EntryMontage)
	{
		return false;
	}

	UAbilityTask_PlayMontageAndWait* NewMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, EntryMontage);
	if (!NewMontageTask)
	{
		return false;
	}

	const int32 PreviousEntryIndex = ActiveEntryIndex;
	UAnimMontage* PreviousEntryMontage = ActiveEntryMontage.Get();
	UAbilityTask_PlayMontageAndWait* PreviousMontageTask = MontageTask.Get();

	// Set the new identity before playback interrupts the prior montage.
	SetDodgeCancelable(false);
	CloseTraceWindow();
	bComboInputWindowOpen = false;
	bComboBranchWindowOpen = false;
	bContinuationBuffered = false;
	ActiveEntryIndex = EntryIndex;
	ActiveEntryMontage = EntryMontage;
	MontageTask = NewMontageTask;
	NewMontageTask->ReadyForActivation();

	// A zero-length or otherwise immediately completed Montage can synchronously run EndAbility.
	if (bEndAbilityRequested)
	{
		return false;
	}

	if (!BoundAnimInstance->Montage_IsActive(EntryMontage))
	{
		NewMontageTask->EndTask();
		MontageTask = PreviousMontageTask;
		ActiveEntryIndex = PreviousEntryIndex;
		ActiveEntryMontage = PreviousEntryMontage;
		return false;
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

void ULightAttackAbility::SetDodgeCancelable(bool bShouldBeCancelable)
{
	if (bShouldBeCancelable)
	{
		if (bEndAbilityRequested || bDodgeCancelable)
		{
			return;
		}

		UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
		if (!AbilitySystemComponent || !DodgeCancelableStateTag.IsValid())
		{
			return;
		}

		AbilitySystemComponent->AddLooseGameplayTag(DodgeCancelableStateTag);
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
	}

	bDodgeCancelable = false;
}

void ULightAttackAbility::OpenTraceWindow()
{
	if (bEndAbilityRequested)
	{
		return;
	}

	if (TraceWindowTask && !TraceWindowTask->IsTraceWindowOpen())
	{
		TraceWindowTask = nullptr;
	}

	if (TraceWindowTask)
	{
		return;
	}

	ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo());
	TraceWindowTask = Character
		? UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(this, Character->GetMeleeTraceSource(), DamageGameplayEffectClass, GetAbilityLevel(), FGameplayTag(), 0.0f)
		: nullptr;
	if (TraceWindowTask)
	{
		TraceWindowTask->ReadyForActivation();
		if (!TraceWindowTask->IsTraceWindowOpen())
		{
			TraceWindowTask = nullptr;
		}
	}
}

void ULightAttackAbility::CloseTraceWindow()
{
	if (TraceWindowTask)
	{
		TraceWindowTask->EndTask();
		TraceWindowTask = nullptr;
	}
}
