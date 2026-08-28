#include "AbilitySystem/Abilities/PlayerGuardAbility.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "PolyQuest.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float GuardHalfArcDegrees = 60.0f;
}

UPlayerGuardAbility::UPlayerGuardAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	GuardAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false);
	GuardingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
	GuardInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Guard")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	DefenseCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
	GuardStaminaDamageDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Stamina.GuardDamage")), false);
	GuardBreakEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.GuardBreak")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

	AbilityTags.AddTag(GuardAbilityTag);
	ActivationOwnedTags.AddTag(GuardingStateTag);
	ActivationBlockedTags.AddTag(GuardingStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));

	CancelableMeleeAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false));
	CancelableMeleeAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
	CancelableMeleeAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false));
	CancelableMeleeAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false));
	CancelableMeleeAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Skill.Melee")), false));
}

bool UPlayerGuardAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !ValidateActivationSetup(ActorInfo))
	{
		return false;
	}

	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CharacterASC || !PlayerCharacter || !PlayerCharacter->CanAttemptGuard())
	{
		return false;
	}

	const bool bIsAttacking = AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag);
	const bool bCanCancelAttack = DefenseCancelableStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DefenseCancelableStateTag);
	return !bIsAttacking || bCanCancelAttack;
}

void UPlayerGuardAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	bGuardActive = false;
	GuardMoveSpeedEffectHandle.Invalidate();
	GuardStaminaRegenMultiplierEffectHandle.Invalidate();
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo)
		|| !GuardInputTag.IsValid() || !PlayerCharacter->IsCombatInputHeld(GuardInputTag))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard activation aborted for '%s': active held input, grounded player, montage, effects, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardMontage);
	InputReleasedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputReleasedEventTag, nullptr, false, true);
	InputCanceledTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputCanceledEventTag, nullptr, false, true);
	if (!MontageTask || !InputReleasedTask || !InputCanceledTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = GuardMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerGuardAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerGuardAbility::OnActiveMontageEnded);
	InputReleasedTask->EventReceived.AddDynamic(this, &UPlayerGuardAbility::OnInputReleased);
	InputCanceledTask->EventReceived.AddDynamic(this, &UPlayerGuardAbility::OnInputCanceled);
	InputReleasedTask->ReadyForActivation();
	InputCanceledTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(GuardMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ApplyGuardEffects())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard activation aborted for '%s': failed to apply a required Guard GameplayEffect."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bGuardActive = true;
	PlayerCharacter->CancelSprintAbility();
	if (DefenseCancelableStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DefenseCancelableStateTag))
	{
		CharacterASC->CancelAbilities(&CancelableMeleeAbilityTags, nullptr, this);
	}
}

