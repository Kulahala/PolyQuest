#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AI/EnemyAIController.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

namespace
{
	bool EvaluateStanceBreakCompatibility(
		int32 ActiveStanceBreakCount,
		int32 PreActivationStunnedContribution,
		bool& bOutHandoff)
	{
		bOutHandoff = false;
		if (ActiveStanceBreakCount < 0 || PreActivationStunnedContribution < 0)
		{
			return false;
		}

		if (ActiveStanceBreakCount == 0 && PreActivationStunnedContribution == 0)
		{
			bOutHandoff = false;
			return true;
		}

		if (ActiveStanceBreakCount == 1 && PreActivationStunnedContribution == 1)
		{
			bOutHandoff = true;
			return true;
		}

		return false;
	}
}

UEnemyVictimExecutionAbility::UEnemyVictimExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bNonLethalRecoveryActive = false;
	bSavedCanWalkOffLedges = false;
	bHasSavedCanWalkOffLedges = false;
	ActiveVictimMontageInstanceID = INDEX_NONE;
#if WITH_DEV_AUTOMATION_TESTS
	bTestBypassMontageActiveCheck = false;
#endif

	VictimAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Victim")), false);
	FrontRequestEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	BackstabRequestEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
	ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	VictimStartEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	VictimLockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	InvulnerableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeathPendingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	BlockMovementTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	BlockJumpTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
	StanceBreakAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.StanceBreak")), false);

	AbilityTags.AddTag(VictimAbilityTag);
	AbilityTags.AddTag(TeardownOnUnpossessTag);

	ActivationOwnedTags.AddTag(VictimLockedStateTag);
	ActivationOwnedTags.AddTag(InvulnerableStateTag);
	ActivationOwnedTags.AddTag(StunnedStateTag);
	ActivationOwnedTags.AddTag(BlockMovementTag);
	ActivationOwnedTags.AddTag(BlockJumpTag);

	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(VictimLockedStateTag);
	ActivationBlockedTags.AddTag(DeathPendingStateTag);

	FAbilityTriggerData FrontTrigger;
	FrontTrigger.TriggerTag = FrontRequestEventTag;
	FrontTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(FrontTrigger);

	FAbilityTriggerData BackstabTrigger;
	BackstabTrigger.TriggerTag = BackstabRequestEventTag;
	BackstabTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(BackstabTrigger);

	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false));
	AbilitiesToCancel.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false));
	AbilitiesToCancel.AddTag(StanceBreakAbilityTag);
}

bool UEnemyVictimExecutionAbility::CanActivateAbility(
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

	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	if (!ActualDeathPendingTag.IsValid())
	{
		return false;
	}

	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsDeathPending() || EnemyCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			return false;
		}
		if (ASC->HasMatchingGameplayTag(ActualDeathPendingTag))
		{
			return false;
		}
	}

	return true;
}

bool UEnemyVictimExecutionAbility::ValidateExecutionRequest(
	const FGameplayEventData* TriggerEventData,
	AEnemyCharacter* EnemyCharacter,
	bool& bOutHandoffFromStanceBreak) const
{
	bOutHandoffFromStanceBreak = false;

	if (!TriggerEventData || !EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsDeathPending() || EnemyCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	const UAbilitySystemComponent* CharacterASC = EnemyCharacter->GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return false;
	}

	if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return false;
	}

	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	if (ActualDeathPendingTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ActualDeathPendingTag))
	{
		return false;
	}

	const AActor* InstigatorActor = TriggerEventData->Instigator.Get();
	if (!InstigatorActor || InstigatorActor->IsActorBeingDestroyed() || InstigatorActor->GetWorld() != EnemyCharacter->GetWorld())
	{
		return false;
	}

	if (TriggerEventData->Target != EnemyCharacter)
	{
		return false;
	}

	const UExecutionLockContext* Context = Cast<UExecutionLockContext>(TriggerEventData->OptionalObject.Get());
	if (!Context || !Context->IsActive() || Context->IsVictimAccepted())
	{
		return false;
	}

	if (Context->GetTargetActor() != EnemyCharacter || Context->GetSourceActor() != InstigatorActor)
	{
		return false;
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const UAbilitySystemComponent* SourceASC = Context->GetSourceASC();
	const UGameplayAbility* SourceAbility = Context->GetSourceAbility();
	if (!SourceASC || (DeadTag.IsValid() && SourceASC->HasMatchingGameplayTag(DeadTag)) || !SourceAbility || !SourceAbility->IsActive() || Context->GetRequestTag() != TriggerEventData->EventTag)
	{
		return false;
	}

	const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	const FGameplayTag BackstabReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);

	if (TriggerEventData->EventTag == FrontReqTag)
	{
		const UCharacterAttributeSet* AttribSet = CharacterASC->GetSet<UCharacterAttributeSet>();
		const bool bPoiseZero = AttribSet && FMath::IsNearlyZero(AttribSet->GetPoise(), KINDA_SMALL_NUMBER);
		if (!bPoiseZero)
		{
			return false;
		}

		bool bHasActiveStanceBreak = false;
		for (const FGameplayAbilitySpec& Spec : CharacterASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UEnemyStanceBreakAbility>() && Spec.IsActive())
			{
				bHasActiveStanceBreak = true;
				break;
			}
		}

		if (!bHasActiveStanceBreak)
		{
			return false;
		}

		bOutHandoffFromStanceBreak = true;
	}
	else if (TriggerEventData->EventTag == BackstabReqTag)
	{
		if (VictimLockedTag.IsValid() && CharacterASC->GetTagCount(VictimLockedTag) > 1)
		{
			return false;
		}

		if (InvulnerableTag.IsValid() && CharacterASC->GetTagCount(InvulnerableTag) > 1)
		{
			return false;
		}

		if (!StunnedTag.IsValid() || !ActivationOwnedTags.HasTagExact(StunnedTag))
		{
			return false;
		}

		const int32 TotalStunnedCount = CharacterASC->GetTagCount(StunnedTag);
		if (TotalStunnedCount < 1)
		{
			return false;
		}

		const int32 PreActivationStunnedContribution = TotalStunnedCount - 1;

		int32 ActiveStanceBreakCount = 0;
		for (const FGameplayAbilitySpec& Spec : CharacterASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UEnemyStanceBreakAbility>() && Spec.IsActive())
			{
				++ActiveStanceBreakCount;
			}
		}

		bool bHandoff = false;
		if (!EvaluateStanceBreakCompatibility(ActiveStanceBreakCount, PreActivationStunnedContribution, bHandoff))
		{
			return false;
		}

		bOutHandoffFromStanceBreak = bHandoff;
	}
	else
	{
		return false;
	}

	return true;
}

void UEnemyVictimExecutionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityInProgress = false;
	bInAuthorizedHitScope = false;
	bNonLethalRecoveryActive = false;
	bSavedCanWalkOffLedges = false;
	bHasSavedCanWalkOffLedges = false;
	ActiveVictimMontageInstanceID = INDEX_NONE;
	StartupVictimMontageInstanceID = INDEX_NONE;
	NonLethalRecoverySourceActor = nullptr;

#if WITH_DEV_AUTOMATION_TESTS
	if (const UEnemyVictimExecutionAbility* CDO = Cast<UEnemyVictimExecutionAbility>(GetClass()->GetDefaultObject()))
	{
		if (CDO->bTestBypassMontageActiveCheck)
		{
			bTestBypassMontageActiveCheck = true;
		}
		if (!BoundAnimInstance.IsValid() && CDO->BoundAnimInstance.IsValid())
		{
			BoundAnimInstance = CDO->BoundAnimInstance;
		}
	}
#endif

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();

	const bool bPreviousPending = bDeathPending || (EnemyCharacter && EnemyCharacter->IsDeathPending());
	if (!bPreviousPending)
	{
		bDeathPending = false;
		bAddedDeathPendingTag = false;
	}

	bool bHandoff = false;
	if (!ValidateExecutionRequest(TriggerEventData, EnemyCharacter, bHandoff) || !CharacterASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EnemyCharacter->SetExecutionVictimAbility(this);

	ActiveExecutionContext = Cast<UExecutionLockContext>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));
	if (!ActiveExecutionContext)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bHandoffFromStanceBreak = bHandoff;

	// 1. Stop physical movement immediately
	if (UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		bMovementLockedByVictim = true;
	}

	// 2. Lock AI Controller
	if (AEnemyAIController* AIController = Cast<AEnemyAIController>(EnemyCharacter->GetController()))
	{
		AIController->BeginExecutionLock();
		bLockedAI = true;
	}

	// 3. Cancel competing enemy actions
	CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this);

	// 4. Accept victim lock in context synchronously
	const bool bAccepted = ActiveExecutionContext->AcceptVictim(
		this,
		EnemyCharacter,
		CharacterASC,
		TriggerEventData->EventTag,
		ActiveExecutionContext->GetSourceActivationToken());

	if (!bAccepted)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 5. Listen for Release event
	const FGameplayTag ReleaseTag = ReleaseEventTag.IsValid() ? ReleaseEventTag : FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	if (!ReleaseTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ReleaseTag, nullptr, false, false);
	if (!WaitReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitReleaseTask->EventReceived.AddDynamic(this, &UEnemyVictimExecutionAbility::OnReleaseReceived);
	WaitReleaseTask->ReadyForActivation();

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestInvalidateWaitReleaseTaskAfterReady)
	{
		WaitReleaseTask = nullptr;
	}
