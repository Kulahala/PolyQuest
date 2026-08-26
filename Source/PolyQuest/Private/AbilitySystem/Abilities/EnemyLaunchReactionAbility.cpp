#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"

#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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

	EnemyLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false);
	EnemyLaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
	LaunchCommitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Launch.Commit")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	EnemySmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);

	AbilityTags.AddTag(EnemyLaunchReactionAbilityTag);
	ActivationOwnedTags.AddTag(HitReactingStateTag);

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
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	BoundEnemyCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !MovementComponent || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction activation aborted for '%s': ASC, living enemy, AnimInstance, valid montages, grounded movement, and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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

	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyLaunchReactionAbility::OnActiveMontageEnded);
	MontageTask->ReadyForActivation();

	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
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

	TWeakObjectPtr<AEnemyCharacter> LocalEnemyCharacter = BoundEnemyCharacter.IsValid()
		? BoundEnemyCharacter
		: (ActorInfo && ActorInfo->AvatarActor.IsValid()
			? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get())
			: Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()));

	AEnemyCharacter* EnemyCharacter = LocalEnemyCharacter.Get();

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
	constexpr float AirborneTransitionGraceSeconds = 0.10f;
}

void UEnemyLaunchReactionAbility::OnLaunchCommitEventReceived(FGameplayEventData Payload)
{
	if (bEndAbilityRequested || bCommitHandled || CurrentPhase != ELaunchPhase::Takeoff)
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

	float ResolvedFacingYaw = 0.0f;
	FVector LaunchVelocity = FVector::ZeroVector;
	if (!FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(ImpactDirectionSnapshot, ImpactReferenceYawSnapshot, LaunchHorizontalSpeed, LaunchVerticalSpeed, ResolvedFacingYaw, LaunchVelocity))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy launch reaction failed to compute launch velocity for '%s'; ending ability."), *GetNameSafe(EnemyCharacter));
		EndFromMontage(true);
		return;
	}

	bCommitHandled = true;
	CurrentPhase = ELaunchPhase::AwaitingAirborne;

	// Pause takeoff montage so it holds the airborne flight silhouette in flight
	BoundAnimInstance->Montage_Pause(TakeoffMontage.Get());

	const FRotator CurrentRotation = EnemyCharacter->GetActorRotation();
	EnemyCharacter->SetActorRotation(FRotator(CurrentRotation.Pitch, ResolvedFacingYaw, CurrentRotation.Roll));

	EnemyCharacter->LaunchCharacter(LaunchVelocity, true, true);

	if (MovementComponent->IsFalling())
	{
		CurrentPhase = ELaunchPhase::Airborne;
	}
	else
	{
		FallValidationTask = UAbilityTask_WaitDelay::WaitDelay(this, AirborneTransitionGraceSeconds);
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

	if (CurrentPhase == ELaunchPhase::Takeoff)
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

	if (CurrentPhase == ELaunchPhase::Takeoff && !bCommitHandled && Montage == TakeoffMontage.Get())
	{
		EndFromMontage(bInterrupted);
	}
	else if (CurrentPhase == ELaunchPhase::LandingRecovery && Montage == LandingRecoveryMontage.Get())
	{
		if (!bInterrupted)
		{
			bLandingRecoveryCompletedNaturally = true;
		}
		EndFromMontage(bInterrupted);
	}
}

bool UEnemyLaunchReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* MovementComponent = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;

	return CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && AnimInstance && TakeoffMontage && LandingRecoveryMontage
		&& MovementComponent && MovementComponent->IsMovingOnGround()
		&& LaunchHorizontalSpeed > 0.0f && FMath::IsFinite(LaunchHorizontalSpeed)
		&& LaunchVerticalSpeed > 0.0f && FMath::IsFinite(LaunchVerticalSpeed)
		&& EnemyLaunchReactionAbilityTag.IsValid() && EnemyLaunchReactionEventTag.IsValid() && LaunchCommitEventTag.IsValid()
		&& HitReactingStateTag.IsValid() && StunnedStateTag.IsValid() && DeadStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& EnemyMeleeAbilityTag.IsValid() && EnemySmallHitReactionAbilityTag.IsValid() && AbilitiesToCancel.Num() == 2;
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
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
