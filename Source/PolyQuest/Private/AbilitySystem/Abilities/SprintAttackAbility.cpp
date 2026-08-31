#include "AbilitySystem/Abilities/SprintAttackAbility.h"

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
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	if (PlayerCharacter)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}
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

	UAbilityTask_PlayMontageAndWait* CreatedMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SprintAttackMontage);
	MontageTask = CreatedMontageTask;
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);
	if (!CreatedMontageTask || !TraceWindowBeginTask || !TraceWindowEndTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask || !RateWindowBeginTask || !RateWindowEndTask)
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
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': montage task is not active."), *GetNameSafe(PlayerCharacter));
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

	if (!bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(SprintAttackMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TryApplyMeleeMotionWarpTarget(PlayerCharacter);

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
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif
	SetDodgeCancelable(false);
	SetRuntimeActionTags(false);
	CloseTraceWindow();
	RestoreBaselineMontageRate();

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}
	ResetMeleeMotionWarpState();

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

void USprintAttackAbility::ResetMeleeMotionWarpState()
{
	MeleeMotionWarpSnapshot.Reset();
}

void USprintAttackAbility::TryApplyMeleeMotionWarpTarget(APlayerCharacter* PlayerCharacter)
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
