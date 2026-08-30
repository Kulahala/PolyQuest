#include "AbilitySystem/Abilities/PlayerMeleeSkillAbility.h"

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
#include "Combat/Melee/MeleeMotionWarping.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

UPlayerMeleeSkillAbility::UPlayerMeleeSkillAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// The general melee-skill category tag used by melee action cancellation (Dodge/Guard/Parry/GuardBreak);
	// concrete skill identity tags and Cooldown GE Granted tags are authored per Gameplay Ability asset.
	MeleeSkillAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Skill.Melee")), false);
	AbilityTags.AddTag(MeleeSkillAbilityTag);

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

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

void UPlayerMeleeSkillAbility::PostLoad()
{
	Super::PostLoad();
	EnsureMeleeSkillCategoryTag();
}

#if WITH_EDITOR
void UPlayerMeleeSkillAbility::PostCDOCompiled(const FPostCDOCompiledContext& Context)
{
	Super::PostCDOCompiled(Context);
	EnsureMeleeSkillCategoryTag();
}
#endif

void UPlayerMeleeSkillAbility::EnsureMeleeSkillCategoryTag()
{
	// Blueprint defaults can replace the inherited tag container. This category
	// tag is native lifecycle identity, while concrete skill tags stay authored.
	if (MeleeSkillAbilityTag.IsValid())
	{
		AbilityTags.AddTag(MeleeSkillAbilityTag);
	}
}

bool UPlayerMeleeSkillAbility::CanActivateAbility(
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
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	return AbilitySystemComponent && PlayerCharacter && MovementComponent && MovementComponent->IsMovingOnGround()
		&& SkillMontage && CostGameplayEffectClass && CooldownGameplayEffectClass && DamageGameplayEffectClass && StaminaRegenDelayGameplayEffectClass
		&& MeleeSkillAbilityTag.IsValid();
}

void UPlayerMeleeSkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif
	bEndAbilityRequested = false;
	bDodgeCancelable = false;
	bRuntimeActionTagsApplied = false;
	bRateWindowApplied = false;
	ActiveMontage = nullptr;
	BoundAnimInstance = nullptr;
	ResetMeleeMotionWarpState();

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed() || !PlayerCharacter->GetWorld() || !AbilitySystemComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->ClearMeleeMotionWarpTargets();

	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter->GetMesh();
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement();
	if (!IsValid(AnimInstance) || !IsValid(MovementComponent) || !MovementComponent->IsMovingOnGround()
		|| !IsValid(SkillMontage) || !CostGameplayEffectClass || !CooldownGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass
		|| !MeleeSkillAbilityTag.IsValid()
		|| !AttackingStateTag.IsValid() || !MovementInputBlockedTag.IsValid() || !JumpInputBlockedTag.IsValid() || !StaminaRegenBlockedTag.IsValid()
		|| !TraceWindowBeginEventTag.IsValid() || !TraceWindowEndEventTag.IsValid()
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid()
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Melee skill activation aborted for '%s': grounded state, montage, cost/cooldown/damage/regen effects, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* CreatedMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SkillMontage);
	MontageTask = CreatedMontageTask;
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);
	if (!IsValid(CreatedMontageTask) || !IsValid(TraceWindowBeginTask) || !IsValid(TraceWindowEndTask) || !IsValid(DodgeCancelWindowBeginTask) || !IsValid(DodgeCancelWindowEndTask) || !IsValid(RateWindowBeginTask) || !IsValid(RateWindowEndTask))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Melee skill activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SkillMontage;
	if (IsValid(BoundAnimInstance))
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerMeleeSkillAbility::OnActiveMontageEnded);
		BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerMeleeSkillAbility::OnActiveMontageEnded);
	}
	if (IsValid(TraceWindowBeginTask))
	{
		TraceWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnTraceWindowBegin);
	}
	if (IsValid(TraceWindowEndTask))
	{
		TraceWindowEndTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnTraceWindowEnd);
	}
	if (IsValid(DodgeCancelWindowBeginTask))
	{
		DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnDodgeCancelWindowBegin);
	}
	if (IsValid(DodgeCancelWindowEndTask))
	{
		DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnDodgeCancelWindowEnd);
	}
	if (IsValid(RateWindowBeginTask))
	{
		RateWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnRateWindowBegin);
	}
	if (IsValid(RateWindowEndTask))
	{
		RateWindowEndTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnRateWindowEnd);
	}

	// Only the montage task starts before the commit: it must be playing so its
	// identity can be confirmed. The six notify-window tasks stay un-armed, so a
	// failed commit leaves no trace, no cancel tag, and no rate window behind.
	CreatedMontageTask->ReadyForActivation();

	// Montage startup can synchronously invoke the bound end delegate. That path has already cleaned every task and pointer.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed() || !PlayerCharacter->GetWorld()
		|| !IsValid(BoundAnimInstance) || !IsValid(ActiveMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (MontageTask.Get() != CreatedMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bTaskActive = CreatedMontageTask && IsValid(CreatedMontageTask) && CreatedMontageTask->IsActive() && !CreatedMontageTask->IsFinished();
	if (!bTaskActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Melee skill activation aborted for '%s': montage task is not active."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageActive = IsValid(BoundAnimInstance) && IsValid(ActiveMontage)
		&& (bTestBypassMontageActiveCheck || BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));
#else
	const bool bMontageActive = IsValid(BoundAnimInstance) && IsValid(ActiveMontage)
		&& BoundAnimInstance->Montage_IsActive(ActiveMontage.Get());
#endif

	// The single commit runs only after the tracked montage identity is confirmed
	// active: before this point nothing is committed and no side effect exists.
	if (!bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Melee skill activation aborted for '%s': montage '%s' did not start; no cost and no cooldown were committed."), *GetNameSafe(PlayerCharacter), *GetNameSafe(SkillMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Zero-side-effect failure: no action tag, no Guard cancel, no trace window;
		// the converged EndAbility stops the confirmed montage.
		UE_LOG(LogPolyQuest, Verbose, TEXT("Melee skill activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bEndAbilityRequested)
	{
		return;
	}

	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed() || !PlayerCharacter->GetWorld()
		|| !IsValid(BoundAnimInstance) || !IsValid(ActiveMontage) || MontageTask.Get() != CreatedMontageTask
		|| !IsValid(CreatedMontageTask) || !CreatedMontageTask->IsActive() || CreatedMontageTask->IsFinished())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SetRuntimeActionTags(true);
	PlayerCharacter->ApplyLockAwareActionFacing();

	TryApplyMeleeMotionWarpTarget(PlayerCharacter);

	auto SafeActivateNotifyTask = [this, Handle, ActorInfo, ActivationInfo](UAbilityTask_WaitGameplayEvent* Task) -> bool
	{
		if (bEndAbilityRequested)
		{
			return false;
		}

		if (!IsValid(Task))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return false;
		}

		Task->ReadyForActivation();

		if (bEndAbilityRequested)
		{
			return false;
		}

		if (!IsValid(Task))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return false;
		}

		return true;
	};

	if (!SafeActivateNotifyTask(TraceWindowBeginTask.Get())
		|| !SafeActivateNotifyTask(TraceWindowEndTask.Get())
		|| !SafeActivateNotifyTask(DodgeCancelWindowBeginTask.Get())
		|| !SafeActivateNotifyTask(DodgeCancelWindowEndTask.Get())
		|| !SafeActivateNotifyTask(RateWindowBeginTask.Get())
		|| !SafeActivateNotifyTask(RateWindowEndTask.Get()))
	{
		return;
	}

	if (bEndAbilityRequested)
	{
		return;
	}

	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed() || !PlayerCharacter->GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
}

void UPlayerMeleeSkillAbility::EndAbility(
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
	SetDodgeCancelable(false);
	SetRuntimeActionTags(false);
	CloseTraceWindow();
	RestoreBaselineMontageRate();

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (IsValid(PlayerCharacter) && !PlayerCharacter->IsActorBeingDestroyed())
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
		}
	}
	ResetMeleeMotionWarpState();

	if (IsValid(BoundAnimInstance))
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerMeleeSkillAbility::OnActiveMontageEnded);
		if (IsValid(ActiveMontage) && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, ActiveMontage.Get());
		}
	}
	BoundAnimInstance = nullptr;

	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
	}
	MontageTask = nullptr;

	if (IsValid(TraceWindowBeginTask))
	{
		TraceWindowBeginTask->EndTask();
	}
	TraceWindowBeginTask = nullptr;

	if (IsValid(TraceWindowEndTask))
	{
		TraceWindowEndTask->EndTask();
	}
	TraceWindowEndTask = nullptr;

	if (IsValid(DodgeCancelWindowBeginTask))
	{
		DodgeCancelWindowBeginTask->EndTask();
	}
	DodgeCancelWindowBeginTask = nullptr;

	if (IsValid(DodgeCancelWindowEndTask))
	{
		DodgeCancelWindowEndTask->EndTask();
	}
	DodgeCancelWindowEndTask = nullptr;

	if (IsValid(RateWindowBeginTask))
	{
		RateWindowBeginTask->EndTask();
	}
	RateWindowBeginTask = nullptr;

	if (IsValid(RateWindowEndTask))
	{
		RateWindowEndTask->EndTask();
	}
	RateWindowEndTask = nullptr;

	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerMeleeSkillAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || !IsValid(Montage) || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void UPlayerMeleeSkillAbility::OnTraceWindowBegin(FGameplayEventData Payload)
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

