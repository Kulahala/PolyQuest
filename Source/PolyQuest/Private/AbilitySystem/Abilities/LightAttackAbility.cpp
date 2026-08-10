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
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Hit")), false);
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

	if (!AbilitySystemComponent || !AnimInstance || !AttackMontage || !CostGameplayEffectClass || !DamageGameplayEffectClass || !HitEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': ASC, AnimInstance, montage, cost effect, damage effect, and hit event tag are required."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bHitEventConsumed = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Light attack activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, true, true);

	if (!MontageTask || !HitEventTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ULightAttackAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ULightAttackAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ULightAttackAbility::OnMontageCancelled);
	HitEventTask->EventReceived.AddDynamic(this, &ULightAttackAbility::OnHitEventReceived);

	HitEventTask->ReadyForActivation();
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
	if (bHitEventConsumed)
	{
		return;
	}

	bHitEventConsumed = true;
	PerformHitTrace();
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
