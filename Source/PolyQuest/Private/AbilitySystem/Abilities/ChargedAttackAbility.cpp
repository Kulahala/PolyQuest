#include "AbilitySystem/Abilities/ChargedAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
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

UChargedAttackAbility::UChargedAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Resource.Stamina.RegenBlocked")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	ChargedReleaseHandoffEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.ReleaseHandoff")), false);
	HoldReadyEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.HoldReady")), false);
	HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.Hit")), false);
	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	DamageDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage.Charged")), false);

	FAbilityTriggerData ChargedReleaseHandoffTrigger;
	ChargedReleaseHandoffTrigger.TriggerTag = ChargedReleaseHandoffEventTag;
	ChargedReleaseHandoffTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(ChargedReleaseHandoffTrigger);
}

bool UChargedAttackAbility::CanActivateAbility(
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

	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const bool bPrimaryInputHeld = PlayerCharacter && PrimaryAttackInputTag.IsValid() && PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag);
	const bool bReleasedPrimaryHandoff = SourceTags && PrimaryAttackInputTag.IsValid() && SourceTags->HasTagExact(PrimaryAttackInputTag);
	return PlayerCharacter && (bPrimaryInputHeld || bReleasedPrimaryHandoff);
}

bool UChargedAttackAbility::ShouldAbilityRespondToEvent(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return IsChargedReleaseHandoffEvent(Payload, AvatarActor);
}

void UChargedAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;
	bHitEventConsumed = false;
	bDodgeCancelable = false;
	bChargingStateApplied = false;
	bMontagePausedAtHoldReady = false;
	bReleaseStarted = false;
	DamageMultiplier = 1.0f;
	ActiveMontage = nullptr;
	BoundAnimInstance = nullptr;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const bool bReleasedPrimaryHandoff = IsChargedReleaseHandoffEvent(TriggerEventData, PlayerCharacter);

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !ChargedAttackMontage
		|| !CostGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass || !PrimaryAttackInputTag.IsValid()
		|| !InputReleasedEventTag.IsValid() || !InputCanceledEventTag.IsValid() || !ChargedReleaseHandoffEventTag.IsValid() || !HoldReadyEventTag.IsValid() || !HitEventTag.IsValid()
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid()
		|| !ChargingStateTag.IsValid() || !DamageDataTag.IsValid() || MinimumChargeDuration > MaximumChargeDuration
		|| MaximumDamageMultiplier < 1.0f || (!bReleasedPrimaryHandoff && !PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag)))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': held input, montage, cost/damage/regen effects, valid timing values, and required gameplay tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ChargedAttackMontage);
	HoldReadyTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HoldReadyEventTag, nullptr, false, true);
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false, true);
	InputReleasedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputReleasedEventTag, nullptr, false, true);
	InputCanceledTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputCanceledEventTag, nullptr, false, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, false, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, false, true);

	if (!MontageTask || !HoldReadyTask || !HitEventTask || !InputReleasedTask || !InputCanceledTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = ChargedAttackMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UChargedAttackAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UChargedAttackAbility::OnActiveMontageEnded);

	HoldReadyTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnHoldReady);
	HitEventTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnHitEventReceived);
	InputReleasedTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnInputReleased);
	InputCanceledTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnInputCanceled);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &UChargedAttackAbility::OnDodgeCancelWindowEnd);

	HoldReadyTask->ReadyForActivation();
	HitEventTask->ReadyForActivation();
	InputReleasedTask->ReadyForActivation();
	InputCanceledTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// A zero-length or otherwise immediately completed Montage can synchronously run EndAbility.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(ChargedAttackMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bReleasedPrimaryHandoff)
	{
		BeginRelease(TriggerEventData->EventMagnitude);
	}
	else
	{
		SetCharging(true);
	}
}

void UChargedAttackAbility::EndAbility(
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
	SetCharging(false);
	SetDodgeCancelable(false);

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UChargedAttackAbility::OnActiveMontageEnded);
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

	if (HoldReadyTask)
	{
		HoldReadyTask->EndTask();
		HoldReadyTask = nullptr;
	}

	if (HitEventTask)
	{
		HitEventTask->EndTask();
		HitEventTask = nullptr;
	}

	if (InputReleasedTask)
	{
		InputReleasedTask->EndTask();
		InputReleasedTask = nullptr;
	}

	if (InputCanceledTask)
	{
		InputCanceledTask->EndTask();
		InputCanceledTask = nullptr;
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
	bMontagePausedAtHoldReady = false;
	bReleaseStarted = false;
	DamageMultiplier = 1.0f;
	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UChargedAttackAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void UChargedAttackAbility::OnHoldReady(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload) || bReleaseStarted || bMontagePausedAtHoldReady)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		EndFromMontage(true);
		return;
	}

	BoundAnimInstance->Montage_Pause(ActiveMontage.Get());
	bMontagePausedAtHoldReady = true;
}

