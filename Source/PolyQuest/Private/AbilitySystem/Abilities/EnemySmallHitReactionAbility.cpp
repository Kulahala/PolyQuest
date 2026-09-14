#include "AbilitySystem/Abilities/EnemySmallHitReactionAbility.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PolyQuest.h"

UEnemySmallHitReactionAbility::UEnemySmallHitReactionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRetriggerInstancedAbility = true;

	SmallHitReactionAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);
	SmallHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Small")), false);
	SmallHitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);

	AbilityTags.AddTag(SmallHitReactionAbilityTag);
	ActivationOwnedTags.AddTag(SmallHitReactingStateTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = SmallHitReactionEventTag;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

bool UEnemySmallHitReactionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ValidateActivationSetup(ActorInfo);
}

void UEnemySmallHitReactionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityRequested = false;

	if (KnockbackTask)
	{
		KnockbackTask->EndTask();
		KnockbackTask = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	if (!CharacterASC || !EnemyCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': ASC, living enemy, AnimInstance, complete four-way montages (Front, Back, Left, Right), and required tags are required."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector LocalImpactDirection = TriggerEventData
		? FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, EnemyCharacter)
		: FVector::ZeroVector;
	const float EnemyYaw = EnemyCharacter->GetActorRotation().Yaw;
	FVector KnockbackWorldDirection = FVector::ZeroVector;
	if (FMath::IsFinite(LocalImpactDirection.X) && FMath::IsFinite(LocalImpactDirection.Y)
		&& FMath::IsFinite(EnemyYaw))
	{
		const FVector LocalKnockbackDirection(-LocalImpactDirection.X, -LocalImpactDirection.Y, 0.0f);
		if (!LocalKnockbackDirection.IsNearlyZero())
		{
			KnockbackWorldDirection = LocalKnockbackDirection.RotateAngleAxis(EnemyYaw, FVector::UpVector);
			if (!FMath::IsFinite(KnockbackWorldDirection.X) || !FMath::IsFinite(KnockbackWorldDirection.Y))
			{
				KnockbackWorldDirection = FVector::ZeroVector;
			}
		}
	}

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontSmallHitReactionMontage;
	MontageSet.Back = BackSmallHitReactionMontage;
	MontageSet.Left = LeftSmallHitReactionMontage;
	MontageSet.Right = RightSmallHitReactionMontage;

	UAnimMontage* SelectedMontage = FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(
		LocalImpactDirection, MontageSet);

	if (!SelectedMontage)
	{
		// Missing impact geometry suppresses movement, not the non-blocking hit presentation.
		SelectedMontage = MontageSet.Front;
	}

	UAbilityTask_PlayActionMontage* CreatedMontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this,
		NAME_None,
		SelectedMontage,
		1.0f,
		NAME_None,
		1.0f,
		0.0f,
		true);
	if (!CreatedMontageTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = CreatedMontageTask;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Enemy small hit reaction activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(EnemyCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bEndAbilityRequested || MontageTask.Get() != CreatedMontageTask || !IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished())
	{
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = SelectedMontage;

	MontageTask->OnCompleted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);

	MontageTask->ReadyForActivation();

	if (bEndAbilityRequested)
	{
		return;
	}

	if (MontageTask.Get() != CreatedMontageTask || !IsValid(CreatedMontageTask) || CreatedMontageTask->IsFinished() || !CreatedMontageTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageActive = bTestBypassMontageActiveCheck || (BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()));
#else
	const bool bMontageActive = BoundAnimInstance && ActiveMontage && BoundAnimInstance->Montage_IsActive(ActiveMontage.Get());
#endif

	if (!BoundAnimInstance || !ActiveMontage || !bMontageActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy small hit reaction activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(EnemyCharacter), *GetNameSafe(ActiveMontage.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UCharacterMovementComponent* MovementComponent = EnemyCharacter->GetCharacterMovement();
	UCurveFloat* FalloffCurve = KnockbackFalloffCurve.Get();
	const bool bKnockbackCurveValid = IsValid(FalloffCurve)
		&& FalloffCurve->FloatCurve.GetNumKeys() > 0
		&& FMath::IsFinite(FalloffCurve->GetFloatValue(0.0f))
		&& FMath::IsFinite(FalloffCurve->GetFloatValue(1.0f));
	const bool bKnockbackConfigValid = FMath::IsFinite(KnockbackDistance)
		&& KnockbackDistance > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(KnockbackDuration) && KnockbackDuration > 0.0f
		&& bKnockbackCurveValid
		&& !KnockbackWorldDirection.IsNearlyZero()
		&& FMath::IsFinite(KnockbackWorldDirection.X)
		&& FMath::IsFinite(KnockbackWorldDirection.Y)
		&& FMath::IsFinite(KnockbackWorldDirection.Z);
	if (MovementComponent && MovementComponent->IsMovingOnGround()
		&& !EnemyCharacter->IsPlayingRootMotion() && !EnemyCharacter->HasAnyRootMotion()
		&& bKnockbackConfigValid)
	{
		const float KnockbackStrength = 2.0f * KnockbackDistance / KnockbackDuration;
		if (!FMath::IsFinite(KnockbackStrength))
		{
			return;
		}

		UAbilityTask_ApplyRootMotionConstantForce* CreatedKnockbackTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			FName(TEXT("EnemySmallHitReactionKnockback")),
			KnockbackWorldDirection,
			KnockbackStrength,
			KnockbackDuration,
			false,
			FalloffCurve,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.0f,
			true);
		if (!CreatedKnockbackTask)
		{
			return;
		}

		KnockbackTask = CreatedKnockbackTask;
		CreatedKnockbackTask->ReadyForActivation();

		if (bEndAbilityRequested || KnockbackTask.Get() != CreatedKnockbackTask || !IsValid(CreatedKnockbackTask))
		{
			return;
		}
	}
}

void UEnemySmallHitReactionAbility::EndAbility(
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

	if (KnockbackTask)
	{
		KnockbackTask->EndTask();
		KnockbackTask = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemySmallHitReactionAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void UEnemySmallHitReactionAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void UEnemySmallHitReactionAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

bool UEnemySmallHitReactionAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = EnemyCharacter ? EnemyCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	FHitReactionFourWayMontageSet MontageSet;
	MontageSet.Front = FrontSmallHitReactionMontage;
	MontageSet.Back = BackSmallHitReactionMontage;
	MontageSet.Left = LeftSmallHitReactionMontage;
	MontageSet.Right = RightSmallHitReactionMontage;

	return CharacterASC && EnemyCharacter && !EnemyCharacter->IsDead() && AnimInstance && MontageSet.IsComplete()
		&& SmallHitReactionAbilityTag.IsValid() && SmallHitReactionEventTag.IsValid() && SmallHitReactingStateTag.IsValid()
		&& StunnedStateTag.IsValid() && DeadStateTag.IsValid();
}

void UEnemySmallHitReactionAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UEnemySmallHitReactionAbility::TestBindTaskCallbacks(UAbilityTask_PlayActionMontage* InTask)
{
	if (InTask)
	{
		InTask->OnCompleted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		InTask->OnInterrupted.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		InTask->OnCancelled.AddDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
	}
}

void UEnemySmallHitReactionAbility::TestUnbindTaskCallbacks(UAbilityTask_PlayActionMontage* InTask)
{
	if (InTask)
	{
		InTask->OnCompleted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCompleted);
		InTask->OnInterrupted.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageInterrupted);
		InTask->OnCancelled.RemoveDynamic(this, &UEnemySmallHitReactionAbility::OnMontageCancelled);
	}
}

const FAbilityMontageRateWindowLifecycle& UEnemySmallHitReactionAbility::GetTestRateWindowLifecycle() const
{
	static const FAbilityMontageRateWindowLifecycle EmptyLifecycle;
	return MontageTask ? MontageTask->GetRateWindowLifecycle() : EmptyLifecycle;
}

int32 UEnemySmallHitReactionAbility::GetTestActiveMontageInstanceID() const
{
	return MontageTask ? MontageTask->GetBoundMontageInstanceID() : INDEX_NONE;
}
#endif
