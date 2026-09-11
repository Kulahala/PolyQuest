#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"

#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemyLaunchReactionAbility::UEnemyLaunchReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	RootMotionKnockdownMontage = nullptr;

	EnemyLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false);
	EnemyLaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
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
	bLedgeSettingModified = false;
	bMovementModeDelegateBound = false;
	bRootMotionKnockdownCompletedNaturally = false;
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

	// 1. Create Root Motion Montage Task
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
	const bool bNaturalCompletion = bRootMotionKnockdownCompletedNaturally;
	bRootMotionKnockdownCompletedNaturally = false;

#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif

	TWeakObjectPtr<AEnemyCharacter> LocalEnemyCharacter = BoundEnemyCharacter.IsValid()
		? BoundEnemyCharacter
		: (ActorInfo && ActorInfo->AvatarActor.IsValid()
			? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get())
			: Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()));

	AEnemyCharacter* EnemyCharacter = LocalEnemyCharacter.Get();

	if (bMovementModeDelegateBound)
	{
		if (EnemyCharacter && !EnemyCharacter->IsActorBeingDestroyed())
		{
			EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnMovementModeChanged);
		}
		bMovementModeDelegateBound = false;
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
		if (ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, ActiveMontage.Get());
		}
		if (RootMotionKnockdownMontage && BoundAnimInstance->Montage_IsActive(RootMotionKnockdownMontage.Get()))
		{
			BoundAnimInstance->Montage_Stop(0.0f, RootMotionKnockdownMontage.Get());
		}
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	ActiveMontage = nullptr;

	if (bLedgeSettingModified)
	{
		if (EnemyCharacter && !EnemyCharacter->IsActorBeingDestroyed())
		{
			if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
			{
				MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
			}
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
}

void UEnemyLaunchReactionAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested)
	{
		return;
	}

	if (Montage == RootMotionKnockdownMontage.Get())
	{
		if (CurrentPhase == ELaunchPhase::RootMotionKnockdown)
		{
			if (!bInterrupted)
			{
				bRootMotionKnockdownCompletedNaturally = true;
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
	if (!RootMotionKnockdownMontage || !MovementComponent)
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
		&& EnemyLaunchReactionAbilityTag.IsValid() && EnemyLaunchReactionEventTag.IsValid()
		&& HitReactingStateTag.IsValid() && StunnedStateTag.IsValid() && DeadStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& EnemyMeleeAbilityTag.IsValid() && EnemySmallHitReactionAbilityTag.IsValid()
		&& TeardownOnUnpossessTag.IsValid() && FacingBlockedStateTag.IsValid()
		&& AbilitiesToCancel.Num() == 2;

	if (!bCommonValid)
	{
		return false;
	}

	return IsRootMotionKnockdownCandidate(EnemyCharacter, MovementComponent);
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

bool UEnemyLaunchReactionAbility::IsTestPhaseNone() const
{
	return CurrentPhase == ELaunchPhase::None;
}

uint8 UEnemyLaunchReactionAbility::GetTestCurrentPhaseRaw() const
{
	return static_cast<uint8>(CurrentPhase);
}
#endif
