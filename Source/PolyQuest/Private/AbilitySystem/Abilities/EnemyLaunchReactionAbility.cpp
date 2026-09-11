#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"

#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Tasks/AbilityTask_TurnToFacing.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemyLaunchReactionAbility::UEnemyLaunchReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	bUseGroundedRootMotionKnockdown = true;
	RootMotionKnockdownMontage = nullptr;

	EnemyLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false);
	EnemyLaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
	LaunchCommitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Launch.Commit")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	EnemySmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);
	TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
	FacingBlockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Block.Facing")), false);

	AbilityTags.AddTag(EnemyLaunchReactionAbilityTag);
	if (TeardownOnUnpossessTag.IsValid())
	{
		AbilityTags.AddTag(TeardownOnUnpossessTag);
	}
	ActivationOwnedTags.AddTag(HitReactingStateTag);
	if (FacingBlockedStateTag.IsValid())
	{
		ActivationOwnedTags.AddTag(FacingBlockedStateTag);
	}

	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(HyperArmorStateTag);
	ActivationBlockedTags.AddTag(HitReactingStateTag);

	FAbilityTriggerData LaunchTrigger;
	LaunchTrigger.TriggerTag = EnemyLaunchReactionEventTag;
	LaunchTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(LaunchTrigger);

	AbilitiesToCancel.AddTag(EnemyMeleeAbilityTag);
	AbilitiesToCancel.AddTag(EnemySmallHitReactionAbilityTag);
}

bool UEnemyLaunchReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UEnemyLaunchReactionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;
	bCommitHandled = false;
	bLedgeSettingModified = false;
	bMovementModeDelegateBound = false;
	bLandingRecoveryCompletedNaturally = false;
	CurrentPhase = ELaunchPhase::None;
#if WITH_DEV_AUTOMATION_TESTS
	TObjectPtr<UAnimInstance> PreservedBoundAnimInstance = BoundAnimInstance;
#endif
	BoundAnimInstance = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (PreservedBoundAnimInstance)
	{
		BoundAnimInstance = PreservedBoundAnimInstance;
	}
	else if (const UEnemyLaunchReactionAbility* CDO = Cast<UEnemyLaunchReactionAbility>(GetClass()->GetDefaultObject()))
	{
		if (CDO->BoundAnimInstance)
		{
			BoundAnimInstance = CDO->BoundAnimInstance;
		}
	}
#endif
	ActiveMontage = nullptr;
	BoundEnemyCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;

#if WITH_DEV_AUTOMATION_TESTS
	if (const UEnemyLaunchReactionAbility* CDO = Cast<UEnemyLaunchReactionAbility>(GetClass()->GetDefaultObject()))
	{
		if (CDO->bTestBypassMontageActiveCheck)
		{
			bTestBypassMontageActiveCheck = true;
		}
		if (CDO->RootMotionKnockdownMontage && !RootMotionKnockdownMontage)
		{
			RootMotionKnockdownMontage = CDO->RootMotionKnockdownMontage;
		}
		if (CDO->TakeoffMontage && !TakeoffMontage)
		{
			TakeoffMontage = CDO->TakeoffMontage;
		}
		if (CDO->LandingRecoveryMontage && !LandingRecoveryMontage)
		{
			LandingRecoveryMontage = CDO->LandingRecoveryMontage;
		}
		if (CDO->BoundAnimInstance && !BoundAnimInstance)
		{
			BoundAnimInstance = CDO->BoundAnimInstance;
		}
	}
#endif

	if (IsInstantiated())
	{
		SetCurrentInfo(Handle, ActorInfo, ActivationInfo);
	}

	UAbilitySystemComponent* CharacterASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: (CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr);
	AEnemyCharacter* EnemyCharacter = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get())
		: (CurrentActorInfo ? Cast<AEnemyCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!AnimInstance && BoundAnimInstance)
	{
		AnimInstance = BoundAnimInstance.Get();
	}
	if (!AnimInstance)
	{
		if (const UEnemyLaunchReactionAbility* CDO = Cast<UEnemyLaunchReactionAbility>(GetClass()->GetDefaultObject()))
		{
			if (CDO->BoundAnimInstance)
			{
				AnimInstance = CDO->BoundAnimInstance.Get();
			}
		}
	}