void UPlayerMeleeSkillAbility::OnTraceWindowEnd(FGameplayEventData Payload)
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

void UPlayerMeleeSkillAbility::OnDodgeCancelWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(true);
	}
}

void UPlayerMeleeSkillAbility::OnDodgeCancelWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(false);
	}
}

void UPlayerMeleeSkillAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

bool UPlayerMeleeSkillAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && ActiveMontage && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.OptionalObject.Get() == ActiveMontage.Get();
}

void UPlayerMeleeSkillAbility::OpenTraceWindow(const TArray<FName>& InTraceSourceNames)
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

void UPlayerMeleeSkillAbility::CloseTraceWindow()
{
	FMeleeTraceWindowLifecycle::CloseAndClear(TraceWindowTask, ActiveTraceNotifyState);
}

void UPlayerMeleeSkillAbility::OnRateWindowBegin(FGameplayEventData Payload)
{
	// Ignore duplicate Begin events; authored rate windows must not overlap.
	if (bRateWindowApplied || !IsGameplayEventFromActiveMontage(Payload) || Payload.EventMagnitude <= 0.0f)
	{
		return;
	}

	if (IsValid(BoundAnimInstance) && IsValid(ActiveMontage) && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveMontage.Get(), Payload.EventMagnitude);
		bRateWindowApplied = true;
	}
}

void UPlayerMeleeSkillAbility::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	RestoreBaselineMontageRate();
}

void UPlayerMeleeSkillAbility::RestoreBaselineMontageRate()
{
	if (!bRateWindowApplied)
	{
		return;
	}

	bRateWindowApplied = false;
	if (IsValid(BoundAnimInstance) && IsValid(ActiveMontage) && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		BoundAnimInstance->Montage_SetPlayRate(ActiveMontage.Get(), 1.0f);
	}
}

void UPlayerMeleeSkillAbility::SetDodgeCancelable(bool bShouldBeCancelable)
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

void UPlayerMeleeSkillAbility::SetRuntimeActionTags(bool bShouldApply)
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

void UPlayerMeleeSkillAbility::ResetMeleeMotionWarpState()
{
	MeleeMotionWarpSnapshot.Reset();
}

void UPlayerMeleeSkillAbility::TryApplyMeleeMotionWarpTarget(APlayerCharacter* PlayerCharacter)
{
	FMeleeMotionWarpConfig WarpConfig;
	WarpConfig.bUseMotionWarping = bUseMotionWarping;
	WarpConfig.WarpTargetName = WarpTargetName;
	WarpConfig.WarpStopDistance = WarpStopDistance;
	WarpConfig.MaxWarpDistance = MaxWarpDistance;
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
					Dist2D, WarpConfig.WarpStopDistance, WarpConfig.WarpStopDistance + WarpConfig.MaxWarpDistance,
					bPlayerOnGround ? 1 : 0, MeleeMotionWarpSnapshot.bCapturedTargetOnGround ? 1 : 0));
		}
#endif
	}
}