#endif

	if (!IsActive() || !ActiveExecutionContext || !WaitReleaseTask || !WaitReleaseTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 6. Cache pending victim montage and listen for VictimStart event to drive presentation
	const FGameplayTag FrontReqTag = FrontRequestEventTag.IsValid()
		? FrontRequestEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);

	const bool bIsFrontRequest = (TriggerEventData && TriggerEventData->EventTag == FrontReqTag);
	PendingVictimMontage = bIsFrontRequest
		? FrontExecutionVictimMontage
		: BackstabExecutionVictimMontage;
	bVictimPresentationStarted = false;

	const FGameplayTag VictimStartTag = VictimStartEventTag.IsValid()
		? VictimStartEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	if (!VictimStartTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitVictimStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, VictimStartTag, nullptr, false, false);
	if (!WaitVictimStartTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitVictimStartTask->EventReceived.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimStartReceived);
	WaitVictimStartTask->ReadyForActivation();

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestInvalidateWaitVictimStartTaskAfterReady)
	{
		WaitVictimStartTask = nullptr;
	}
#endif

	if (!IsActive() || !ActiveExecutionContext || !WaitVictimStartTask || !WaitVictimStartTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
}

bool UEnemyVictimExecutionAbility::BeginAuthorizedHitScope()
{
	if (!IsActive() || bEndAbilityInProgress || bDeathPending)
	{
		return false;
	}

	bInAuthorizedHitScope = true;
	return true;
}

void UEnemyVictimExecutionAbility::EndAuthorizedHitScope()
{
	bInAuthorizedHitScope = false;
}

void UEnemyVictimExecutionAbility::NotifyLethalDamageReceived()
{
	bDeathPending = true;
	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);

	if (!bAddedDeathPendingTag && ActualDeathPendingTag.IsValid())
	{
		if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
		{
			CharacterASC->AddLooseGameplayTag(ActualDeathPendingTag);
			bAddedDeathPendingTag = true;
		}
	}
}

