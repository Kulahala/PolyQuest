#include "AbilitySystem/Abilities/SprintAttackAbility.h"

#include "AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/BaseCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

USprintAttackAbility::USprintAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	SprintStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	MovementInputBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	JumpInputBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	StaminaRegenBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Resource.Stamina.RegenBlocked")), false);
	TraceWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.Begin")), false);
	TraceWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.End")), false);
	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	DefenseCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
}

bool USprintAttackAbility::CanActivateAbility(
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

	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return AbilitySystemComponent && PlayerCharacter && SprintAttackMontage && CostGameplayEffectClass && DamageGameplayEffectClass
		&& StaminaRegenDelayGameplayEffectClass && SprintStateTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(SprintStateTag)
		&& PlayerCharacter->ShouldRequestSprintAttack();
}

void USprintAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	bDodgeCancelable = false;
	bRuntimeActionTagsApplied = false;
	bRateWindowApplied = false;
	ActiveMontage = nullptr;
	BoundAnimInstance = nullptr;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !SprintAttackMontage || !CostGameplayEffectClass
		|| !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass || !SprintStateTag.IsValid() || !AttackingStateTag.IsValid()
		|| !MovementInputBlockedTag.IsValid() || !JumpInputBlockedTag.IsValid() || !StaminaRegenBlockedTag.IsValid()
		|| !TraceWindowBeginEventTag.IsValid() || !TraceWindowEndEventTag.IsValid()
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid()
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid()
		|| !AbilitySystemComponent->HasMatchingGameplayTag(SprintStateTag) || !PlayerCharacter->ShouldRequestSprintAttack())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': active grounded Sprint, montage, cost/damage/regen effects, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SprintAttackMontage);
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);
	if (!MontageTask || !TraceWindowBeginTask || !TraceWindowEndTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask || !RateWindowBeginTask || !RateWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Sprint attack activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SetRuntimeActionTags(true);
	PlayerCharacter->ApplyLockAwareActionFacing();

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SprintAttackMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &USprintAttackAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &USprintAttackAbility::OnActiveMontageEnded);
	TraceWindowBeginTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnTraceWindowBegin);
	TraceWindowEndTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnTraceWindowEnd);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnDodgeCancelWindowEnd);
	RateWindowBeginTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnRateWindowBegin);
	RateWindowEndTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnRateWindowEnd);

	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	RateWindowBeginTask->ReadyForActivation();
	RateWindowEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// Montage startup can synchronously invoke the bound end delegate. That path has already cleaned every task and pointer.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(SprintAttackMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
	PlayerCharacter->CancelSprintAbility();
}

void USprintAttackAbility::EndAbility(
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
	SetRuntimeActionTags(false);
	CloseTraceWindow();
	RestoreBaselineMontageRate();

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &USprintAttackAbility::OnActiveMontageEnded);
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

	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USprintAttackAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void USprintAttackAbility::OnTraceWindowBegin(FGameplayEventData Payload)
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

void USprintAttackAbility::OnTraceWindowEnd(FGameplayEventData Payload)
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

void USprintAttackAbility::OnDodgeCancelWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(true);
	}
}

void USprintAttackAbility::OnDodgeCancelWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(false);
	}
}

void USprintAttackAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

bool USprintAttackAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && ActiveMontage && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.OptionalObject.Get() == ActiveMontage.Get();
}

void USprintAttackAbility::OpenTraceWindow(const TArray<FName>& InTraceSourceNames)
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

void USprintAttackAbility::CloseTraceWindow()
{
	FMeleeTraceWindowLifecycle::CloseAndClear(TraceWindowTask, ActiveTraceNotifyState);
}

void USprintAttackAbility::OnRateWindowBegin(FGameplayEventData Payload)
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

void USprintAttackAbility::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	RestoreBaselineMontageRate();
}

void USprintAttackAbility::RestoreBaselineMontageRate()
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

void USprintAttackAbility::SetDodgeCancelable(bool bShouldBeCancelable)
{
	if (bShouldBeCancelable)
	{
		if (bEndAbilityRequested || bDodgeCancelable)
		{
			return;
		}

		if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (!DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid())
			{
				return;
			}

			CharacterASC->AddLooseGameplayTag(DodgeCancelableStateTag);
			CharacterASC->AddLooseGameplayTag(DefenseCancelableStateTag);
			bDodgeCancelable = true;
		}
		return;
	}

	if (!bDodgeCancelable)
	{
		return;
	}

	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			CharacterASC->RemoveLooseGameplayTag(DodgeCancelableStateTag);
		}
		if (DefenseCancelableStateTag.IsValid())
		{
			CharacterASC->RemoveLooseGameplayTag(DefenseCancelableStateTag);
		}
	}

	bDodgeCancelable = false;
}

void USprintAttackAbility::SetRuntimeActionTags(bool bShouldApply)
{
	if (bShouldApply)
	{
		if (bRuntimeActionTagsApplied)
		{
			return;
		}

		if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystemComponent->AddLooseGameplayTag(AttackingStateTag);
			AbilitySystemComponent->AddLooseGameplayTag(MovementInputBlockedTag);
			AbilitySystemComponent->AddLooseGameplayTag(JumpInputBlockedTag);
			AbilitySystemComponent->AddLooseGameplayTag(StaminaRegenBlockedTag);
			bRuntimeActionTagsApplied = true;
		}
		return;
	}

	if (!bRuntimeActionTagsApplied)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(AttackingStateTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(MovementInputBlockedTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(JumpInputBlockedTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(StaminaRegenBlockedTag);
	}

	bRuntimeActionTagsApplied = false;
}
