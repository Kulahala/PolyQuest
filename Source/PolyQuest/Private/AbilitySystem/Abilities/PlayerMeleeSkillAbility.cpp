#include "AbilitySystem/Abilities/PlayerMeleeSkillAbility.h"

#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/BaseCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
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
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !MovementComponent || !MovementComponent->IsMovingOnGround()
		|| !SkillMontage || !CostGameplayEffectClass || !CooldownGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass
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

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SkillMontage);
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag, nullptr, false, true);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag, nullptr, false, true);
	if (!MontageTask || !TraceWindowBeginTask || !TraceWindowEndTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask || !RateWindowBeginTask || !RateWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Melee skill activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SkillMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerMeleeSkillAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerMeleeSkillAbility::OnActiveMontageEnded);
	TraceWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnTraceWindowBegin);
	TraceWindowEndTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnTraceWindowEnd);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnDodgeCancelWindowEnd);
	RateWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnRateWindowBegin);
	RateWindowEndTask->EventReceived.AddDynamic(this, &UPlayerMeleeSkillAbility::OnRateWindowEnd);

	// Only the montage task starts before the commit: it must be playing so its
	// identity can be confirmed. The six notify-window tasks stay un-armed, so a
	// failed commit leaves no trace, no cancel tag, and no rate window behind.
	MontageTask->ReadyForActivation();

	// Montage startup can synchronously invoke the bound end delegate. That path has already cleaned every task and pointer.
	if (bEndAbilityRequested)
	{
		return;
	}

	// The single commit runs only after the tracked montage identity is confirmed
	// active: before this point nothing is committed and no side effect exists.
	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
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

	SetRuntimeActionTags(true);
	PlayerCharacter->ApplyActionFacing();

	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	RateWindowBeginTask->ReadyForActivation();
	RateWindowEndTask->ReadyForActivation();

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
	SetDodgeCancelable(false);
	SetRuntimeActionTags(false);
	CloseTraceWindow();
	RestoreBaselineMontageRate();

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerMeleeSkillAbility::OnActiveMontageEnded);
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

void UPlayerMeleeSkillAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void UPlayerMeleeSkillAbility::OnTraceWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		OpenTraceWindow();
	}
}

void UPlayerMeleeSkillAbility::OnTraceWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		CloseTraceWindow();
	}
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

void UPlayerMeleeSkillAbility::OpenTraceWindow()
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

void UPlayerMeleeSkillAbility::CloseTraceWindow()
{
	if (TraceWindowTask)
	{
		TraceWindowTask->EndTask();
		TraceWindowTask = nullptr;
	}
}

void UPlayerMeleeSkillAbility::OnRateWindowBegin(FGameplayEventData Payload)
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
	if (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
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
