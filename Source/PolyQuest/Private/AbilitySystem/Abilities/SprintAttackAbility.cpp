#include "AbilitySystem/Abilities/SprintAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/BaseCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	SprintStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	MovementInputBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	JumpInputBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	StaminaRegenBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Resource.Stamina.RegenBlocked")), false);
	HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Sprint.Hit")), false);
	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
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
	bEndAbilityRequested = false;
	bHitEventConsumed = false;
	bDodgeCancelable = false;
	bRuntimeActionTagsApplied = false;
	ActiveMontage = nullptr;
	BoundAnimInstance = nullptr;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !SprintAttackMontage || !CostGameplayEffectClass
		|| !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass || !SprintStateTag.IsValid() || !AttackingStateTag.IsValid()
		|| !MovementInputBlockedTag.IsValid() || !JumpInputBlockedTag.IsValid() || !StaminaRegenBlockedTag.IsValid() || !HitEventTag.IsValid()
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid()
		|| !AbilitySystemComponent->HasMatchingGameplayTag(SprintStateTag) || !PlayerCharacter->ShouldRequestSprintAttack())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': active grounded Sprint, montage, cost/damage/regen effects, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SprintAttackMontage);
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);
	if (!MontageTask || !HitEventTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask)
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

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SprintAttackMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &USprintAttackAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &USprintAttackAbility::OnActiveMontageEnded);
	HitEventTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnHitEventReceived);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &USprintAttackAbility::OnDodgeCancelWindowEnd);

	HitEventTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// Montage startup can synchronously invoke the bound end delegate. That path has already cleaned every task and pointer.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Sprint attack activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(SprintAttackMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SetRuntimeActionTags(true);
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
	SetDodgeCancelable(false);
	SetRuntimeActionTags(false);

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

	if (HitEventTask)
	{
		HitEventTask->EndTask();
		HitEventTask = nullptr;
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

	bHitEventConsumed = false;
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

void USprintAttackAbility::OnHitEventReceived(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload) || bHitEventConsumed)
	{
		return;
	}

	bHitEventConsumed = true;
	PerformHitTrace();
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

void USprintAttackAbility::PerformHitTrace()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !SourceAbilitySystemComponent || !World)
	{
		return;
	}

	const FVector Start = AvatarActor->GetActorLocation() + FVector(0.0f, 0.0f, TraceHeightOffset);
	const FVector End = Start + AvatarActor->GetActorForwardVector() * TraceDistance;
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(TraceRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, Start, End, FQuat::Identity, ECC_Pawn, CollisionShape, QueryParams);

	ABaseCharacter* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const FHitResult& HitResult : HitResults)
	{
		ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(HitResult.GetActor());
		if (!TargetCharacter || TargetCharacter == AvatarActor)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Start, TargetCharacter->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestTarget = TargetCharacter;
			NearestDistanceSquared = DistanceSquared;
		}
	}

	if (!NearestTarget)
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystemComponent = NearestTarget->GetAbilitySystemComponent();
	if (!TargetAbilitySystemComponent)
	{
		return;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = SourceAbilitySystemComponent->MakeOutgoingSpec(
		DamageGameplayEffectClass,
		GetAbilityLevel(),
		SourceAbilitySystemComponent->MakeEffectContext());
	if (DamageSpecHandle.IsValid() && DamageSpecHandle.Data.IsValid())
	{
		TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
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

		if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystemComponent->AddLooseGameplayTag(DodgeCancelableStateTag);
			bDodgeCancelable = true;
		}
		return;
	}

	if (!bDodgeCancelable)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(DodgeCancelableStateTag);
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