void UEnemyVictimExecutionAbility::OnVictimStartReceived(FGameplayEventData Payload)
{
	if (!IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	if (bVictimPresentationStarted)
	{
		return;
	}

	const FGameplayTag ExpectedVictimStartTag = VictimStartEventTag.IsValid()
		? VictimStartEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	if (!ExpectedVictimStartTag.IsValid() || Payload.EventTag != ExpectedVictimStartTag)
	{
		return;
	}

	UExecutionLockContext* Context = ActiveExecutionContext.Get();
	if (!Context || !Context->IsActive())
	{
		return;
	}

	if (Payload.OptionalObject.Get() != Context)
	{
		return;
	}

	// 1. Verify Hit has been resolved before VictimStart
	const bool bHitResolved = (Context->GetHitState() == EExecutionSessionHitState::NonLethal ||
		Context->GetHitState() == EExecutionSessionHitState::DeathPending);
	if (!bHitResolved)
	{
		// Out-of-order VictimStart (before Hit) must be rejected without consuming success flag
		return;
	}

	// 2. Must have begun release transaction from Player, not already released, not cancelled
	if (!Context->IsReleaseSent() || Context->IsVictimReleased() || Context->WasReleaseCancelled())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* ContextSourceActor = Context->GetSourceActor();
	AActor* PayloadInstigator = const_cast<AActor*>(Payload.Instigator.Get());
	AActor* PayloadTarget = const_cast<AActor*>(Payload.Target.Get());

	if (!IsValid(AvatarActor) || !IsValid(ContextSourceActor) || !IsValid(PayloadInstigator) || !IsValid(PayloadTarget))
	{
		return;
	}

	if (Context->GetTargetActor() != AvatarActor || Context->GetVictimAbility() != this)
	{
		return;
	}

	if (PayloadInstigator != ContextSourceActor || PayloadTarget != AvatarActor)
	{
		return;
	}

	const UObject* AnimObj = Payload.OptionalObject2.Get();
	if (!AnimObj || (!AnimObj->IsA<UAnimMontage>() && !AnimObj->IsA<UAnimSequenceBase>()))
	{
		return;
	}

	bVictimPresentationStarted = true;
	Context->MarkVictimReleased(this);

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(AvatarActor);
	const bool bCommitDeath = bDeathPending || (EnemyCharacter && EnemyCharacter->IsDeathPending()) || (Context->GetHitState() == EExecutionSessionHitState::DeathPending);
	if (bCommitDeath)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 3. Non-lethal: full Root Motion recovery montage from time 0 (strictly use snapshot from activation)
	NonLethalRecoverySourceActor = ContextSourceActor;
	UAnimMontage* MontageToPlay = PendingVictimMontage.Get();
	UCharacterMovementComponent* CMC = EnemyCharacter ? EnemyCharacter->GetCharacterMovement() : nullptr;
	UAnimInstance* AnimInstance = nullptr;
	if (EnemyCharacter)
	{
		if (USkeletalMeshComponent* Mesh = EnemyCharacter->GetMesh())
		{
			AnimInstance = Mesh->GetAnimInstance();
		}
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (!AnimInstance && BoundAnimInstance.IsValid())
	{
		AnimInstance = BoundAnimInstance.Get();
	}
	if (!AnimInstance)
	{
		if (const UEnemyVictimExecutionAbility* CDO = Cast<UEnemyVictimExecutionAbility>(GetClass()->GetDefaultObject()))
		{
			if (CDO->BoundAnimInstance.IsValid())
			{
				AnimInstance = CDO->BoundAnimInstance.Get();
			}
		}
	}
#endif

	const bool bHasSlotTracks = MontageToPlay && (MontageToPlay->SlotAnimTracks.Num() > 0);
	const float PlayLength = MontageToPlay ? MontageToPlay->GetPlayLength() : 0.0f;
	const bool bPlayLengthValid = FMath::IsFinite(PlayLength) && PlayLength > 0.0f;

	const bool bModeAllowedForRecovery = CMC && (
		(bMovementLockedByVictim && CMC->MovementMode == MOVE_None) ||
		(CMC->MovementMode == MOVE_Walking)
	);

	bool bHasValidFloor = false;
	if (bModeAllowedForRecovery)
	{
		const FVector CapsuleLocation = CMC->UpdatedComponent ? CMC->UpdatedComponent->GetComponentLocation() : EnemyCharacter->GetActorLocation();
		FFindFloorResult FloorResult;
		CMC->FindFloor(CapsuleLocation, FloorResult, false /* bCanUseCachedLocation = false: force real downward sweep */);
		bHasValidFloor = FloorResult.IsWalkableFloor();
	}

	const bool bCanHandoff = MontageToPlay && EnemyCharacter && !EnemyCharacter->IsActorBeingDestroyed() &&
		AnimInstance && CMC && bModeAllowedForRecovery && bHasSlotTracks && bPlayLengthValid && bHasValidFloor;

	if (!bCanHandoff)
	{
		NonLethalRecoverySourceActor = nullptr;
		UE_LOG(LogPolyQuest, Warning, TEXT("EnemyVictimExecutionAbility for '%s' cannot handoff victim montage '%s' (ModeAllowed: %d, HasSlotTracks: %d, PlayLengthValid: %d [%.2f], ValidFloor: %d). Cleaning up."),
			*GetNameSafe(EnemyCharacter), *GetNameSafe(MontageToPlay), bModeAllowedForRecovery, bHasSlotTracks, bPlayLengthValid, PlayLength, bHasValidFloor);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}
	if (WaitVictimStartTask)
	{
		WaitVictimStartTask->EndTask();
		WaitVictimStartTask = nullptr;
	}

	CMC->StopMovementImmediately();
	bSavedCanWalkOffLedges = CMC->bCanWalkOffLedges;
	bHasSavedCanWalkOffLedges = true;
	CMC->bCanWalkOffLedges = false;

	EnemyCharacter->MovementModeChangedDelegate.AddUniqueDynamic(this, &UEnemyVictimExecutionAbility::OnMovementModeChanged);
	CMC->SetMovementMode(MOVE_Walking);

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestCancelDuringStartupMovementMode)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
#endif

	if (!IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	VictimMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		false /* bStopWhenAbilityEnds = false */,
		1.0f  /* AnimRootMotionTranslationScale */,
		0.0f  /* StartTimeSeconds = 0.0f */,
		true  /* bAllowInterruptAfterBlendOut = true */);

	if (VictimMontageTask)
	{
		VictimMontageTask->OnCompleted.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCompleted);
		VictimMontageTask->OnBlendOut.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageBlendOut);
		VictimMontageTask->OnInterrupted.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageInterrupted);
		VictimMontageTask->OnCancelled.AddDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCancelled);

		// Establish presentation ownership and startup tracking prior to activation so synchronous cancel can cleanly stop the montage
		PendingStartupVictimMontage = MontageToPlay;
		bStartupCancellationPending = false;
		ActiveVictimMontage = MontageToPlay;
		ActiveVictimMontageInstanceID = INDEX_NONE;
		StartupVictimMontageInstanceID = INDEX_NONE;

		if (AnimInstance)
		{
			AnimInstance->OnMontageStarted.AddUniqueDynamic(this, &UEnemyVictimExecutionAbility::HandleOnMontageStarted);
		}

#if WITH_DEV_AUTOMATION_TESTS
		if (bTestCancelDuringStartupReadyForActivation)
		{
			CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		}
#endif

		if (!IsActive() || bEndAbilityInProgress || bStartupCancellationPending)
		{
			if (AnimInstance)
			{
				AnimInstance->OnMontageStarted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::HandleOnMontageStarted);
			}
			PendingStartupVictimMontage = nullptr;
			StartupVictimMontageInstanceID = INDEX_NONE;
			bStartupCancellationPending = false;
			ActiveVictimMontage = nullptr;
			ActiveVictimMontageInstanceID = INDEX_NONE;
			return;
		}

#if WITH_DEV_AUTOMATION_TESTS
		if (!bTestBypassMontageActiveCheck)
