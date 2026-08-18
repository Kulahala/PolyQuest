#include "AbilitySystem/Abilities/EnemyMeleeAbility.h"

#include "AI/EnemyAIController.h"
#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Enemy/EnemyAttackProfile.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

UEnemyMeleeAbility::UEnemyMeleeAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	TraceWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.Begin")), false);
	TraceWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.TraceWindow.End")), false);
	HyperArmorStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	HyperArmorBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.HyperArmor.Begin")), false);
	HyperArmorEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.HyperArmor.End")), false);

	AbilityTags.AddTag(EnemyMeleeAbilityTag);
	ActivationOwnedTags.AddTag(AttackingStateTag);
	ActivationBlockedTags.AddTag(AttackingStateTag);
	ActivationBlockedTags.AddTag(HitReactingStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));
}

bool UEnemyMeleeAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UEnemyMeleeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	ActiveAttackProfile = nullptr;
	ActiveMontage = nullptr;
	ActiveDamageGameplayEffectClass = nullptr;
	ActiveCooldownAfterAttack = 0.0f;
	ActiveGuardStaminaDamage = 0.0f;
	bAttackStarted = false;
	bHyperArmorActive = false;
	BoundAnimInstance = nullptr;

	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	AEnemyAIController* EnemyAIController = EnemyCharacter ? Cast<AEnemyAIController>(EnemyCharacter->GetController()) : nullptr;

	if (!ValidateActivationSetup(ActorInfo) || !EnemyAIController)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy melee activation aborted for '%s': ASC, controller target/pending profile in range, AnimInstance, valid AttackSet, and trace window tags are required."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		if (EnemyAIController)
		{
			EnemyAIController->ClearPendingAttackProfile();
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UEnemyAttackProfile* PendingProfile = EnemyAIController->GetPendingAttackProfile();
	if (!PendingProfile || !PendingProfile->IsValidAttackProfile())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy melee activation aborted for '%s': pending AttackProfile is invalid or missing."), *GetNameSafe(EnemyCharacter));
		EnemyAIController->ClearPendingAttackProfile();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	ActiveAttackProfile = PendingProfile;
	ActiveMontage = PendingProfile->GetAttackMontage();
	ActiveDamageGameplayEffectClass = PendingProfile->GetDamageGameplayEffectClass();
	ActiveCooldownAfterAttack = PendingProfile->GetCooldownAfterAttack();
	ActiveGuardStaminaDamage = PendingProfile->GetGuardStaminaDamage();

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ActiveMontage);
	TraceWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowBeginEventTag, nullptr, false, true);
	TraceWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TraceWindowEndEventTag, nullptr, false, true);
	HyperArmorBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HyperArmorBeginEventTag, nullptr, false, true);
	HyperArmorEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HyperArmorEndEventTag, nullptr, false, true);
	if (!MontageTask || !TraceWindowBeginTask || !TraceWindowEndTask || !HyperArmorBeginTask || !HyperArmorEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy melee activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(EnemyCharacter));
		EnemyAIController->ClearPendingAttackProfile();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy melee activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EnemyAIController->ClearPendingAttackProfile();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyMeleeAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyMeleeAbility::OnActiveMontageEnded);
	TraceWindowBeginTask->EventReceived.AddDynamic(this, &UEnemyMeleeAbility::OnTraceWindowBegin);
	TraceWindowEndTask->EventReceived.AddDynamic(this, &UEnemyMeleeAbility::OnTraceWindowEnd);
	HyperArmorBeginTask->EventReceived.AddDynamic(this, &UEnemyMeleeAbility::OnHyperArmorBegin);
	HyperArmorEndTask->EventReceived.AddDynamic(this, &UEnemyMeleeAbility::OnHyperArmorEnd);

	TraceWindowBeginTask->ReadyForActivation();
	TraceWindowEndTask->ReadyForActivation();
	HyperArmorBeginTask->ReadyForActivation();
	HyperArmorEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// Zero-length or invalid authored montages can synchronously complete and run the unified teardown.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy melee activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(ActiveMontage));
		EnemyAIController->ClearPendingAttackProfile();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bAttackStarted = true;
}

