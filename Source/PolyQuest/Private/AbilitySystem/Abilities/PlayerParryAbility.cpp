#include "AbilitySystem/Abilities/PlayerParryAbility.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Character/Player/PlayerCharacter.h"
#include "Combat/Feedback/CombatFeedbackDataAsset.h"
#include "Framework/PolyQuestPlayerController.h"
#include "PolyQuest.h"

namespace
{
	// Same front half-arc geometry as the D1 Guard: 120 degrees total.
	constexpr float ParryHalfArcDegrees = 60.0f;
}

UPlayerParryAbility::UPlayerParryAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ParryAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Parry")), false);
	ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	ParryActiveStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.ParryActive")), false);
	ParryWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Defense.Parry.Window.Begin")), false);
	ParryWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Defense.Parry.Window.End")), false);
	ParryPoiseDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Parry")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	DefenseCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

	AbilityTags.AddTag(ParryAbilityTag);
	ActivationOwnedTags.AddTag(ParryingStateTag);
	ActivationBlockedTags.AddTag(ParryingStateTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));

	DefenseCancelableAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false));
	DefenseCancelableAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
	DefenseCancelableAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false));
	DefenseCancelableAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false));
	DefenseCancelableAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false));
}

bool UPlayerParryAbility::CanActivateAbility(
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
	if (!CharacterASC)
	{
		return false;
	}

	const bool bIsAttacking = AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag);
	const bool bCanCancelAttack = DefenseCancelableStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DefenseCancelableStateTag);
	return !bIsAttacking || bCanCancelAttack;
}

void UPlayerParryAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	bEndAbilityRequested = false;
	bParryWindowOpen = false;
	bParryActiveTagApplied = false;
	bMovementLockedByParry = false;
	bMontageCompletedNaturally = false;
	BoundAnimInstance = nullptr;
	ActiveMontage = nullptr;

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
	if (!CharacterASC || !PlayerCharacter || !AnimInstance || !ValidateActivationSetup(ActorInfo))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Parry activation aborted for '%s': grounded living player, AnimInstance, montage, effects, and required tags are required."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ParryMontage);
	ParryWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ParryWindowBeginEventTag, nullptr, false, true);
	ParryWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ParryWindowEndEventTag, nullptr, false, true);
	if (!MontageTask || !ParryWindowBeginTask || !ParryWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Parry activation aborted for '%s': failed to create an AbilityTask."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitStaminaCostOnly(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("Player Parry activation rejected for '%s' because the Stamina cost could not be committed."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BoundAnimInstance = AnimInstance;
	ActiveMontage = ParryMontage;
	BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerParryAbility::OnActiveMontageEnded);
	BoundAnimInstance->OnMontageEnded.AddDynamic(this, &UPlayerParryAbility::OnActiveMontageEnded);
	ParryWindowBeginTask->EventReceived.AddDynamic(this, &UPlayerParryAbility::OnParryWindowBegin);
	ParryWindowEndTask->EventReceived.AddDynamic(this, &UPlayerParryAbility::OnParryWindowEnd);
	ParryWindowBeginTask->ReadyForActivation();
	ParryWindowEndTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	// A zero-length or otherwise immediately completed Montage can synchronously run teardown.
	if (bEndAbilityRequested)
	{
		return;
	}

	if (!BoundAnimInstance || !ActiveMontage || !BoundAnimInstance->Montage_IsActive(ActiveMontage.Get()))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Parry activation aborted for '%s': montage '%s' did not start."), *GetNameSafe(PlayerCharacter), *GetNameSafe(ParryMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		bMovementLockedByParry = true;
	}

	PlayerCharacter->CancelSprintAbility();
	// Routes through the shared Guard-cancellation helper so a still-held RMB earns
	// the one-shot resume after Parry ends; the helper cancels only a live Guard.
	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
	if (DefenseCancelableStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DefenseCancelableStateTag))
	{
		CharacterASC->CancelAbilities(&DefenseCancelableAbilityTags, nullptr, this);
	}
}