#endif
	UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !MovementComponent || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': ASC, living enemy, AnimInstance, valid montages, grounded movement, and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bExecuteRootMotion = IsRootMotionKnockdownCandidate(EnemyCharacter, MovementComponent);
	if (bExecuteRootMotion)
	{
		// 1. Create Root Motion Montage Task (without CommitEventTask)
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RootMotionKnockdownMontage);
		if (!MontageTask)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': failed to create root motion knockdown montage AbilityTask."), *GetNameSafe(EnemyCharacter));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy launch reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		BoundAnimInstance = AnimInstance;
		ActiveMontage = RootMotionKnockdownMontage;
		BoundEnemyCharacter = EnemyCharacter;
		CurrentPhase = ELaunchPhase::RootMotionKnockdown;

		// 2. Stop AI navigation and current velocity
		if (AAIController* AIController = EnemyCharacter->GetController<AAIController>())
		{
			AIController->StopMovement();
		}
		MovementComponent->StopMovementImmediately();

		// 3. Cancel enemy melee and small hit reaction before starting Root Motion
		CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);
		if (bEndAbilityRequested)
		{
			return;
		}

		// 4. Fail-closed if lingering Root Motion is still active after cancellation
		if (EnemyCharacter->HasAnyRootMotion())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction root motion branch aborted for '%s': lingering root motion detected after cancelling competing abilities; fail-closed ending ability."), *GetNameSafe(EnemyCharacter));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		// 5. Resolve attacker direction and set instant facing yaw
		ImpactReferenceYawSnapshot = EnemyCharacter->GetActorRotation().Yaw;
		if (TriggerEventData)
		{
			ImpactDirectionSnapshot = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, EnemyCharacter);
		}

		float ResolvedFacingYaw = 0.0f;
		if (!TryResolveRootMotionFacingYaw(ImpactDirectionSnapshot, ImpactReferenceYawSnapshot, ResolvedFacingYaw))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction root motion branch aborted for '%s': failed to resolve valid facing yaw from impact context."), *GetNameSafe(EnemyCharacter));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		EnemyCharacter->SetActorRotation(FRotator(0.0f, ResolvedFacingYaw, 0.0f));

		// 6. Preserve and disable ledge walk-off protection before ReadyForActivation()
		bSavedCanWalkOffLedges = MovementComponent->bCanWalkOffLedges;
		MovementComponent->bCanWalkOffLedges = false;
		bLedgeSettingModified = true;

		// 7. Bind Montage End and MovementModeChanged delegates
		if (BoundAnimInstance)
		{
			BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
			BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
		}
		EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnMovementModeChanged);
		EnemyCharacter->MovementModeChangedDelegate.AddDynamic(this, &UEnemyLaunchReactionAbility::OnMovementModeChanged);
		bMovementModeDelegateBound = true;

		// 8. Start Montage Task
		MontageTask->ReadyForActivation();

		if (bEndAbilityRequested)
		{
			return;
		}

		const bool bMontageActive =
#if WITH_DEV_AUTOMATION_TESTS
			bTestBypassMontageActiveCheck ||