#endif
		{
			VictimMontageTask->ReadyForActivation();
		}

		if (AnimInstance)
		{
			AnimInstance->OnMontageStarted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::HandleOnMontageStarted);
		}

		if (!IsActive() || bEndAbilityInProgress || bStartupCancellationPending)
		{
			// Ability was cancelled/ended while inside ReadyForActivation call!
			// If a new instance was created for this specific startup call, ensure it is cleanly stopped with 0.0s blend.
			// CONTRACT: Strictly stop only the confirmed instance ID for this activation; NEVER fallback to asset search!
			if (AnimInstance && PendingStartupVictimMontage)
			{
				const int32 TargetInstanceID = (ActiveVictimMontageInstanceID != INDEX_NONE)
					? ActiveVictimMontageInstanceID
					: StartupVictimMontageInstanceID;

				if (TargetInstanceID != INDEX_NONE)
				{
					FAnimMontageInstance* ResidualInstance = AnimInstance->GetMontageInstanceForID(TargetInstanceID);
					if (ResidualInstance && ResidualInstance->Montage == PendingStartupVictimMontage && !ResidualInstance->IsStopped())
					{
						FMontageBlendSettings BlendOutSettings;
						BlendOutSettings.Blend = PendingStartupVictimMontage->BlendOut;
						BlendOutSettings.Blend.BlendTime = 0.0f;
						BlendOutSettings.BlendMode = PendingStartupVictimMontage->BlendModeOut;
						BlendOutSettings.BlendProfile = PendingStartupVictimMontage->BlendProfileOut;
						ResidualInstance->Stop(BlendOutSettings, true);
					}
				}
			}

			PendingStartupVictimMontage = nullptr;
			StartupVictimMontageInstanceID = INDEX_NONE;
			bStartupCancellationPending = false;
			ActiveVictimMontage = nullptr;
			ActiveVictimMontageInstanceID = INDEX_NONE;
			return;
		}

		PendingStartupVictimMontage = nullptr;
		bStartupCancellationPending = false;

		FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(MontageToPlay) : nullptr;
		const bool bMontagePlaying =
#if WITH_DEV_AUTOMATION_TESTS
			bTestBypassMontageActiveCheck ||
#endif
			(MontageInstance != nullptr);

		const bool bTaskActive =
#if WITH_DEV_AUTOMATION_TESTS
			bTestBypassMontageActiveCheck ||
#endif
			(VictimMontageTask && VictimMontageTask->IsActive());

		if (bMontagePlaying && bTaskActive && CMC->MovementMode == MOVE_Walking)
		{
			bNonLethalRecoveryActive = true;
			if (ActiveVictimMontageInstanceID == INDEX_NONE)
			{
				ActiveVictimMontageInstanceID = (StartupVictimMontageInstanceID != INDEX_NONE)
					? StartupVictimMontageInstanceID
					: (MontageInstance ? MontageInstance->GetInstanceID() : INDEX_NONE);
			}
			StartupVictimMontageInstanceID = INDEX_NONE;
		}
		else
		{
			StopVictimMontagePresentation(false);
			bNonLethalRecoveryActive = false;
			ActiveVictimMontage = nullptr;
			ActiveVictimMontageInstanceID = INDEX_NONE;
			NonLethalRecoverySourceActor = nullptr;
			EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnMovementModeChanged);
			if (bHasSavedCanWalkOffLedges)
			{
				CMC->bCanWalkOffLedges = bSavedCanWalkOffLedges;
				bHasSavedCanWalkOffLedges = false;
			}
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
	else
	{
		NonLethalRecoverySourceActor = nullptr;
		EnemyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnMovementModeChanged);
		if (bHasSavedCanWalkOffLedges)
		{
			CMC->bCanWalkOffLedges = bSavedCanWalkOffLedges;
			bHasSavedCanWalkOffLedges = false;
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UEnemyVictimExecutionAbility::OnVictimMontageCompleted()
{
	if (bNonLethalRecoveryActive)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	StopVictimMontagePresentation(true);
}

void UEnemyVictimExecutionAbility::OnVictimMontageBlendOut()
{
	if (bNonLethalRecoveryActive)
	{
		// 恢复期间自然 BlendOut 不是完成，不释放锁、不结束 Task
		return;
	}
	StopVictimMontagePresentation(true);
}

void UEnemyVictimExecutionAbility::OnVictimMontageInterrupted()
{
	if (bNonLethalRecoveryActive)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	StopVictimMontagePresentation(false);
}

void UEnemyVictimExecutionAbility::OnVictimMontageCancelled()
{
	if (bNonLethalRecoveryActive)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	StopVictimMontagePresentation(false);
}

void UEnemyVictimExecutionAbility::HandleOnMontageStarted(UAnimMontage* Montage)
{
	if (StartupVictimMontageInstanceID != INDEX_NONE)
	{
		// Already captured an instance for this startup call; do not accept or overwrite with any subsequent instance!
		return;
	}

	if (Montage == PendingStartupVictimMontage || Montage == ActiveVictimMontage)
	{
		UAnimInstance* AnimInstance = nullptr;
		if (AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (USkeletalMeshComponent* Mesh = EnemyCharacter->GetMesh())
			{
				AnimInstance = Mesh->GetAnimInstance();
			}
		}
#if WITH_DEV_AUTOMATION_TESTS
		if (!AnimInstance && BoundAnimInstance.IsValid())
		{
			AnimInstance = BoundAnimInstance.Get();
		}
		if (!AnimInstance)
		{
			if (const UEnemyVictimExecutionAbility* CDO = Cast<UEnemyVictimExecutionAbility>(GetClass()->GetDefaultObject()))
			{
				if (CDO->BoundAnimInstance.IsValid())
				{
					AnimInstance = CDO->BoundAnimInstance.Get();
				}
			}
		}
#endif
		if (AnimInstance)
		{
			FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
			if (!Instance && AnimInstance->MontageInstances.Num() > 0)
			{
				for (int32 Index = AnimInstance->MontageInstances.Num() - 1; Index >= 0; --Index)
				{
					if (AnimInstance->MontageInstances[Index] && AnimInstance->MontageInstances[Index]->Montage == Montage)
					{
						Instance = AnimInstance->MontageInstances[Index];
						break;
					}
				}
			}
			if (Instance)
			{
				const int32 NewbornID = Instance->GetInstanceID();
				ActiveVictimMontageInstanceID = NewbornID;
				StartupVictimMontageInstanceID = NewbornID;

				// Crucial contract: Once this startup call captures its newborn instance, immediately unbind
				// from OnMontageStarted to prevent any reentrant montage play from re-triggering this handler!
				AnimInstance->OnMontageStarted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::HandleOnMontageStarted);

#if WITH_DEV_AUTOMATION_TESTS
				if (TestOnMontageStartedHook)
				{
					TestOnMontageStartedHook(Montage);
				}
#endif

				// If ability has been cancelled/ended before or during instance birth, stop this newborn instance immediately!
				if (bStartupCancellationPending || !IsActive() || bEndAbilityInProgress)
				{
					FMontageBlendSettings BlendOutSettings;
					BlendOutSettings.Blend = Montage->BlendOut;
					BlendOutSettings.Blend.BlendTime = 0.0f;
					BlendOutSettings.BlendMode = Montage->BlendModeOut;
					BlendOutSettings.BlendProfile = Montage->BlendProfileOut;
					Instance->Stop(BlendOutSettings, true);

					bStartupCancellationPending = false;
				}
			}
		}
	}
}

