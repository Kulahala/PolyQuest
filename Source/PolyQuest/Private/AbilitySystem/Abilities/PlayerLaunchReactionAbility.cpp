#include "AbilitySystem/Abilities/PlayerLaunchReactionAbility.h"

#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Tasks/AbilityTask_TurnToFacing.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UPlayerLaunchReactionAbility::UPlayerLaunchReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	PlayerLaunchReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Launch")), false);
	PlayerLaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Launch")), false);
	LaunchCommitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Launch.Commit")), false);
	CancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	CancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);

	AbilityTags.AddTag(PlayerLaunchReactionAbilityTag);
	ActivationOwnedTags.AddTag(HitReactingStateTag);
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));

	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(HyperArmorStateTag);
	ActivationBlockedTags.AddTag(HitReactingStateTag);

	FAbilityTriggerData LaunchTrigger;
	LaunchTrigger.TriggerTag = PlayerLaunchReactionEventTag;
	LaunchTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(LaunchTrigger);

	const TArray<FName> TargetActionTagNames = {
		TEXT("Ability.Attack.Primary"),
		TEXT("Ability.Attack.Light"),
		TEXT("Ability.Attack.Charged"),
		TEXT("Ability.Attack.Sprint"),
		TEXT("Ability.Skill.Melee"),
		TEXT("Ability.Dodge"),
		TEXT("Ability.Movement.Sprint"),
		TEXT("Ability.Movement.Jump"),
		TEXT("Ability.Defense.Guard"),
		TEXT("Ability.Defense.Parry"),
		TEXT("Ability.Reaction.Player.Small")
	};

	for (const FName& TagName : TargetActionTagNames)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (TagName != TEXT("Ability.Dodge"))
		{
			BlockAbilitiesWithTag.AddTag(Tag);
		}
		AbilitiesToCancel.AddTag(Tag);
	}
}

bool UPlayerLaunchReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UPlayerLaunchReactionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;
	bCommitHandled = false;
	bDodgeCancelable = false;
	bLedgeSettingModified = false;
	bMovementModeDelegateBound = false;
	CurrentPhase = ELaunchPhase::None;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	BoundPlayerCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !MovementComponent || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': ASC, living player, AnimInstance, valid montages, grounded movement, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. Create and activate commit and cancel event listeners before starting takeoff montage
	CommitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, LaunchCommitEventTag, nullptr, false, false);
	CancelBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, CancelWindowBeginEventTag, nullptr, false, true);
	CancelEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, CancelWindowEndEventTag, nullptr, false, true);

	if (!CommitEventTask || !CancelBeginTask || !CancelEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': failed to create event tasks."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CommitEventTask->EventReceived.AddDynamic(this, &UPlayerLaunchReactionAbility::OnLaunchCommitEventReceived);
	CancelBeginTask->EventReceived.AddDynamic(this, &UPlayerLaunchReactionAbility::OnCancelWindowBegin);
	CancelEndTask->EventReceived.AddDynamic(this, &UPlayerLaunchReactionAbility::OnCancelWindowEnd);

	CommitEventTask->ReadyForActivation();
	CancelBeginTask->ReadyForActivation();
	CancelEndTask->ReadyForActivation();

	// 2. Create takeoff montage task
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TakeoffMontage);
	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': failed to create takeoff montage AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player launch reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = TakeoffMontage;
	BoundPlayerCharacter = PlayerCharacter;
	CurrentPhase = ELaunchPhase::Takeoff;

	// Capture reference yaw and impact direction snapshot before starting montage task
	ImpactReferenceYawSnapshot = PlayerCharacter->GetActorRotation().Yaw;
	if (TriggerEventData)
	{
		ImpactDirectionSnapshot = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, PlayerCharacter);
	}

	SmoothingState.Reset();
	SmoothingState.TryFreeze(ImpactDirectionSnapshot, ImpactReferenceYawSnapshot, LaunchHorizontalSpeed, LaunchVerticalSpeed);

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnActiveMontageEnded);
		BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerLaunchReactionAbility::OnActiveMontageEnded);
	}
	MontageTask->ReadyForActivation();

	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': takeoff montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(TakeoffMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Stop current velocity
	MovementComponent->StopMovementImmediately();

	// 5. Bind MovementModeChangedDelegate
	PlayerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnMovementModeChanged);
	PlayerCharacter->MovementModeChangedDelegate.AddDynamic(this, &UPlayerLaunchReactionAbility::OnMovementModeChanged);
	bMovementModeDelegateBound = true;

	// 6. Cancel pre-existing combat actions after takeoff montage is confirmed active
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);

	if (bEndAbilityRequested)
	{
		return;
	}

	// 7. Start facing smoothing task after previous abilities/root motion are cancelled
	if (SmoothingState.HasFrozenLaunch())
	{
		FacingTurnTask = UAbilityTask_TurnToFacing::TurnToFacing(
			this,
			PlayerCharacter,
			SmoothingState.GetStartYaw(),
			SmoothingState.GetTargetYaw(),
			FacingTurnRateDegreesPerSecond);

		if (FacingTurnTask)
		{
			FacingTurnTask->OnTurnCompleted.AddUObject(this, &UPlayerLaunchReactionAbility::OnFacingTurnCompleted);
			FacingTurnTask->OnTurnFailed.AddUObject(this, &UPlayerLaunchReactionAbility::OnFacingTurnFailed);
			FacingTurnTask->ReadyForActivation();
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': failed to create facing turn AbilityTask."), *GetNameSafe(PlayerCharacter));
			EndFromMontage(true);
			return;
		}
	}
	else
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player launch reaction facing freeze was skipped/failed for '%s' due to malformed context."), *GetNameSafe(PlayerCharacter));
	}
}