#endif
			(BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));

		if (!bMontageActive)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': root motion knockdown montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(RootMotionKnockdownMontage));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		// 9. Begin stance break deferral after montage is demonstrably active
		EnemyCharacter->BeginLaunchStanceBreakDeferral();
		return;
	}

	// -------------------------------------------------------------------------
	// Legacy Physics Launch Fallback Branch
	// -------------------------------------------------------------------------
	// 1. Create and activate commit event listener before starting takeoff montage
	CommitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, LaunchCommitEventTag, nullptr, false, false);
	if (!CommitEventTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': failed to create commit event task."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CommitEventTask->EventReceived.AddDynamic(this, &UEnemyLaunchReactionAbility::OnLaunchCommitEventReceived);
	CommitEventTask->ReadyForActivation();

	// 2. Create takeoff montage task
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TakeoffMontage);
	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': failed to create takeoff montage AbilityTask."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy launch reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = TakeoffMontage;
	BoundEnemyCharacter = EnemyCharacter;
	CurrentPhase = ELaunchPhase::Takeoff;

	// Capture reference yaw and impact direction snapshot before starting montage task
	ImpactReferenceYawSnapshot = EnemyCharacter->GetActorRotation().Yaw;
	if (TriggerEventData)
	{
		ImpactDirectionSnapshot = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, EnemyCharacter);
	}

	SmoothingState.Reset();
	SmoothingState.TryFreeze(ImpactDirectionSnapshot, ImpactReferenceYawSnapshot, LaunchHorizontalSpeed, LaunchVerticalSpeed);

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
		BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
	}
	MontageTask->ReadyForActivation();

	if (bEndAbilityRequested)
	{
		return;
	}

	const bool bMontageActive =
#if WITH_DEV_AUTOMATION_TESTS
		bTestBypassMontageActiveCheck ||
#endif
		(BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));

	if (!bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': takeoff montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(TakeoffMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Begin stance break deferral only after takeoff montage is demonstrably active
	EnemyCharacter->BeginLaunchStanceBreakDeferral();

	// 3. Stop AI navigation movement
	if (AAIController* AIController = EnemyCharacter->GetController<AAIController>())
	{
		AIController->StopMovement();
	}

	// 4. Stop current velocity
	MovementComponent->StopMovementImmediately();

	// 6. Bind MovementModeChangedDelegate
	EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnMovementModeChanged);
	EnemyCharacter->MovementModeChangedDelegate.AddDynamic(this, &UEnemyLaunchReactionAbility::OnMovementModeChanged);
	bMovementModeDelegateBound = true;

	// 7. Cancel enemy melee and small hit reaction after takeoff montage is confirmed active
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);

	if (bEndAbilityRequested)
	{
		return;
	}

	// 8. Start facing smoothing task after previous abilities/root motion are cancelled
	if (SmoothingState.HasFrozenLaunch())
	{
		FacingTurnTask = UAbilityTask_TurnToFacing::TurnToFacing(
			this,
			EnemyCharacter,
			SmoothingState.GetStartYaw(),
			SmoothingState.GetTargetYaw(),
			FacingTurnRateDegreesPerSecond);

		if (FacingTurnTask)
		{
			FacingTurnTask->OnTurnCompleted.AddUObject(this, &UEnemyLaunchReactionAbility::OnFacingTurnCompleted);
			FacingTurnTask->OnTurnFailed.AddUObject(this, &UEnemyLaunchReactionAbility::OnFacingTurnFailed);
			FacingTurnTask->ReadyForActivation();
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': failed to create facing turn AbilityTask."), *GetNameSafe(EnemyCharacter));
			EndFromMontage(true);
			return;
		}
	}
	else
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy launch reaction facing freeze was skipped/failed for '%s' due to malformed context."), *GetNameSafe(EnemyCharacter));
	}
}