void UPlayerGuardAbility::EndAbility(
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
	bGuardActive = false;
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	ClearGuardEffects();

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerGuardAbility::OnActiveMontageEnded);
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

	ActiveMontage = nullptr;
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	const bool bShouldClearResume = !PlayerCharacter || !GuardInputTag.IsValid() || !PlayerCharacter->IsCombatInputHeld(GuardInputTag)
		|| (DeadStateTag.IsValid() && CharacterASC && CharacterASC->HasMatchingGameplayTag(DeadStateTag))
		|| (StunnedStateTag.IsValid() && CharacterASC && CharacterASC->HasMatchingGameplayTag(StunnedStateTag));
	if (PlayerCharacter && bShouldClearResume)
	{
		PlayerCharacter->ClearGuardResumeEligibility();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UPlayerGuardAbility::TryGuardMeleeHit(AActor* AttackingActor, float GuardStaminaDamage, const FHitResult& HitResult)
{
	if (!IsGuardActive() || !AttackingActor || !IsAttackerInGuardArc(AttackingActor))
	{
		return false;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	const UGameplayEffect* GuardStaminaCostEffect = GuardStaminaCostGameplayEffectClass
		? GuardStaminaCostGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !PlayerCharacter || !GuardStaminaCostEffect || !GuardStaminaDamageDataTag.IsValid()
		|| !GuardBreakEventTag.IsValid() || !StunnedStateTag.IsValid()
		|| CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f
		|| CharacterASC->HasMatchingGameplayTag(StunnedStateTag))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = CharacterASC->MakeEffectContext();
	EffectContext.AddSourceObject(AttackingActor);
	const FGameplayEffectSpecHandle GuardStaminaCostSpecHandle = CharacterASC->MakeOutgoingSpec(
		GuardStaminaCostGameplayEffectClass,
		GetAbilityLevel(),
		EffectContext);
	if (!GuardStaminaCostSpecHandle.IsValid() || !GuardStaminaCostSpecHandle.Data.IsValid())
	{
		return false;
	}

	GuardStaminaCostSpecHandle.Data->SetSetByCallerMagnitude(GuardStaminaDamageDataTag, -FMath::Max(GuardStaminaDamage, 0.0f));
	if (!CharacterASC->ApplyGameplayEffectSpecToSelf(*GuardStaminaCostSpecHandle.Data.Get()).WasSuccessfullyApplied())
	{
		return false;
	}

	TriggerGuardSuccessFeedback(HitResult);

	ApplyStaminaRegenDelay();
	if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) > 0.0f)
	{
		return true;
	}

	PlayerCharacter->ClearGuardResumeEligibility();
	FGameplayEventData GuardBreakEventData;
	GuardBreakEventData.EventTag = GuardBreakEventTag;
	GuardBreakEventData.Instigator = AttackingActor;
	GuardBreakEventData.Target = PlayerCharacter;
	if (CharacterASC->HandleGameplayEvent(GuardBreakEventTag, &GuardBreakEventData) <= 0)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player '%s' reached zero Stamina while Guarding but no Guard Break Ability accepted the event; the resolving hit stays absorbed."), *GetNameSafe(PlayerCharacter));
	}
	EndFromMontage(true);
	return true;
}

void UPlayerGuardAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	EndFromMontage(bInterrupted);
}

void UPlayerGuardAbility::OnInputReleased(FGameplayEventData Payload)
{
	if (!IsGuardInputEvent(Payload))
	{
		return;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->ClearGuardResumeEligibility();
	}
	EndFromMontage(false);
}

void UPlayerGuardAbility::OnInputCanceled(FGameplayEventData Payload)
{
	if (!IsGuardInputEvent(Payload))
	{
		return;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->ClearGuardResumeEligibility();
	}
	EndFromMontage(true);
}

bool UPlayerGuardAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	return CharacterASC && PlayerCharacter && MovementComponent && MovementComponent->IsMovingOnGround() && AnimInstance && GuardMontage
		&& GuardMoveSpeedGameplayEffectClass && GuardStaminaRegenMultiplierGameplayEffectClass && GuardStaminaCostGameplayEffectClass
		&& StaminaRegenDelayGameplayEffectClass && GuardAbilityTag.IsValid() && GuardingStateTag.IsValid() && GuardInputTag.IsValid()
		&& InputReleasedEventTag.IsValid() && InputCanceledEventTag.IsValid() && AttackingStateTag.IsValid() && DefenseCancelableStateTag.IsValid()
		&& GuardStaminaDamageDataTag.IsValid() && GuardBreakEventTag.IsValid() && DeadStateTag.IsValid() && StunnedStateTag.IsValid()
		&& CancelableMeleeAbilityTags.Num() == 5;
}

bool UPlayerGuardAbility::ApplyGuardEffects()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* GuardMoveSpeedEffect = GuardMoveSpeedGameplayEffectClass
		? GuardMoveSpeedGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	const UGameplayEffect* GuardStaminaRegenMultiplierEffect = GuardStaminaRegenMultiplierGameplayEffectClass
		? GuardStaminaRegenMultiplierGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !GuardMoveSpeedEffect || !GuardStaminaRegenMultiplierEffect)
	{
		return false;
	}

	GuardMoveSpeedEffectHandle = CharacterASC->ApplyGameplayEffectToSelf(
		GuardMoveSpeedEffect,
		GetAbilityLevel(),
		CharacterASC->MakeEffectContext());
	GuardStaminaRegenMultiplierEffectHandle = CharacterASC->ApplyGameplayEffectToSelf(
		GuardStaminaRegenMultiplierEffect,
		GetAbilityLevel(),
		CharacterASC->MakeEffectContext());
	if (GuardMoveSpeedEffectHandle.IsValid() && GuardStaminaRegenMultiplierEffectHandle.IsValid())
	{
		return true;
	}

	ClearGuardEffects();
	return false;
}