void UEnemyVictimExecutionAbility::StopVictimMontagePresentation(bool bIsNaturalCompletion)
{
	if (VictimMontageTask)
	{
		VictimMontageTask->OnCompleted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCompleted);
		VictimMontageTask->OnBlendOut.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageBlendOut);
		VictimMontageTask->OnInterrupted.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageInterrupted);
		VictimMontageTask->OnCancelled.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnVictimMontageCancelled);
		VictimMontageTask->EndTask();
		VictimMontageTask = nullptr;
	}

	if (!bIsNaturalCompletion)
	{
		if (AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (!EnemyCharacter->IsActorBeingDestroyed())
			{
				UAnimInstance* AnimInstance = nullptr;
				if (USkeletalMeshComponent* Mesh = EnemyCharacter->GetMesh())
				{
					AnimInstance = Mesh->GetAnimInstance();
				}
#if WITH_DEV_AUTOMATION_TESTS
				if (!AnimInstance && BoundAnimInstance.IsValid())
				{
					AnimInstance = BoundAnimInstance.Get();
				}
				if (!AnimInstance)
				{
					if (const UEnemyVictimExecutionAbility* CDO = Cast<UEnemyVictimExecutionAbility>(GetClass()->GetDefaultObject()))
					{
						if (CDO->BoundAnimInstance.IsValid())
						{
							AnimInstance = CDO->BoundAnimInstance.Get();
						}
					}
				}
#endif
				if (AnimInstance && ActiveVictimMontage)
				{
					FAnimMontageInstance* InstanceToStop = nullptr;
					if (ActiveVictimMontageInstanceID != INDEX_NONE)
					{
						InstanceToStop = AnimInstance->GetMontageInstanceForID(ActiveVictimMontageInstanceID);
					}

					if (InstanceToStop && InstanceToStop->Montage == ActiveVictimMontage)
					{
						FMontageBlendSettings BlendOutSettings;
						BlendOutSettings.Blend = ActiveVictimMontage->BlendOut;
						BlendOutSettings.Blend.BlendTime = 0.0f;
						BlendOutSettings.BlendMode = ActiveVictimMontage->BlendModeOut;
						BlendOutSettings.BlendProfile = ActiveVictimMontage->BlendProfileOut;
						InstanceToStop->Stop(BlendOutSettings, true);
					}
				}
			}
		}
	}

	ActiveVictimMontage = nullptr;
	ActiveVictimMontageInstanceID = INDEX_NONE;
}