void UEnemyLaunchReactionAbility::EndAbility(
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
	const bool bNaturalCompletion = bLandingRecoveryCompletedNaturally;
	bLandingRecoveryCompletedNaturally = false;

#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif

	TWeakObjectPtr<AEnemyCharacter> LocalEnemyCharacter = BoundEnemyCharacter.IsValid()
		? BoundEnemyCharacter
		: (ActorInfo && ActorInfo->AvatarActor.IsValid()
			? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get())
			: Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()));

	AEnemyCharacter* EnemyCharacter = LocalEnemyCharacter.Get();

	if (FacingTurnTask)
	{
		FacingTurnTask->EndTask();
		FacingTurnTask = nullptr;
	}

	SmoothingState.Reset();

	if (bMovementModeDelegateBound && EnemyCharacter)
	{
		EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnMovementModeChanged);
		bMovementModeDelegateBound = false;
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
		if (ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, ActiveMontage.Get());
		}
		if (TakeoffMontage && BoundAnimInstance->Montage_IsActive(TakeoffMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, TakeoffMontage.Get());
		}
		if (LandingRecoveryMontage && BoundAnimInstance->Montage_IsActive(LandingRecoveryMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, LandingRecoveryMontage.Get());
		}
		if (RootMotionKnockdownMontage && BoundAnimInstance->Montage_IsActive(RootMotionKnockdownMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, RootMotionKnockdownMontage.Get());
		}
		BoundAnimInstance = nullptr;
	}

	if (CommitEventTask)
	{
		CommitEventTask->EndTask();
		CommitEventTask = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (FallValidationTask)
	{
		FallValidationTask->EndTask();
		FallValidationTask = nullptr;
	}

	ActiveMontage = nullptr;

	if (bLedgeSettingModified && EnemyCharacter && !EnemyCharacter->IsActorBeingDestroyed())
	{
		if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
		{
			MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
		}
		bLedgeSettingModified = false;
	}

	BoundEnemyCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;
	CurrentPhase = ELaunchPhase::None;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (LocalEnemyCharacter.IsValid())
	{
		AEnemyCharacter* TargetEnemy = LocalEnemyCharacter.Get();
		if (TargetEnemy && !TargetEnemy->IsActorBeingDestroyed())
		{
			if (bNaturalCompletion)
			{
				TargetEnemy->CompleteLaunchStanceBreakDeferral();
			}
			else
			{
				TargetEnemy->AbortLaunchStanceBreakDeferral();
			}
		}
	}
}

namespace
{
	constexpr float EnemyAirborneTransitionGraceSeconds = 0.10f;
}

void UEnemyLaunchReactionAbility::OnLaunchCommitEventReceived(FGameplayEventData Payload)
{
	if (bEndAbilityRequested || CurrentPhase == ELaunchPhase::RootMotionKnockdown || bCommitHandled || CurrentPhase != ELaunchPhase::Takeoff)
	{
		return;
	}

	AEnemyCharacter* EnemyCharacter = BoundEnemyCharacter.IsValid() ? BoundEnemyCharacter.Get() : Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	if (!EnemyCharacter || !CharacterASC || !MovementComponent || !BoundAnimInstance || !TakeoffMontage || !IsEventFromTakeoffMontage(Payload))
	{
		return;
	}

	if (!BoundAnimInstance->Montage_IsActive(TakeoffMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction commit aborted for '%s': takeoff montage '%s' is not active; ending ability."), *GetNameSafe(EnemyCharacter), *GetNameSafe(TakeoffMontage));
		EndFromMontage(true);
		return;
	}

	if (!SmoothingState.TryReceiveCommit())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction failed to accept commit on frozen state for '%s'; ending ability."), *GetNameSafe(EnemyCharacter));
		EndFromMontage(true);
		return;
	}

	bCommitHandled = true;
	CurrentPhase = ELaunchPhase::TurningToLaunch;

	// Pause takeoff montage so it holds the airborne flight silhouette in flight
	BoundAnimInstance->Montage_Pause(TakeoffMontage.Get());

	if (SmoothingState.IsTurnCompleted())
	{
		CommitFrozenLaunch();
	}
}

void UEnemyLaunchReactionAbility::OnFacingTurnCompleted()
{
	FacingTurnTask = nullptr;
	SmoothingState.MarkTurnCompleted();

	if (SmoothingState.IsCommitReceived())
	{
		CommitFrozenLaunch();
	}
}

void UEnemyLaunchReactionAbility::OnFacingTurnFailed()
{
	FacingTurnTask = nullptr;
	UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction facing turn task failed for '%s'; ending ability."), *GetNameSafe(BoundEnemyCharacter.Get()));
	EndFromMontage(true);
}

