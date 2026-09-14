#include "AbilitySystem/Abilities/PlayerLaunchReactionAbility.h"

#include "Abilities/GameplayAbilityTriggerType.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
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
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	AbilityTags.AddTag(PlayerLaunchReactionAbilityTag);
	AbilityTags.AddTag(TeardownOnUnpossessTag);
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
		TEXT("Ability.Action.CancelableBy.Reaction"),
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
	bSavedCanWalkOffLedges = true;
	bLedgeSettingModified = false;
	bMovementModeDelegateBound = false;
	CurrentPhase = ELaunchPhase::None;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;
	ActiveMontageInstanceID = INDEX_NONE;
	BoundPlayerCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;

	if (IsInstantiated())
	{
		SetCurrentInfo(Handle, ActorInfo, ActivationInfo);
	}

	UAbilitySystemComponent* CharacterASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: (CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr);
	APlayerCharacter* PlayerCharacter = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get())
		: (CurrentActorInfo ? Cast<APlayerCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !MovementComponent || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': ASC, living player, AnimInstance, valid montages, grounded movement, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. Create the standard Task owning RateWindow and DodgeOnly windows
	UAbilityTask_PlayActionMontage* CreatedMontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this, NAME_None, RootMotionKnockdownMontage, 1.0f, NAME_None,
		1.0f, // AnimRootMotionTranslationScale
		0.0f, // StartTimeSeconds
		true, // bAllowInterruptAfterBlendOut
		EActionMontageCancelPolicy::DodgeOnly); // CancelPolicy
	MontageTask = CreatedMontageTask;

	if (!MontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': failed to create root motion tasks."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bCommitSucceeded = CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask) return;
	if (!bCommitSucceeded)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player launch reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = RootMotionKnockdownMontage;
	BoundPlayerCharacter = PlayerCharacter;
	CurrentPhase = ELaunchPhase::RootMotionKnockdown;

	// 2. Cancel competing player abilities before starting Root Motion
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);
	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask)
	{
		return;
	}

	// 3. Fail-closed if lingering Root Motion is still active after cancellation
	if (PlayerCharacter->HasAnyRootMotion())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction root motion branch aborted for '%s': lingering root motion detected after cancelling competing abilities; fail-closed ending ability."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 4. Clear residual velocity before locking facing and launching montage
	MovementComponent->StopMovementImmediately();

	// 5. Resolve attacker direction and set instant facing yaw
	ImpactReferenceYawSnapshot = PlayerCharacter->GetActorRotation().Yaw;
	if (TriggerEventData)
	{
		ImpactDirectionSnapshot = FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, PlayerCharacter);
	}

	float ResolvedFacingYaw = 0.0f;
	if (!TryResolveRootMotionFacingYaw(ImpactDirectionSnapshot, ImpactReferenceYawSnapshot, ResolvedFacingYaw))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction root motion branch aborted for '%s': failed to resolve valid facing yaw from impact context."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	PlayerCharacter->SetActorRotation(FRotator(0.0f, ResolvedFacingYaw, 0.0f));

	// 6. Preserve and disable ledge walk-off protection before ReadyForActivation()
	bSavedCanWalkOffLedges = MovementComponent->bCanWalkOffLedges;
	MovementComponent->bCanWalkOffLedges = false;
	bLedgeSettingModified = true;

	// 7. Bind Montage End and MovementModeChanged delegates
	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnActiveMontageEnded);
		BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerLaunchReactionAbility::OnActiveMontageEnded);
	}
	PlayerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnMovementModeChanged);
	PlayerCharacter->MovementModeChangedDelegate.AddDynamic(this, &UPlayerLaunchReactionAbility::OnMovementModeChanged);
	bMovementModeDelegateBound = true;

	// 8. Start Montage Task
	CreatedMontageTask->OnFailed.AddDynamic(this, &UPlayerLaunchReactionAbility::OnMontageFailed);
	CreatedMontageTask->ReadyForActivation();

	if (!IsActive() || bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask)
	{
		return;
	}

	const bool bMontageActive =
		(BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));

	if (!bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction activation aborted for '%s': root motion knockdown montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(RootMotionKnockdownMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FAnimMontageInstance* StartedInstance = BoundAnimInstance->GetActiveInstanceForMontage(ActiveMontage);
	ActiveMontageInstanceID = StartedInstance ? StartedInstance->GetInstanceID() : INDEX_NONE;
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

	TWeakObjectPtr<APlayerCharacter> LocalPlayerCharacter = BoundPlayerCharacter.IsValid()
		? BoundPlayerCharacter
		: (ActorInfo && ActorInfo->AvatarActor.IsValid()
			? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get())
			: Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()));

	APlayerCharacter* PlayerCharacter = LocalPlayerCharacter.Get();

	if (bMovementModeDelegateBound)
	{
		if (PlayerCharacter)
		{
			PlayerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnMovementModeChanged);
		}
		bMovementModeDelegateBound = false;
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerLaunchReactionAbility::OnActiveMontageEnded);
		BoundAnimInstance = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->OnFailed.RemoveAll(this);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	ActiveMontage = nullptr;
	ActiveMontageInstanceID = INDEX_NONE;

	if (bLedgeSettingModified)
	{
		if (PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed())
		{
			if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
			{
				MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
			}
		}
		bLedgeSettingModified = false;
	}

	BoundPlayerCharacter.Reset();
	ImpactDirectionSnapshot = FVector::ZeroVector;
	ImpactReferenceYawSnapshot = 0.0f;
	CurrentPhase = ELaunchPhase::None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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

	if (CurrentPhase == ELaunchPhase::RootMotionKnockdown)
	{
		if (MovementComponent->MovementMode != MOVE_Walking)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player launch reaction root motion knockdown aborted for '%s': movement mode changed from Walking to %d; ending ability."), *GetNameSafe(Character), static_cast<int32>(MovementComponent->MovementMode));
			EndFromMontage(true);
			return;
		}
	}
}

void UPlayerLaunchReactionAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!IsActive() || bEndAbilityRequested || Montage != ActiveMontage.Get() || ActiveMontageInstanceID == INDEX_NONE) return;
	const FAnimMontageInstance* Instance = IsValid(BoundAnimInstance)
		? BoundAnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID) : nullptr;
	if (Instance && Instance->IsValid()) return;
	EndFromMontage(bInterrupted);
}

void UPlayerLaunchReactionAbility::OnMontageFailed()
{
	if (IsActive() && !bEndAbilityRequested) EndFromMontage(true);
}

bool UPlayerLaunchReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	const bool bIsDead = CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);

	const bool bCommonValid = CharacterASC && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed() && !bIsDead && AnimInstance
		&& MovementComponent && MovementComponent->IsMovingOnGround()
		&& PlayerLaunchReactionAbilityTag.IsValid() && PlayerLaunchReactionEventTag.IsValid()
		&& HitReactingStateTag.IsValid() && StunnedStateTag.IsValid() && DeadStateTag.IsValid() && HyperArmorStateTag.IsValid()
		&& TeardownOnUnpossessTag.IsValid()
		&& BlockAbilitiesWithTag.Num() == 10 && AbilitiesToCancel.Num() == 11;

	if (!bCommonValid)
	{
		return false;
	}

	return IsRootMotionKnockdownCandidate(PlayerCharacter, MovementComponent);
}

bool UPlayerLaunchReactionAbility::IsRootMotionKnockdownCandidate(
	const APlayerCharacter* PlayerCharacter,
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

bool UPlayerLaunchReactionAbility::TryResolveRootMotionFacingYaw(
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

void UPlayerLaunchReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPlayerLaunchReactionAbility::IsTestPhaseRootMotionKnockdown() const
{
	return CurrentPhase == ELaunchPhase::RootMotionKnockdown;
}

bool UPlayerLaunchReactionAbility::IsTestPhaseNone() const
{
	return CurrentPhase == ELaunchPhase::None;
}

uint8 UPlayerLaunchReactionAbility::GetTestCurrentPhaseRaw() const
{
	return static_cast<uint8>(CurrentPhase);
}

const FAbilityMontageRateWindowLifecycle& UPlayerLaunchReactionAbility::GetTestRateWindowLifecycle() const
{
	static const FAbilityMontageRateWindowLifecycle EmptyLifecycle;
	return MontageTask ? MontageTask->GetRateWindowLifecycle() : EmptyLifecycle;
}

FAbilityMontageRateWindowLifecycle& UPlayerLaunchReactionAbility::GetTestRateWindowLifecycle_Mutable()
{
	check(MontageTask);
	return MontageTask->GetRateWindowLifecycle_Mutable();
}

int32 UPlayerLaunchReactionAbility::GetTestRateWindowMontageInstanceID() const
{
	return MontageTask ? MontageTask->GetBoundMontageInstanceID() : INDEX_NONE;
}

bool UPlayerLaunchReactionAbility::HasTestRateWindowTasks() const
{
	return MontageTask && MontageTask->IsActive() && !MontageTask->IsTerminated();
}

bool UPlayerLaunchReactionAbility::GetTestDodgeCancelable() const
{
	return MontageTask && MontageTask->HasContributedDodgeTag();
}
#endif