void UChargedAttackAbility::OnHitEventReceived(FGameplayEventData Payload)
{
	if (!bReleaseStarted || !IsGameplayEventFromActiveMontage(Payload) || bHitEventConsumed)
	{
		return;
	}

	bHitEventConsumed = true;
	PerformHitTrace();
}

void UChargedAttackAbility::OnInputReleased(FGameplayEventData Payload)
{
	if (IsPrimaryAttackInputEvent(Payload))
	{
		BeginRelease(Payload.EventMagnitude);
	}
}

void UChargedAttackAbility::OnInputCanceled(FGameplayEventData Payload)
{
	if (IsPrimaryAttackInputEvent(Payload))
	{
		EndFromMontage(true);
	}
}

void UChargedAttackAbility::OnDodgeCancelWindowBegin(FGameplayEventData Payload)
{
	if (bReleaseStarted && IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(true);
	}
}

void UChargedAttackAbility::OnDodgeCancelWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(false);
	}
}

void UChargedAttackAbility::BeginRelease(float HeldDuration)
{
	if (bEndAbilityRequested || bReleaseStarted || !CurrentActorInfo)
	{
		return;
	}

	SetCharging(false);

	if (!CheckCost(CurrentSpecHandle, CurrentActorInfo, nullptr))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Charged attack release rejected for '%s' because its Stamina cost cannot be paid."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		EndFromMontage(true);
		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack release failed to commit for '%s'."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		EndFromMontage(true);
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Charged attack release failed for '%s': active montage '%s' is unavailable."), *GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(ActiveMontage));
		EndFromMontage(true);
		return;
	}

	const float ChargeRange = MaximumChargeDuration - MinimumChargeDuration;
	const float ChargeAlpha = ChargeRange > KINDA_SMALL_NUMBER
		? FMath::Clamp((HeldDuration - MinimumChargeDuration) / ChargeRange, 0.0f, 1.0f)
		: 1.0f;
	DamageMultiplier = FMath::Lerp(1.0f, MaximumDamageMultiplier, ChargeAlpha);
	bReleaseStarted = true;

	if (bMontagePausedAtHoldReady)
	{
		BoundAnimInstance->Montage_Resume(ActiveMontage.Get());
		bMontagePausedAtHoldReady = false;
	}
}

void UChargedAttackAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

bool UChargedAttackAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && ActiveMontage && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.OptionalObject.Get() == ActiveMontage.Get();
}

bool UChargedAttackAbility::IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.InstigatorTags.HasTagExact(PrimaryAttackInputTag);
}

bool UChargedAttackAbility::IsChargedReleaseHandoffEvent(const FGameplayEventData* Payload, const AActor* AvatarActor) const
{
	return Payload && AvatarActor && ChargedReleaseHandoffEventTag.IsValid()
		&& Payload->EventTag.MatchesTagExact(ChargedReleaseHandoffEventTag)
		&& Payload->Instigator == AvatarActor && Payload->Target == AvatarActor
		&& Payload->InstigatorTags.HasTagExact(PrimaryAttackInputTag);
}

void UChargedAttackAbility::PerformHitTrace()
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
		DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, -BaseDamage * DamageMultiplier);
		TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
	}
}

void UChargedAttackAbility::SetCharging(bool bShouldCharge)
{
	if (bShouldCharge)
	{
		if (bEndAbilityRequested || bChargingStateApplied)
		{
			return;
		}

		if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystemComponent->AddLooseGameplayTag(ChargingStateTag);
			bChargingStateApplied = true;
		}
		return;
	}

	if (!bChargingStateApplied)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(ChargingStateTag);
	}

	bChargingStateApplied = false;
}

void UChargedAttackAbility::SetDodgeCancelable(bool bShouldBeCancelable)
{
	if (bShouldBeCancelable)
	{
		if (bEndAbilityRequested || bDodgeCancelable)
		{
			return;
		}

		UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
		if (!AbilitySystemComponent || !DodgeCancelableStateTag.IsValid())
		{
			return;
		}

		AbilitySystemComponent->AddLooseGameplayTag(DodgeCancelableStateTag);
		bDodgeCancelable = true;
		return;
	}

	if (!bDodgeCancelable)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		if (DodgeCancelableStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(DodgeCancelableStateTag);
		}
	}

	bDodgeCancelable = false;
}
