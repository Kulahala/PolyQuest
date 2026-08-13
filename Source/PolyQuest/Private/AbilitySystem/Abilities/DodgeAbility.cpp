#include "AbilitySystem/Abilities/DodgeAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

UDodgeAbility::UDodgeAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Dodge")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));

	PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	LightAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false);
	ChargedAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false);
	SprintAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	InvulnerabilityBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Dodge.Invulnerability.Begin")), false);
	InvulnerabilityEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Dodge.Invulnerability.End")), false);
}

bool UDodgeAbility::CanActivateAbility(
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
	if (!AbilitySystemComponent || !MovementComponent || !MovementComponent->IsMovingOnGround())
	{
		return false;
	}

	const bool bIsAttacking = AttackingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(AttackingStateTag);
	const bool bCanCancelAttack = DodgeCancelableStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DodgeCancelableStateTag);
	const bool bIsCharging = ChargingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ChargingStateTag);
	return !bIsAttacking || bCanCancelAttack || bIsCharging;
}

void UDodgeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	InvulnerabilityEffectHandle.Invalidate();

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	const FVector DodgeDirection = PlayerCharacter ? PlayerCharacter->GetDodgeWorldDirection() : FVector::ZeroVector;

	if (!AbilitySystemComponent || !PlayerCharacter || !AnimInstance || !DodgeMontage || !CostGameplayEffectClass
		|| !StaminaRegenDelayGameplayEffectClass || !InvulnerabilityGameplayEffectClass || !PrimaryAttackAbilityTag.IsValid()
		|| !LightAttackAbilityTag.IsValid() || !ChargedAttackAbilityTag.IsValid() || !SprintAttackAbilityTag.IsValid() || !AttackingStateTag.IsValid()
		|| !ChargingStateTag.IsValid() || !DodgeCancelableStateTag.IsValid() || !InvulnerabilityBeginEventTag.IsValid() || !InvulnerabilityEndEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': ASC, player, AnimInstance, montage, cost, regeneration delay, invulnerability effect, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DodgeMontage);
	InvulnerabilityBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InvulnerabilityBeginEventTag, nullptr, true, true);
	InvulnerabilityEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InvulnerabilityEndEventTag, nullptr, true, true);
	if (!MontageTask || !InvulnerabilityBeginTask || !InvulnerabilityEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Dodge activation rejected for '%s' because CommitAbility failed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bCanCancelAttack = AbilitySystemComponent->HasMatchingGameplayTag(DodgeCancelableStateTag);
	const bool bWasCharging = AbilitySystemComponent->HasMatchingGameplayTag(ChargingStateTag);
	FGameplayTagContainer AbilityTagsToCancel;
	AbilityTagsToCancel.AddTag(PrimaryAttackAbilityTag);
	if (bCanCancelAttack || bWasCharging)
	{
		AbilityTagsToCancel.AddTag(LightAttackAbilityTag);
		AbilityTagsToCancel.AddTag(ChargedAttackAbilityTag);
		AbilityTagsToCancel.AddTag(SprintAttackAbilityTag);
	}
	AbilitySystemComponent->CancelAbilities(&AbilityTagsToCancel, nullptr, this);

	if (!DodgeDirection.IsNearlyZero())
	{
		PlayerCharacter->SetActorRotation(DodgeDirection.Rotation());
	}

	MontageTask->OnCompleted.AddDynamic(this, &UDodgeAbility::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UDodgeAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UDodgeAbility::OnMontageCancelled);
	InvulnerabilityBeginTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnInvulnerabilityBegin);
	InvulnerabilityEndTask->EventReceived.AddDynamic(this, &UDodgeAbility::OnInvulnerabilityEnd);

	InvulnerabilityBeginTask->ReadyForActivation();
	InvulnerabilityEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

void UDodgeAbility::EndAbility(
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
	ClearInvulnerabilityEffect();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (InvulnerabilityBeginTask)
	{
		InvulnerabilityBeginTask->EndTask();
		InvulnerabilityBeginTask = nullptr;
	}

	if (InvulnerabilityEndTask)
	{
		InvulnerabilityEndTask->EndTask();
		InvulnerabilityEndTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UDodgeAbility::OnMontageCompleted()
{
	EndFromMontage(false);
}

void UDodgeAbility::OnMontageInterrupted()
{
	EndFromMontage(true);
}

void UDodgeAbility::OnMontageCancelled()
{
	EndFromMontage(true);
}

void UDodgeAbility::OnInvulnerabilityBegin(FGameplayEventData)
{
	if (bEndAbilityRequested || InvulnerabilityEffectHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* InvulnerabilityEffect = InvulnerabilityGameplayEffectClass
		? InvulnerabilityGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!AbilitySystemComponent || !InvulnerabilityEffect)
	{
		return;
	}

	InvulnerabilityEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
		InvulnerabilityEffect,
		GetAbilityLevel(),
		AbilitySystemComponent->MakeEffectContext());

	if (!InvulnerabilityEffectHandle.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Dodge invulnerability failed to apply for '%s'."), *GetNameSafe(GetAvatarActorFromActorInfo()));
	}
}

void UDodgeAbility::OnInvulnerabilityEnd(FGameplayEventData)
{
	ClearInvulnerabilityEffect();
}

void UDodgeAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UDodgeAbility::ClearInvulnerabilityEffect()
{
	if (!InvulnerabilityEffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(InvulnerabilityEffectHandle);
	}

	InvulnerabilityEffectHandle.Invalidate();
}