void UEnemyVictimExecutionAbility::OnReleaseReceived(FGameplayEventData Payload)
{
	if (!IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	if (bNonLethalRecoveryActive)
	{
		// Duplicate or late release has no effect once recovery is active
		return;
	}

	const FGameplayTag ExpectedReleaseTag = ReleaseEventTag.IsValid()
		? ReleaseEventTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	if (!ExpectedReleaseTag.IsValid() || Payload.EventTag != ExpectedReleaseTag)
	{
		return;
	}

	UExecutionLockContext* Context = ActiveExecutionContext.Get();
	if (!Context || !Context->IsActive())
	{
		return;
	}

	if (Payload.OptionalObject.Get() != Context)
	{
		return;
	}

	if (!Context->IsReleaseSent())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* ContextSourceActor = Context->GetSourceActor();
	AActor* PayloadInstigator = const_cast<AActor*>(Payload.Instigator.Get());
	AActor* PayloadTarget = const_cast<AActor*>(Payload.Target.Get());

	if (!IsValid(AvatarActor) || !IsValid(ContextSourceActor) || !IsValid(PayloadInstigator) || !IsValid(PayloadTarget))
	{
		return;
	}

	if (Context->GetTargetActor() != AvatarActor || Context->GetVictimAbility() != this)
	{
		return;
	}

	if (PayloadInstigator != ContextSourceActor || PayloadTarget != AvatarActor)
	{
		return;
	}

	Context->MarkVictimReleased(this);

	const bool bWasCancelled = Context->WasReleaseCancelled();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UEnemyVictimExecutionAbility::OnMovementModeChanged(
	ACharacter* InCharacter,
	EMovementMode PrevMovementMode,
	uint8 PreviousCustomMode)
{
	if (!IsActive() || bEndAbilityInProgress || !bNonLethalRecoveryActive)
	{
		return;
	}

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	if (!EnemyCharacter || InCharacter != EnemyCharacter)
	{
		return;
	}

	const UCharacterMovementComponent* CMC = EnemyCharacter->GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	if (CMC->MovementMode != MOVE_Walking)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UEnemyVictimExecutionAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bEndAbilityInProgress)
	{
		return;
	}

	bEndAbilityInProgress = true;
	bInAuthorizedHitScope = false;

	if (PendingStartupVictimMontage != nullptr)
	{
		bStartupCancellationPending = true;
	}

	AEnemyCharacter* CachedEnemy = Cast<AEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CachedASC = GetAbilitySystemComponentFromActorInfo();
	UExecutionLockContext* CachedContext = ActiveExecutionContext.Get();
	AActor* CachedSourceActor = CachedContext ? CachedContext->GetSourceActor() : nullptr;

	if (CachedEnemy)
	{
		CachedEnemy->MovementModeChangedDelegate.RemoveDynamic(this, &UEnemyVictimExecutionAbility::OnMovementModeChanged);
	}

	if (bHasSavedCanWalkOffLedges && CachedEnemy)
	{
		if (UCharacterMovementComponent* MovementComponent = CachedEnemy->GetCharacterMovement())
		{
			MovementComponent->bCanWalkOffLedges = bSavedCanWalkOffLedges;
		}
		bHasSavedCanWalkOffLedges = false;
	}

	const bool bHadNonLethalRecovery = bNonLethalRecoveryActive;
	StopVictimMontagePresentation(false);

	const bool bCommitDeath = bDeathPending || (CachedEnemy && CachedEnemy->IsDeathPending());
	bool bDeathCommittedSuccessfully = false;

	const bool bHitResolved = CachedContext && (
		CachedContext->GetHitState() == EExecutionSessionHitState::NonLethal ||
		CachedContext->GetHitState() == EExecutionSessionHitState::DeathPending);
	const bool bConfirmedRelease = !bWasCancelled && CachedContext &&
		(CachedContext->IsVictimReleased() || CachedContext->IsReleaseSent());
	if (PendingVictimMontage.Get() != nullptr && bConfirmedRelease && bHitResolved && !bVictimPresentationStarted)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("EnemyVictimExecutionAbility for '%s' configured victim montage '%s' but received no VictimStart event before Release."),
			*GetNameSafe(CachedEnemy), *GetNameSafe(PendingVictimMontage.Get()));
	}

	if (bCommitDeath && CachedEnemy && CachedASC)
	{
		const bool bCanFinalize = CachedContext && CachedContext->BeginOutcomeFinalization(this);
		if (bCanFinalize)
		{
			bDeathCommittedSuccessfully = CachedEnemy->CommitExecutionDeath(CachedContext);
			if (bDeathCommittedSuccessfully)
			{
				CachedContext->CompleteOutcomeFinalization(this);
			}
			else
			{
				CachedContext->AbortOutcomeFinalization(this);
			}
		}

		// Fallback: If execution commit failed or context was invalid, verify if Health is <= 0.
		// Never leave an enemy with Health <= 0 without Dead or DeathPending.
		const float CurrentHealth = CachedASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
		if (!bDeathCommittedSuccessfully && CurrentHealth <= 0.0f)
		{
			CachedEnemy->SetDeadState();
			bDeathCommittedSuccessfully = CachedEnemy->IsDead();
		}
	}
	else
	{
		if (bLockedAI)
		{
			if (CachedEnemy)
			{
				if (AEnemyAIController* AIController = Cast<AEnemyAIController>(CachedEnemy->GetController()))
				{
					AIController->EndExecutionLock();
				}
			}
			bLockedAI = false;
		}

		const bool bCanRestoreEnemy = CachedEnemy && !CachedEnemy->IsDead() && !CachedEnemy->IsActorBeingDestroyed();
		if (bCanRestoreEnemy)
		{
			if (UCharacterMovementComponent* MovementComponent = CachedEnemy->GetCharacterMovement())
			{
				if (bHadNonLethalRecovery)
				{
					if (bWasCancelled && MovementComponent->MovementMode == MOVE_Walking)
					{
						MovementComponent->StopMovementImmediately();
					}
				}
				else if (bMovementLockedByVictim)
				{
					if (MovementComponent->MovementMode == MOVE_None)
					{
						const FVector CapsuleLocation = MovementComponent->UpdatedComponent
							? MovementComponent->UpdatedComponent->GetComponentLocation()
							: CachedEnemy->GetActorLocation();
						FFindFloorResult FloorResult;
						MovementComponent->FindFloor(CapsuleLocation, FloorResult, false);
						if (FloorResult.IsWalkableFloor())
						{
							MovementComponent->SetMovementMode(MOVE_Walking);
						}
						else
						{
							MovementComponent->SetMovementMode(MOVE_Falling);
						}
					}
					// If MovementMode != MOVE_None (e.g. externally changed to Falling or other), preserve external mode ownership!
				}
			}

			if (bHandoffFromStanceBreak)
			{
				if (!CachedEnemy->RestorePoiseToMax())
				{
					UE_LOG(LogPolyQuest, Warning, TEXT("Victim execution ended for '%s' but Poise could not be restored."), *GetNameSafe(CachedEnemy));
				}
			}
		}
	}

	// Only clean up DeathPending tag if Dead is confirmed written, or if non-lethal (not bCommitDeath).
	// If commit failed and enemy is still not dead despite Health <= 0, retain DeathPending tag.
	const FGameplayTag ActualDeathPendingTag = DeathPendingStateTag.IsValid()
		? DeathPendingStateTag
		: FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);

	const bool bIsDeadConfirmed = CachedEnemy ? CachedEnemy->IsDead() : false;
	const bool bSafeToClearPendingTag = bIsDeadConfirmed || !bCommitDeath;

	if (bAddedDeathPendingTag && ActualDeathPendingTag.IsValid() && bSafeToClearPendingTag)
	{
		if (CachedASC)
		{
			CachedASC->RemoveLooseGameplayTag(ActualDeathPendingTag);
		}
		bAddedDeathPendingTag = false;
	}

	if (CachedEnemy)
	{
		CachedEnemy->ClearExecutionVictimAbility(this);
	}

	bMovementLockedByVictim = false;
	bHandoffFromStanceBreak = false;
	bLockedAI = false;

	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}

	if (WaitVictimStartTask)
	{
		WaitVictimStartTask->EndTask();
		WaitVictimStartTask = nullptr;
	}

	PendingVictimMontage = nullptr;
	bVictimPresentationStarted = false;
	bNonLethalRecoveryActive = false;
	ActiveVictimMontageInstanceID = INDEX_NONE;
	NonLethalRecoverySourceActor = nullptr;

	if (CachedContext && bWasCancelled)
	{
		CachedContext->InvalidateSession();
	}
	ActiveExecutionContext = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	bEndAbilityInProgress = false;
}

bool UEnemyVictimExecutionAbility::IsNonLethalRecoveryFrom(const AActor* SourceActor) const
{
	if (!SourceActor || !IsActive() || bEndAbilityInProgress)
	{
		return false;
	}

	if (!bNonLethalRecoveryActive || bDeathPending)
	{
		return false;
	}

	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	if (!EnemyCharacter || EnemyCharacter->IsDead() || EnemyCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	return NonLethalRecoverySourceActor.Get() == SourceActor;
}

#if WITH_DEV_AUTOMATION_TESTS
void UEnemyVictimExecutionAbility::SetTestInvalidateWaitVictimStartTaskAfterReady(bool bInvalidate)
{
	bTestInvalidateWaitVictimStartTaskAfterReady = bInvalidate;
	if (bInvalidate && WaitVictimStartTask)
	{
		WaitVictimStartTask->EndTask();
		WaitVictimStartTask = nullptr;
	}
}

void UEnemyVictimExecutionAbility::TestTriggerMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode)
{
	OnMovementModeChanged(Cast<ACharacter>(GetAvatarActorFromActorInfo()), PrevMode, PrevCustomMode);
}
#endif