void UPlayerLaunchReactionAbility::EndAbility(
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
	APlayerCharacter* PlayerCharacter = BoundPlayerCharacter.IsValid() ? BoundPlayerCharacter.Get() : Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());

	if (FacingTurnTask)
	{
		FacingTurnTask->EndTask();
		FacingTurnTask = nullptr;
	}

	SmoothingState.Reset();

	if (bMovementModeDelegateBound && PlayerCharacter)
	{
		PlayerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnMovementModeChanged);
		bMovementModeDelegateBound = false;
	}

	SetDodgeCancelable(false);

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnActiveMontageEnded);
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

	if (CancelBeginTask)
	{
		CancelBeginTask->EndTask();
		CancelBeginTask = nullptr;
	}

	if (CancelEndTask)
	{
		CancelEndTask->EndTask();
		CancelEndTask = nullptr;
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

	if (bLedgeSettingModified && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed())
	{
		if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
		{
			MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
		}
		bLedgeSettingModified = false;
	}

	BoundPlayerCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;
	CurrentPhase = ELaunchPhase::None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

namespace
{
	constexpr float AirborneTransitionGraceSeconds = 0.10f;
}

void UPlayerLaunchReactionAbility::OnLaunchCommitEventReceived(FGameplayEventData Payload)
{
	if (bEndAbilityRequested || bCommitHandled || CurrentPhase != ELaunchPhase::Takeoff)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = BoundPlayerCharacter.IsValid() ? BoundPlayerCharacter.Get() : Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	if (!PlayerCharacter || !CharacterASC || !MovementComponent || !BoundAnimInstance || !TakeoffMontage || !IsEventFromTakeoffMontage(Payload))
	{
		return;
	}

	if (!BoundAnimInstance->Montage_IsActive(TakeoffMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction commit aborted for '%s': takeoff montage '%s' is not active; ending ability."), *GetNameSafe(PlayerCharacter), *GetNameSafe(TakeoffMontage));
		EndFromMontage(true);
		return;
	}

	if (!SmoothingState.TryReceiveCommit())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction failed to accept commit on frozen state for '%s'; ending ability."), *GetNameSafe(PlayerCharacter));
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

void UPlayerLaunchReactionAbility::OnFacingTurnCompleted()
{
	FacingTurnTask = nullptr;
	SmoothingState.MarkTurnCompleted();

	if (SmoothingState.IsCommitReceived())
	{
		CommitFrozenLaunch();
	}
}

void UPlayerLaunchReactionAbility::OnFacingTurnFailed()
{
	FacingTurnTask = nullptr;
	UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction facing turn task failed for '%s'; ending ability."), *GetNameSafe(BoundPlayerCharacter.Get()));
	EndFromMontage(true);
}

void UPlayerLaunchReactionAbility::CommitFrozenLaunch()
{
	if (bEndAbilityRequested || CurrentPhase != ELaunchPhase::TurningToLaunch)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = BoundPlayerCharacter.IsValid() ? BoundPlayerCharacter.Get() : Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	if (!PlayerCharacter || !CharacterASC || !MovementComponent || !BoundAnimInstance || !TakeoffMontage)
	{
		EndFromMontage(true);
		return;
	}

	if (PlayerCharacter->HasAnyRootMotion())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction commit aborted for '%s': active Root Motion detected; fail-closed ending ability."), *GetNameSafe(PlayerCharacter));
		EndFromMontage(true);
		return;
	}

	FVector LaunchVelocity = FVector::ZeroVector;
	if (!SmoothingState.TryConsumeLaunchVelocity(LaunchVelocity))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction failed to consume launch velocity for '%s'; ending ability."), *GetNameSafe(PlayerCharacter));
		EndFromMontage(true);
		return;
	}

	CurrentPhase = ELaunchPhase::AwaitingAirborne;

	PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);

	if (MovementComponent->IsFalling())
	{
		CurrentPhase = ELaunchPhase::Airborne;
	}
	else
	{
		FallValidationTask = UAbilityTask_WaitDelay::WaitDelay(this, AirborneTransitionGraceSeconds);
		if (FallValidationTask)
		{
			FallValidationTask->OnFinish.AddDynamic(this, &UPlayerLaunchReactionAbility::OnFallValidationFinished);
			FallValidationTask->ReadyForActivation();
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction failed to create falling validation task for '%s'; ending ability."), *GetNameSafe(PlayerCharacter));
			EndFromMontage(true);
			return;
		}
	}
}