void UPlayerParryAbility::EndAbility(
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
	bParryWindowOpen = false;
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	if (bParryActiveTagApplied)
	{
		if (CharacterASC && ParryActiveStateTag.IsValid())
		{
			CharacterASC->RemoveLooseGameplayTag(ParryActiveStateTag);
		}
		bParryActiveTagApplied = false;
	}

	if (BoundAnimInstance)
	{
		BoundAnimInstance->OnMontageEnded.RemoveDynamic(this, &UPlayerParryAbility::OnActiveMontageEnded);
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

	if (ParryWindowBeginTask)
	{
		ParryWindowBeginTask->EndTask();
		ParryWindowBeginTask = nullptr;
	}

	if (ParryWindowEndTask)
	{
		ParryWindowEndTask->EndTask();
		ParryWindowEndTask = nullptr;
	}

	ActiveMontage = nullptr;

	// Restore walking only from this ability's own MOVE_None lock; an external
	// transition to Falling must win. MOVE_None is the only locked state that is
	// neither on the ground nor falling in this project's movement modes.
	if (bMovementLockedByParry && PlayerCharacter && !PlayerCharacter->IsActorBeingDestroyed()
		&& CharacterASC && DeadStateTag.IsValid() && !CharacterASC->HasMatchingGameplayTag(DeadStateTag))
	{
		UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement();
		if (MovementComponent && !MovementComponent->IsFalling() && !MovementComponent->IsMovingOnGround())
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}
	bMovementLockedByParry = false;

	if (bMontageCompletedNaturally)
	{
		bMontageCompletedNaturally = false;
		CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UPlayerParryAbility::TryParryMeleeHit(AActor* AttackingActor, const FHitResult& HitResult)
{
	if (!IsParryActive() || !AttackingActor || !IsAttackerInParryArc(AttackingActor))
	{
		return false;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* ParryCounterPoiseEffect = ParryCounterPoiseGameplayEffectClass
		? ParryCounterPoiseGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !ParryCounterPoiseEffect || !ParryPoiseDataTag.IsValid() || ParryPoiseDamage <= 0.0f)
	{
		return false;
	}

	if (UAbilitySystemComponent* AttackerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AttackingActor))
	{
		FGameplayEffectContextHandle EffectContext = CharacterASC->MakeEffectContext();
		EffectContext.AddSourceObject(AttackingActor);
		const FGameplayEffectSpecHandle ParryCounterSpecHandle = CharacterASC->MakeOutgoingSpec(
			ParryCounterPoiseGameplayEffectClass,
			GetAbilityLevel(),
			EffectContext);
		if (ParryCounterSpecHandle.IsValid() && ParryCounterSpecHandle.Data.IsValid())
		{
			ParryCounterSpecHandle.Data->SetSetByCallerMagnitude(ParryPoiseDataTag, -ParryPoiseDamage);
			AttackerASC->ApplyGameplayEffectSpecToSelf(*ParryCounterSpecHandle.Data.Get());
		}
	}

	// Trigger presentation feedback on successful parry contact.
	TriggerParrySuccessFeedback(HitResult);

	// The contact is consumed either way; a missing attacker ASC skips only the counter.
	return true;
}

void UPlayerParryAbility::TriggerParrySuccessFeedback(const FHitResult& HitResult)
{
#if WITH_DEV_AUTOMATION_TESTS
	++TestParrySuccessFeedbackCount;
#endif

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter)
	{
		return;
	}

	const UCombatFeedbackDataAsset* FeedbackData = PlayerCharacter->GetCombatFeedbackData();

	// 1. Request Hit-Stop via PlayerController
	if (FeedbackData)
	{
		const float HitStopDuration = FeedbackData->Defense.ParrySuccessHitStopDurationSeconds;
		const float HitStopTimeDilation = FeedbackData->Defense.ParrySuccessHitStopTimeDilation;

		if (APolyQuestPlayerController* PlayerController = Cast<APolyQuestPlayerController>(PlayerCharacter->GetController()))
		{
			if (FMath::IsFinite(HitStopDuration) && HitStopDuration > 0.0f
				&& FMath::IsFinite(HitStopTimeDilation) && HitStopTimeDilation > 0.0f && HitStopTimeDilation <= 1.0f)
			{
				PlayerController->RequestCombatImpactHitStop(HitStopDuration, HitStopTimeDilation);
			}
		}
	}

	// 2. Trigger Big camera shake via Player narrow wrapper
	PlayerCharacter->TriggerParrySuccessCameraShake();

	// 3. Play optional sound
	if (FeedbackData)
	{
		if (USoundBase* ParrySuccessSound = FeedbackData->Defense.ParrySuccessSound.Get())
		{
			FVector SoundLocation = PlayerCharacter->GetActorLocation();
			if (HitResult.GetActor() == PlayerCharacter
				&& !HitResult.ImpactPoint.ContainsNaN()
				&& FMath::IsFinite(HitResult.ImpactPoint.X) && FMath::IsFinite(HitResult.ImpactPoint.Y) && FMath::IsFinite(HitResult.ImpactPoint.Z)
				&& !HitResult.ImpactPoint.IsNearlyZero())
			{
				SoundLocation = HitResult.ImpactPoint;
			}

#if WITH_DEV_AUTOMATION_TESTS
			++TestParrySuccessSoundDispatchCount;
			TestLastParrySuccessSoundLocation = SoundLocation;
			if (bTestBypassAudioPlayback)
			{
				return;
			}
#endif

			if (!SoundLocation.ContainsNaN()
				&& FMath::IsFinite(SoundLocation.X) && FMath::IsFinite(SoundLocation.Y) && FMath::IsFinite(SoundLocation.Z))
			{
				if (UWorld* World = PlayerCharacter->GetWorld())
				{
					UGameplayStatics::PlaySoundAtLocation(World, ParrySuccessSound, SoundLocation);
				}
			}
		}
	}
}

void UPlayerParryAbility::OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEndAbilityRequested || Montage != ActiveMontage.Get())
	{
		return;
	}

	if (!bInterrupted)
	{
		bMontageCompletedNaturally = true;
	}

	EndFromMontage(bInterrupted);
}