void UEnemyLaunchReactionAbility::CommitFrozenLaunch()
{
	if (bEndAbilityRequested || CurrentPhase != ELaunchPhase::TurningToLaunch)
	{
		return;
	}

	AEnemyCharacter* EnemyCharacter = BoundEnemyCharacter.IsValid() ? BoundEnemyCharacter.Get() : Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	if (!EnemyCharacter || !CharacterASC || !MovementComponent || !BoundAnimInstance || !TakeoffMontage)
	{
		EndFromMontage(true);
		return;
	}

	if (EnemyCharacter->HasAnyRootMotion())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction commit aborted for '%s': active Root Motion detected; fail-closed ending ability."), *GetNameSafe(EnemyCharacter));
		EndFromMontage(true);
		return;
	}

	FVector LaunchVelocity = FVector::ZeroVector;
	if (!SmoothingState.TryConsumeLaunchVelocity(LaunchVelocity))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction failed to consume launch velocity for '%s'; ending ability."), *GetNameSafe(EnemyCharacter));
		EndFromMontage(true);
		return;
	}

	CurrentPhase = ELaunchPhase::AwaitingAirborne;

	EnemyCharacter->LaunchCharacter(LaunchVelocity, true, true);

	if (MovementComponent->IsFalling())
	{
		CurrentPhase = ELaunchPhase::Airborne;
	}
	else
	{
		FallValidationTask = UAbilityTask_WaitDelay::WaitDelay(this, EnemyAirborneTransitionGraceSeconds);
		if (FallValidationTask)
		{
			FallValidationTask->OnFinish.AddDynamic(this, &UEnemyLaunchReactionAbility::OnFallValidationFinished);
			FallValidationTask->ReadyForActivation();
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction failed to create falling validation task for '%s'; ending ability."), *GetNameSafe(EnemyCharacter));
			EndFromMontage(true);
			return;
		}
	}
}

void UEnemyLaunchReactionAbility::OnFallValidationFinished()
{
	if (bEndAbilityRequested || CurrentPhase == ELaunchPhase::Airborne)
	{
		return;
	}

	if (CurrentPhase == ELaunchPhase::AwaitingAirborne)
	{
		const AEnemyCharacter* EnemyCharacter = BoundEnemyCharacter.IsValid() ? BoundEnemyCharacter.Get() : Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
		const UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;
		if (EnemyCharacter && MovementComponent && MovementComponent->IsFalling())
		{
			CurrentPhase = ELaunchPhase::Airborne;
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction failed falling validation after commit for '%s'; ending ability."), *GetNameSafe(EnemyCharacter));
			EndFromMontage(true);
		}
	}
}

void UEnemyLaunchReactionAbility::OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (bEndAbilityRequested || !Character)
	{
		return;
	}

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	if (CurrentPhase == ELaunchPhase::RootMotionKnockdown)
	{
		if (MovementComponent->MovementMode != MOVE_Walking)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction root motion knockdown aborted for '%s': movement mode changed from Walking to %d; ending ability."), *GetNameSafe(Character), static_cast<int32>(MovementComponent->MovementMode));
			EndFromMontage(true);
			return;
		}
	}
	else if (CurrentPhase == ELaunchPhase::Takeoff || CurrentPhase == ELaunchPhase::TurningToLaunch)
	{
		if (MovementComponent->IsFalling())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction takeoff aborted for '%s': unexpected premature falling before commit; ending ability."), *GetNameSafe(Character));
			EndFromMontage(true);
			return;
		}
	}
	else if (CurrentPhase == ELaunchPhase::AwaitingAirborne)
	{
		if (MovementComponent->IsFalling())
		{
			CurrentPhase = ELaunchPhase::Airborne;
		}
	}
	else if (CurrentPhase == ELaunchPhase::Airborne)
	{
		if (MovementComponent->IsMovingOnGround())
		{
			CurrentPhase = ELaunchPhase::LandingRecovery;
			MovementComponent->StopMovementImmediately();

			if (FallValidationTask)
			{
				FallValidationTask->EndTask();
				FallValidationTask = nullptr;
			}

			// Stop the paused Takeoff montage before starting LandingRecovery
			if (BoundAnimInstance && TakeoffMontage)
			{
				BoundAnimInstance->Montage_Stop(0.0f, TakeoffMontage.Get());
			}

			if (MontageTask)
			{
				MontageTask->EndTask();
				MontageTask = nullptr;
			}
			ActiveMontage = nullptr;

			if (!BoundAnimInstance || !LandingRecoveryMontage)
			{
				UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction cannot play landing recovery for '%s': missing AnimInstance or LandingRecoveryMontage."), *GetNameSafe(Character));
				EndFromMontage(true);
				return;
			}

			ActiveMontage = LandingRecoveryMontage;
			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, LandingRecoveryMontage);
			if (!MontageTask)
			{
				UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction failed to create landing recovery montage task for '%s'."), *GetNameSafe(Character));
				EndFromMontage(true);
				return;
			}

			MontageTask->ReadyForActivation();

			if (bEndAbilityRequested)
			{
				return;
			}

			if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
			{
				UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction landing recovery montage '%s' did not start on '%s'."), *GetNameSafe(LandingRecoveryMontage), *GetNameSafe(Character));
				EndFromMontage(true);
				return;
			}

			bSavedCanWalkOffLedges = MovementComponent->bCanWalkOffLedges;
			MovementComponent->bCanWalkOffLedges = false;
			bLedgeSettingModified = true;
		}
	}
	else if (CurrentPhase == ELaunchPhase::LandingRecovery)
	{
		if (MovementComponent->IsFalling())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction landing recovery aborted for '%s': character fell off ledge or surface during recovery; ending ability."), *GetNameSafe(Character));
			EndFromMontage(true);
		}
	}
}

void UEnemyLaunchReactionAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested)
	{
		return;
	}

	if (Montage == TakeoffMontage.Get())
	{
		if (CurrentPhase == ELaunchPhase::Takeoff && !bCommitHandled)
		{
			EndFromMontage(bInterrupted);
			return;
		}
		if (CurrentPhase == ELaunchPhase::TurningToLaunch)
		{
			EndFromMontage(true);
			return;
		}
	}
	else if (Montage == LandingRecoveryMontage.Get())
	{
		if (CurrentPhase == ELaunchPhase::LandingRecovery)
		{
			if (!bInterrupted)
			{
				bLandingRecoveryCompletedNaturally = true;
			}
			EndFromMontage(bInterrupted);
			return;
		}
	}
	else if (Montage == RootMotionKnockdownMontage.Get())
	{
		if (CurrentPhase == ELaunchPhase::RootMotionKnockdown)
		{
			if (!bInterrupted)
			{
				bLandingRecoveryCompletedNaturally = true;
			}
			EndFromMontage(bInterrupted);
			return;
		}
	}
}

bool UEnemyLaunchReactionAbility::IsRootMotionKnockdownCandidate(
	const AEnemyCharacter* EnemyCharacter,
	const UCharacterMovementComponent* MovementComponent) const
{
	if (!bUseGroundedRootMotionKnockdown || !RootMotionKnockdownMontage || !MovementComponent)
	{
		return false;
	}

	if (!RootMotionKnockdownMontage->HasRootMotion() || RootMotionKnockdownMontage->SlotAnimTracks.Num() == 0)
	{
		return false;
	}

	const float PlayLength = RootMotionKnockdownMontage->GetPlayLength();
	if (!FMath::IsFinite(PlayLength) || PlayLength <= 0.0f)
	{
		return false;
	}

	return MovementComponent->MovementMode == MOVE_Walking;
}

bool UEnemyLaunchReactionAbility::IsLegacyLaunchCandidate(const UCharacterMovementComponent* MovementComponent) const
{
	if (!TakeoffMontage || !LandingRecoveryMontage || !MovementComponent)
	{
		return false;
	}

	return MovementComponent->IsMovingOnGround()
		&& LaunchHorizontalSpeed > 0.0f && FMath::IsFinite(LaunchHorizontalSpeed)
		&& LaunchVerticalSpeed > 0.0f && FMath::IsFinite(LaunchVerticalSpeed)
		&& FacingTurnRateDegreesPerSecond > 0.0f && FMath::IsFinite(FacingTurnRateDegreesPerSecond);
}