void UEnemyMeleeAbility::EndAbility(
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
	const bool bShouldStartCooldown = bAttackStarted;
	const float CooldownAfterAttack = ActiveCooldownAfterAttack;
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	AEnemyAIController* EnemyAIController = EnemyCharacter ? Cast<AEnemyAIController>(EnemyCharacter->GetController()) : nullptr;
	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (HyperArmorStateTag.IsValid())
		{
			CharacterASC->SetLooseGameplayTagCount(HyperArmorStateTag, 0);
		}
	}
	bHyperArmorActive = false;
	CloseTraceWindow();

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyMeleeAbility::OnActiveMontageEnded);
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

	if (HyperArmorBeginTask)
	{
		HyperArmorBeginTask->EndTask();
		HyperArmorBeginTask = nullptr;
	}

	if (HyperArmorEndTask)
	{
		HyperArmorEndTask->EndTask();
		HyperArmorEndTask = nullptr;
	}

	ActiveAttackProfile = nullptr;
	ActiveMontage = nullptr;
	ActiveDamageGameplayEffectClass = nullptr;
	ActiveCooldownAfterAttack = 0.0f;
	ActiveGuardStaminaDamage = 0.0f;
	bAttackStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (bShouldStartCooldown && IsValid(EnemyAIController))
	{
		EnemyAIController->StartMeleeAttackCooldown(CooldownAfterAttack);
	}
	else if (IsValid(EnemyAIController))
	{
		EnemyAIController->ClearPendingAttackProfile();
	}
}

void UEnemyMeleeAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void UEnemyMeleeAbility::OnTraceWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		OpenTraceWindow();
	}
}

void UEnemyMeleeAbility::OnTraceWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		CloseTraceWindow();
	}
}

void UEnemyMeleeAbility::OnHyperArmorBegin(FGameplayEventData Payload)
{
	if (bHyperArmorActive || !bAttackStarted || !IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (HyperArmorStateTag.IsValid())
		{
			CharacterASC->SetLooseGameplayTagCount(HyperArmorStateTag, 1);
			bHyperArmorActive = true;
		}
	}
}

void UEnemyMeleeAbility::OnHyperArmorEnd(FGameplayEventData Payload)
{
	if (!bAttackStarted || !IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (HyperArmorStateTag.IsValid())
		{
			CharacterASC->SetLooseGameplayTagCount(HyperArmorStateTag, 0);
		}
	}
	bHyperArmorActive = false;
}

bool UEnemyMeleeAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AEnemyAIController* EnemyAIController = EnemyCharacter ? Cast<AEnemyAIController>(EnemyCharacter->GetController()) : nullptr;
	const UEnemyAttackSet* AttackSet = EnemyCharacter ? EnemyCharacter->GetAttackSet() : nullptr;
	FString SetValidationReason;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	return AbilitySystemComponent && EnemyCharacter && EnemyAIController && AttackSet && AttackSet->IsAttackSetValid(SetValidationReason) && EnemyAIController->HasValidAttackSet() && AnimInstance
		&& EnemyMeleeAbilityTag.IsValid() && AttackingStateTag.IsValid() && HitReactingStateTag.IsValid() && TraceWindowBeginEventTag.IsValid() && TraceWindowEndEventTag.IsValid()
		&& HyperArmorStateTag.IsValid() && HyperArmorBeginEventTag.IsValid() && HyperArmorEndEventTag.IsValid()
		&& EnemyAIController->HasValidCombatTarget() && EnemyAIController->IsCombatTargetInMeleeRange() && EnemyAIController->HasPendingAttackProfile() && EnemyAIController->IsPendingAttackInRange();
}

bool UEnemyMeleeAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && ActiveMontage && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.OptionalObject.Get() == ActiveMontage.Get();
}

void UEnemyMeleeAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UEnemyMeleeAbility::OpenTraceWindow()
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
		? UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(this, Character->GetMeleeTraceSource(), ActiveDamageGameplayEffectClass, GetAbilityLevel(), FGameplayTag(), 0.0f, ActiveGuardStaminaDamage)
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

void UEnemyMeleeAbility::CloseTraceWindow()
{
	if (TraceWindowTask)
	{
		TraceWindowTask->EndTask();
		TraceWindowTask = nullptr;
	}
}