void UPlayerGuardAbility::ClearGuardEffects()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	if (CharacterASC && GuardMoveSpeedEffectHandle.IsValid())
	{
		CharacterASC->RemoveActiveGameplayEffect(GuardMoveSpeedEffectHandle);
	}
	if (CharacterASC && GuardStaminaRegenMultiplierEffectHandle.IsValid())
	{
		CharacterASC->RemoveActiveGameplayEffect(GuardStaminaRegenMultiplierEffectHandle);
	}

	GuardMoveSpeedEffectHandle.Invalidate();
	GuardStaminaRegenMultiplierEffectHandle.Invalidate();
}

void UPlayerGuardAbility::ApplyStaminaRegenDelay()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* StaminaRegenDelayEffect = StaminaRegenDelayGameplayEffectClass
		? StaminaRegenDelayGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !StaminaRegenDelayEffect)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard could not refresh Stamina regeneration delay for '%s'."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		return;
	}

	const FActiveGameplayEffectHandle RegenDelayEffectHandle = CharacterASC->ApplyGameplayEffectToSelf(
		StaminaRegenDelayEffect,
		GetAbilityLevel(),
		CharacterASC->MakeEffectContext());
	if (!RegenDelayEffectHandle.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Guard failed to apply Stamina regeneration delay for '%s'."), *GetNameSafe(GetAvatarActorFromActorInfo()));
	}
}

bool UPlayerGuardAbility::IsGuardInputEvent(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.InstigatorTags.HasTagExact(GuardInputTag);
}

bool UPlayerGuardAbility::IsAttackerInGuardArc(const AActor* AttackingActor) const
{
	const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || !AttackingActor)
	{
		return false;
	}

	const FVector ToAttacker = (AttackingActor->GetActorLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal2D();
	const FVector GuardForward = PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (ToAttacker.IsNearlyZero() || GuardForward.IsNearlyZero())
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(GuardHalfArcDegrees));
	return FVector::DotProduct(GuardForward, ToAttacker) >= MinimumDot;
}

void UPlayerGuardAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UPlayerGuardAbility::TriggerGuardSuccessFeedback(const FHitResult& HitResult)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter)
	{
		return;
	}

	if (GuardSuccessSound)
	{
		FVector SoundLocation = PlayerCharacter->GetActorLocation();
		if (HitResult.GetActor() == PlayerCharacter
			&& !HitResult.ImpactPoint.ContainsNaN()
			&& FMath::IsFinite(HitResult.ImpactPoint.X) && FMath::IsFinite(HitResult.ImpactPoint.Y) && FMath::IsFinite(HitResult.ImpactPoint.Z)
			&& !HitResult.ImpactPoint.IsNearlyZero())
		{
			SoundLocation = HitResult.ImpactPoint;
		}

		if (SoundLocation.ContainsNaN()
			|| !FMath::IsFinite(SoundLocation.X) || !FMath::IsFinite(SoundLocation.Y) || !FMath::IsFinite(SoundLocation.Z))
		{
			return;
		}

		UWorld* World = PlayerCharacter->GetWorld();
		if (!World)
		{
			return;
		}

#if WITH_DEV_AUTOMATION_TESTS
		++TestGuardSuccessSoundDispatchCount;
		TestLastGuardSuccessSoundLocation = SoundLocation;
		if (bTestBypassAudioPlayback)
		{
			return;
		}
#endif

		UGameplayStatics::PlaySoundAtLocation(World, GuardSuccessSound, SoundLocation);
	}
}