bool UEnemyLaunchReactionAbility::TryResolveRootMotionFacingYaw(
	const FVector& LocalAttackerDirection,
	float ImpactReferenceYaw,
	float& OutFacingYaw)
{
	OutFacingYaw = 0.0f;

	if (!FMath::IsFinite(LocalAttackerDirection.X) || !FMath::IsFinite(LocalAttackerDirection.Y))
	{
		return false;
	}

	FVector LocalPlanarDir(LocalAttackerDirection.X, LocalAttackerDirection.Y, 0.0f);
	if (LocalPlanarDir.IsNearlyZero() || !LocalPlanarDir.Normalize())
	{
		return false;
	}

	if (!FMath::IsFinite(LocalPlanarDir.X) || !FMath::IsFinite(LocalPlanarDir.Y))
	{
		return false;
	}

	if (!FMath::IsFinite(ImpactReferenceYaw))
	{
		return false;
	}

	const FRotator TargetRotation(0.0f, ImpactReferenceYaw, 0.0f);
	const FVector WorldAttackerDir = TargetRotation.RotateVector(LocalPlanarDir);
	if (!FMath::IsFinite(WorldAttackerDir.X) || !FMath::IsFinite(WorldAttackerDir.Y))
	{
		return false;
	}

	const float ResolvedFacingYaw = WorldAttackerDir.Rotation().Yaw;
	if (!FMath::IsFinite(ResolvedFacingYaw))
	{
		return false;
	}

	OutFacingYaw = ResolvedFacingYaw;
	return true;
}

bool UEnemyLaunchReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!AnimInstance && BoundAnimInstance)
	{
		AnimInstance = BoundAnimInstance.Get();
	}
	if (!AnimInstance)
	{
		if (const UEnemyLaunchReactionAbility* CDO = Cast<UEnemyLaunchReactionAbility>(GetClass()->GetDefaultObject()))
		{
			if (CDO->BoundAnimInstance)
			{
				AnimInstance = CDO->BoundAnimInstance.Get();
			}
		}
	}
#endif
	const UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	const bool bCommonValid = CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && AnimInstance && MovementComponent
		&& MovementComponent->IsMovingOnGround()
		&& EnemyLaunchReactionAbilityTag.IsValid() && EnemyLaunchReactionEventTag.IsValid() && LaunchCommitEventTag.IsValid()
		&& HitReactingStateTag.IsValid() && StunnedStateTag.IsValid() && DeadStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& EnemyMeleeAbilityTag.IsValid() && EnemySmallHitReactionAbilityTag.IsValid()
		&& TeardownOnUnpossessTag.IsValid() && FacingBlockedStateTag.IsValid()
		&& AbilitiesToCancel.Num() == 2;

	if (!bCommonValid)
	{
		return false;
	}

	return IsRootMotionKnockdownCandidate(EnemyCharacter, MovementComponent) || IsLegacyLaunchCandidate(MovementComponent);
}

bool UEnemyLaunchReactionAbility::IsEventFromTakeoffMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bEndAbilityRequested || !TakeoffMontage || !AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return false;
	}

	const UObject* PayloadObject = Payload.OptionalObject.Get();
	if (!PayloadObject)
	{
		return false;
	}

	if (PayloadObject == TakeoffMontage.Get())
	{
		return true;
	}

	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PayloadObject))
	{
		for (const FSlotAnimationTrack& Track : TakeoffMontage->SlotAnimTracks)
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

void UEnemyLaunchReactionAbility::EndFromMontage(bool bWasCancelled)
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	if (!ActorInfo && BoundEnemyCharacter.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = BoundEnemyCharacter->GetAbilitySystemComponent())
		{
			ActorInfo = ASC->AbilityActorInfo.Get();
		}
	}
	EndAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UEnemyLaunchReactionAbility::IsTestPhaseRootMotionKnockdown() const
{
	return CurrentPhase == ELaunchPhase::RootMotionKnockdown;
}

bool UEnemyLaunchReactionAbility::IsTestPhaseTakeoff() const
{
	return CurrentPhase == ELaunchPhase::Takeoff;
}

bool UEnemyLaunchReactionAbility::IsTestPhaseNone() const
{
	return CurrentPhase == ELaunchPhase::None;
}

uint8 UEnemyLaunchReactionAbility::GetTestCurrentPhaseRaw() const
{
	return static_cast<uint8>(CurrentPhase);
}
#endif