void UPlayerLaunchReactionAbility::OnFallValidationFinished()
{
	if (bEndAbilityRequested || CurrentPhase == ELaunchPhase::Airborne)
	{
		return;
	}

	if (CurrentPhase == ELaunchPhase::AwaitingAirborne)
	{
		const APlayerCharacter* PlayerCharacter = BoundPlayerCharacter.IsValid() ? BoundPlayerCharacter.Get() : Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
		const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
		if (PlayerCharacter && MovementComponent && MovementComponent->IsFalling())
		{
			CurrentPhase = ELaunchPhase::Airborne;
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction failed falling validation after commit for '%s'; ending ability."), *GetNameSafe(PlayerCharacter));
			EndFromMontage(true);
		}
	}
}

void UPlayerLaunchReactionAbility::OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
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

	if (CurrentPhase == ELaunchPhase::Takeoff || CurrentPhase == ELaunchPhase::TurningToLaunch)
	{
		if (MovementComponent->IsFalling())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction aborted for '%s': unexpected premature falling before launch; ending ability."), *GetNameSafe(Character));
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
				UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction cannot play landing recovery for '%s': missing AnimInstance or LandingRecoveryMontage."), *GetNameSafe(Character));
				EndFromMontage(true);
				return;
			}

			ActiveMontage = LandingRecoveryMontage;
			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, LandingRecoveryMontage);
			if (!MontageTask)
			{
				UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction failed to create landing recovery montage task for '%s'."), *GetNameSafe(Character));
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
				UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction landing recovery montage '%s' did not start on '%s'."), *GetNameSafe(LandingRecoveryMontage), *GetNameSafe(Character));
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
			UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction landing recovery aborted for '%s': character fell off ledge or surface during recovery; ending ability."), *GetNameSafe(Character));
			EndFromMontage(true);
		}
	}
}

void UPlayerLaunchReactionAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
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
			EndFromMontage(bInterrupted);
			return;
		}
	}
}

bool UPlayerLaunchReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	const bool bIsDead = CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);

	return CharacterASC && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed() && !bIsDead && AnimInstance && TakeoffMontage && LandingRecoveryMontage
		&& MovementComponent && MovementComponent->IsMovingOnGround()
		&& LaunchHorizontalSpeed > 0.0f && FMath::IsFinite(LaunchHorizontalSpeed)
		&& LaunchVerticalSpeed > 0.0f && FMath::IsFinite(LaunchVerticalSpeed)
		&& FacingTurnRateDegreesPerSecond > 0.0f && FMath::IsFinite(FacingTurnRateDegreesPerSecond)
		&& PlayerLaunchReactionAbilityTag.IsValid() && PlayerLaunchReactionEventTag.IsValid() && LaunchCommitEventTag.IsValid()
		&& CancelWindowBeginEventTag.IsValid() && CancelWindowEndEventTag.IsValid() && DodgeCancelableStateTag.IsValid()
		&& HitReactingStateTag.IsValid() && StunnedStateTag.IsValid() && DeadStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& BlockAbilitiesWithTag.Num() == 10 && AbilitiesToCancel.Num() == 11;
}

bool UPlayerLaunchReactionAbility::IsEventFromTakeoffMontage(const FGameplayEventData& Payload) const
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

bool UPlayerLaunchReactionAbility::IsEventFromLandingRecoveryMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bEndAbilityRequested || !LandingRecoveryMontage || !AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestBypassAnimInstanceActiveCheck)
	{
		// Bypass AnimInstance active check for isolated automation test cases
	}
	else
#endif
	{
		if (!BoundAnimInstance || !BoundAnimInstance->Montage_IsActive(LandingRecoveryMontage.Get()))
		{
			return false;
		}
	}

	const UObject* PayloadObject = Payload.OptionalObject.Get();
	if (!PayloadObject)
	{
		return false;
	}

	if (PayloadObject == LandingRecoveryMontage.Get())
	{
		return true;
	}

	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PayloadObject))
	{
		for (const FSlotAnimationTrack& Track : LandingRecoveryMontage->SlotAnimTracks)
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

void UPlayerLaunchReactionAbility::OnCancelWindowBegin(FGameplayEventData Payload)
{
	if (bEndAbilityRequested || CurrentPhase != ELaunchPhase::LandingRecovery || !IsEventFromLandingRecoveryMontage(Payload))
	{
		return;
	}

	SetDodgeCancelable(true);
}

void UPlayerLaunchReactionAbility::OnCancelWindowEnd(FGameplayEventData Payload)
{
	if (bEndAbilityRequested || CurrentPhase != ELaunchPhase::LandingRecovery || !IsEventFromLandingRecoveryMontage(Payload))
	{
		return;
	}

	SetDodgeCancelable(false);
}

void UPlayerLaunchReactionAbility::SetDodgeCancelable(bool bShouldCancel)
{
	if (bDodgeCancelable == bShouldCancel)
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	if (!CharacterASC)
	{
		return;
	}

	bDodgeCancelable = bShouldCancel;
	if (bDodgeCancelable)
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			CharacterASC->AddLooseGameplayTag(DodgeCancelableStateTag);
		}
	}
	else
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			CharacterASC->RemoveLooseGameplayTag(DodgeCancelableStateTag);
		}
	}
}

void UPlayerLaunchReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