void UPlayerParryAbility::OnParryWindowBegin(FGameplayEventData Payload)
{
	// IsParryWindowEventFromActiveMontage already rejects ended abilities; the
	// window-open query is intentionally absent here because this call opens it.
	if (!IsParryWindowEventFromActiveMontage(Payload))
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	if (CharacterASC && ParryActiveStateTag.IsValid())
	{
		CharacterASC->AddLooseGameplayTag(ParryActiveStateTag);
		bParryActiveTagApplied = true;
		bParryWindowOpen = true;
	}
}

void UPlayerParryAbility::OnParryWindowEnd(FGameplayEventData Payload)
{
	if (!IsParryWindowEventFromActiveMontage(Payload))
	{
		return;
	}

	bParryWindowOpen = false;
	if (bParryActiveTagApplied)
	{
		if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo())
		{
			CharacterASC->RemoveLooseGameplayTag(ParryActiveStateTag);
		}
		bParryActiveTagApplied = false;
	}
}

bool UPlayerParryAbility::ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* CharacterASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	const USkeletalMeshComponent* SkeletalMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;

	return CharacterASC && PlayerCharacter && MovementComponent && MovementComponent->IsMovingOnGround() && AnimInstance && ParryMontage
		&& CostGameplayEffectClass && CooldownGameplayEffectClass && ParryCounterPoiseGameplayEffectClass && ParryPoiseDamage > 0.0f
		&& ParryAbilityTag.IsValid() && ParryingStateTag.IsValid() && ParryActiveStateTag.IsValid()
		&& ParryWindowBeginEventTag.IsValid() && ParryWindowEndEventTag.IsValid() && ParryPoiseDataTag.IsValid()
		&& AttackingStateTag.IsValid() && DefenseCancelableStateTag.IsValid()
		&& DeadStateTag.IsValid() && StunnedStateTag.IsValid()
		&& DefenseCancelableAbilityTags.Num() == 5;
}

bool UPlayerParryAbility::IsParryWindowEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return !bEndAbilityRequested && ActiveMontage && AvatarActor && Payload.Instigator == AvatarActor && Payload.Target == AvatarActor
		&& Payload.OptionalObject.Get() == ActiveMontage.Get();
}

bool UPlayerParryAbility::IsAttackerInParryArc(const AActor* AttackingActor) const
{
	const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || !AttackingActor)
	{
		return false;
	}

	const FVector ToAttacker = (AttackingActor->GetActorLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal2D();
	const FVector ParryForward = PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (ToAttacker.IsNearlyZero() || ParryForward.IsNearlyZero())
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(ParryHalfArcDegrees));
	return FVector::DotProduct(ParryForward, ToAttacker) >= MinimumDot;
}

void UPlayerParryAbility::EndFromMontage(bool bWasCancelled)
{
	if (CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
