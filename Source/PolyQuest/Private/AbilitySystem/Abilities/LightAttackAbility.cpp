#include "AbilitySystem/Abilities/LightAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

ULightAttackAbility::ULightAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Hit")), false);
	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
}

void ULightAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	USkeletalMeshComponent* SkeletalMesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!AbilitySystemComponent || !AnimInstance || !AttackMontage || !CostGameplayEffectClass || !DamageGameplayEffectClass || !StaminaRegenDelayGameplayEffectClass || !HitEventTag.IsValid()
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid() || !DodgeCancelableStateTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': ASC, AnimInstance, montage, cost effect, damage effect, Stamina regeneration delay effect, hit event tag, and Dodge cancel tags are required."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bHitEventConsumed = false;
	bDodgeCancelable = false;
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, true, true);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag, nullptr, true, true);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag, nullptr, true, true);

	if (!MontageTask || !HitEventTask || !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Light attack activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ULightAttackAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ULightAttackAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ULightAttackAbility::OnMontageCancelled);
	HitEventTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnHitEventReceived);
	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnDodgeCancelWindowEnd);

	HitEventTask->ReadyForActivation();
	DodgeCancelWindowBeginTask->ReadyForActivation();
	DodgeCancelWindowEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

void ULightAttackAbility::EndAbility(
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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULightAttackAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void ULightAttackAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void ULightAttackAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

void ULightAttackAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void ULightAttackAbility::OnHitEventReceived(FGameplayEventData)
{
	if (bEndAbilityRequested || bHitEventConsumed)
	{
		return;
	}

	bHitEventConsumed = true;
	PerformHitTrace();
}

void ULightAttackAbility::OnDodgeCancelWindowBegin(FGameplayEventData)
{
	if (!bEndAbilityRequested)
	{
		SetDodgeCancelable(true);
	}
}

void ULightAttackAbility::OnDodgeCancelWindowEnd(FGameplayEventData)
{
	SetDodgeCancelable(false);
}

void ULightAttackAbility::SetDodgeCancelable(bool bShouldBeCancelable)
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

void ULightAttackAbility::PerformHitTrace()
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
