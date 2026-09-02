#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

void UPlayerBackstabExecutionContext::OnMontageCompleted()
{
	if (UPlayerBackstabExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageCompleted(Token);
	}
}

void UPlayerBackstabExecutionContext::OnMontageBlendOut()
{
	if (UPlayerBackstabExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageBlendOut(Token);
	}
}

void UPlayerBackstabExecutionContext::OnMontageInterrupted()
{
	if (UPlayerBackstabExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageInterrupted(Token);
	}
}

void UPlayerBackstabExecutionContext::OnMontageCancelled()
{
	if (UPlayerBackstabExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageCancelled(Token);
	}
}

void UPlayerBackstabExecutionContext::OnHitEventReceived(FGameplayEventData Payload)
{
	if (UPlayerBackstabExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleHitEventReceived(Payload, Token);
	}
}

void UPlayerBackstabExecutionContext::OnTargetDestroyed(AActor* DestroyedActor)
{
	if (UPlayerBackstabExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleTargetDestroyed(DestroyedActor, Token);
	}
}

UPlayerBackstabExecutionAbility::UPlayerBackstabExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FGameplayTag ExecutionBackstabAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Backstab")), false);
	const FGameplayTag CancelableByDodgeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	const FGameplayTag CancelableByDefenseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);
	const FGameplayTag CancelableByReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false);
	const FGameplayTag TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	if (ExecutionBackstabAbilityTag.IsValid()) AbilityTags.AddTag(ExecutionBackstabAbilityTag);
	if (CancelableByDodgeTag.IsValid()) AbilityTags.AddTag(CancelableByDodgeTag);
	if (CancelableByDefenseTag.IsValid()) AbilityTags.AddTag(CancelableByDefenseTag);
	if (CancelableByReactionTag.IsValid()) AbilityTags.AddTag(CancelableByReactionTag);
	if (TeardownOnUnpossessTag.IsValid()) AbilityTags.AddTag(TeardownOnUnpossessTag);

	const FGameplayTag AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag BlockMovementTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag BlockJumpTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);

	if (AttackingStateTag.IsValid()) ActivationOwnedTags.AddTag(AttackingStateTag);
	if (BlockMovementTag.IsValid()) ActivationOwnedTags.AddTag(BlockMovementTag);
	if (BlockJumpTag.IsValid()) ActivationOwnedTags.AddTag(BlockJumpTag);

	const FGameplayTag DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	const FGameplayTag ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	const FGameplayTag ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	const FGameplayTag SprintingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
	const FGameplayTag DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag ExhaustedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

	if (AttackingStateTag.IsValid()) ActivationBlockedTags.AddTag(AttackingStateTag);
	if (DodgingStateTag.IsValid()) ActivationBlockedTags.AddTag(DodgingStateTag);
	if (ParryingStateTag.IsValid()) ActivationBlockedTags.AddTag(ParryingStateTag);
	if (ChargingStateTag.IsValid()) ActivationBlockedTags.AddTag(ChargingStateTag);
	if (SprintingStateTag.IsValid()) ActivationBlockedTags.AddTag(SprintingStateTag);
	if (DeadStateTag.IsValid()) ActivationBlockedTags.AddTag(DeadStateTag);
	if (ExhaustedStateTag.IsValid()) ActivationBlockedTags.AddTag(ExhaustedStateTag);
	if (StunnedStateTag.IsValid()) ActivationBlockedTags.AddTag(StunnedStateTag);
}

bool UPlayerBackstabExecutionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (!ExecutionMontage || !DamageGameplayEffectClass)
	{
		return false;
	}

	if (!FMath::IsFinite(MinExecutionDistance) || !FMath::IsFinite(MaxExecutionDistance)
		|| MinExecutionDistance < 0.0f || MaxExecutionDistance <= MinExecutionDistance)
	{
		return false;
	}

	if (!FMath::IsFinite(MaxBackAngleDegrees) || MaxBackAngleDegrees < 0.0f || MaxBackAngleDegrees > 90.0f)
	{
		return false;
	}

	const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!PlayerCharacter || PlayerCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	const UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UMeleeWeaponDefinition* MeleeWeapon = EquipmentComp ? Cast<UMeleeWeaponDefinition>(EquipmentComp->GetCurrentMainHandWeapon()) : nullptr;
	if (!MeleeWeapon)
	{
		return false;
	}

	const AEnemyCharacter* TargetActor = PlayerCharacter->GetLockedTarget();
	if (!ValidateTargetPrerequisites(PlayerCharacter, TargetActor))
	{
		return false;
	}

	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckBackstabGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		return false;
	}

	return true;
}

bool UPlayerBackstabExecutionAbility::ValidateTargetPrerequisites(
	const APlayerCharacter* PlayerCharacter,
	const AEnemyCharacter* TargetActor) const
{
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed())
	{
		return false;
	}

	if (TargetActor->GetWorld() != PlayerCharacter->GetWorld())
	{
		return false;
	}

	if (TargetActor->IsDead())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return false;
	}

	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	if (InvulnerableTag.IsValid() && TargetASC->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
	}

	// Backstab requires target to NOT be Stunned (distinct from Front Execution)
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	if (StunnedTag.IsValid() && TargetASC->HasMatchingGameplayTag(StunnedTag))
	{
		return false;
	}

	return true;
}

namespace
{
	bool EvaluateBackstabGeometryCore(
		const FVector& PlayerLoc,
		const FVector& TargetLoc,
		const FVector& TargetForward,
		float MinDist,
		float MaxDist,
		float MaxAngle,
		float& OutDist2D,
		float& OutAngleDegrees)
	{
		OutDist2D = 0.0f;
		OutAngleDegrees = 0.0f;

		if (!FMath::IsFinite(MinDist) || !FMath::IsFinite(MaxDist)
			|| MinDist < 0.0f || MaxDist <= MinDist)
		{
			return false;
		}

		if (!FMath::IsFinite(MaxAngle) || MaxAngle < 0.0f || MaxAngle > 90.0f)
		{
			return false;
		}

		if (!FMath::IsFinite(PlayerLoc.X) || !FMath::IsFinite(PlayerLoc.Y) || !FMath::IsFinite(PlayerLoc.Z)
			|| !FMath::IsFinite(TargetLoc.X) || !FMath::IsFinite(TargetLoc.Y) || !FMath::IsFinite(TargetLoc.Z))
		{
			return false;
		}

		const float Dist2D = FVector::Dist2D(PlayerLoc, TargetLoc);
		if (!FMath::IsFinite(Dist2D) || Dist2D < MinDist || Dist2D > MaxDist)
		{
			return false;
		}
		OutDist2D = Dist2D;

		if (!FMath::IsFinite(TargetForward.X) || !FMath::IsFinite(TargetForward.Y) || !FMath::IsFinite(TargetForward.Z))
		{
			return false;
		}

		const FVector TargetForward2D = FVector(TargetForward.X, TargetForward.Y, 0.0f).GetSafeNormal2D();
		const FVector TargetToPlayer2D = (FVector(PlayerLoc.X, PlayerLoc.Y, 0.0f) - FVector(TargetLoc.X, TargetLoc.Y, 0.0f)).GetSafeNormal2D();

		if (!FMath::IsFinite(TargetForward2D.X) || !FMath::IsFinite(TargetForward2D.Y) || TargetForward2D.IsNearlyZero()
			|| !FMath::IsFinite(TargetToPlayer2D.X) || !FMath::IsFinite(TargetToPlayer2D.Y) || TargetToPlayer2D.IsNearlyZero())
		{
			return false;
		}

		// Behind vector is negative TargetForward2D
		const FVector TargetBackward2D = -TargetForward2D;
		const float RawDot = TargetBackward2D | TargetToPlayer2D;
		if (!FMath::IsFinite(RawDot))
		{
			return false;
		}

		const float Dot = FMath::Clamp(RawDot, -1.0f, 1.0f);
		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
		if (!FMath::IsFinite(AngleDegrees))
		{
			return false;
		}
		OutAngleDegrees = AngleDegrees;

		if (AngleDegrees < 0.0f || AngleDegrees > MaxAngle)
		{
			return false;
		}

		return true;
	}
}

bool UPlayerBackstabExecutionAbility::CheckBackstabGeometry(
	const APlayerCharacter* PlayerCharacter,
	const AEnemyCharacter* TargetActor,
	float& OutDist2D,
	float& OutAngleDegrees) const
{
	OutDist2D = 0.0f;
	OutAngleDegrees = 0.0f;

	if (!IsValid(PlayerCharacter) || !IsValid(TargetActor))
	{
		return false;
	}

	return EvaluateBackstabGeometryCore(
		PlayerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		TargetActor->GetActorForwardVector(),
		MinExecutionDistance,
		MaxExecutionDistance,
		MaxBackAngleDegrees,
		OutDist2D,
		OutAngleDegrees);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometry(
	const APlayerCharacter* Player,
	const AEnemyCharacter* Target,
	float& OutDist2D,
	float& OutAngleDegrees) const
{
	return CheckBackstabGeometry(Player, Target, OutDist2D, OutAngleDegrees);
}

bool UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
	const FVector& PlayerLoc,
	const FVector& TargetLoc,
	const FVector& TargetForward,
	float MinDist,
	float MaxDist,
	float MaxAngle,
	float& OutDist2D,
	float& OutAngleDegrees)
{
	return EvaluateBackstabGeometryCore(
		PlayerLoc,
		TargetLoc,
		TargetForward,
		MinDist,
		MaxDist,
		MaxAngle,
		OutDist2D,
		OutAngleDegrees);
}

void UPlayerBackstabExecutionAbility::TestTriggerHitEvent(const FGameplayEventData& Payload)
{
	HandleHitEventReceived(Payload, CurrentActivationToken);
}
#endif

void UPlayerBackstabExecutionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Invalidate any previous activation context before starting
	if (ActiveContext)
	{
		ActiveContext->OwningAbility.Reset();
		ActiveContext = nullptr;
	}

	bEndAbilityInProgress = false;
	bDamageEventConsumed = false;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. Re-validate ActorInfo, ASC, Avatar, and World
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!PlayerCharacter || PlayerCharacter->IsActorBeingDestroyed() || !PlayerCharacter->GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* CharacterASC = ActorInfo->AbilitySystemComponent.Get();
	if (!CharacterASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag ExhaustedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag SprintingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);

	if ((DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag))
		|| (ExhaustedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ExhaustedStateTag))
		|| (StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag))
		|| (SprintingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(SprintingStateTag)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 2. Re-validate authored assets
	if (!ExecutionMontage || !DamageGameplayEffectClass)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': missing ExecutionMontage or DamageGameplayEffectClass."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Re-validate distance and angle configuration bounds ([0, 90] contract)
	if (!FMath::IsFinite(MinExecutionDistance) || !FMath::IsFinite(MaxExecutionDistance)
		|| !FMath::IsFinite(MaxBackAngleDegrees)
		|| MinExecutionDistance < 0.0f || MaxExecutionDistance <= MinExecutionDistance
		|| MaxBackAngleDegrees < 0.0f || MaxBackAngleDegrees > 90.0f)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': invalid execution config bounds."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 4. Re-validate equipped weapon: must have melee weapon in main hand
	const UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UMeleeWeaponDefinition* MeleeWeapon = EquipmentComp ? Cast<UMeleeWeaponDefinition>(EquipmentComp->GetCurrentMainHandWeapon()) : nullptr;
	if (!MeleeWeapon)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': equipped main hand weapon is not a melee weapon."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 5. Re-validate target prerequisites
	AEnemyCharacter* TargetActor = PlayerCharacter->GetLockedTarget();
	if (!ValidateTargetPrerequisites(PlayerCharacter, TargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 6. Re-validate backstab geometry
	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckBackstabGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bEndAbilityInProgress || !IsActive())
	{
		return;
	}

	ReservedTarget = TargetActor;

	// Create fresh activation context with isolated generation token
	const uint32 ThisActivationToken = ++CurrentActivationToken;
	ActiveContext = NewObject<UPlayerBackstabExecutionContext>(this);
	ActiveContext->OwningAbility = this;
	ActiveContext->Token = ThisActivationToken;

	// Motion-Warp vs Direct Yaw alignment
	bool bMotionWarpApplied = false;
	if (bUseMotionWarping)
	{
		FMeleeMotionWarpConfig WarpConfig;
		WarpConfig.bUseMotionWarping = true;
		WarpConfig.WarpTargetName = WarpTargetName;
		WarpConfig.MinTriggerDistance = MinTriggerDistance;
		WarpConfig.WarpStopDistance = WarpStopDistance;
		WarpConfig.MaxTriggerDistance = MaxTriggerDistance;
		WarpConfig.MaxWarpAngleDegrees = MaxWarpAngleDegrees;

		if (FMeleeMotionWarpingLifecycle::IsConfigValid(WarpConfig))
		{
			const UCharacterMovementComponent* PlayerMove = PlayerCharacter->GetCharacterMovement();
			const UCharacterMovementComponent* TargetMove = TargetActor->GetCharacterMovement();
			const bool bPlayerOnGround = PlayerMove && PlayerMove->IsMovingOnGround();
			const bool bTargetOnGround = TargetMove && TargetMove->IsMovingOnGround();

			FTransform OutWarpTransform;
			if (FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
				PlayerCharacter->GetActorLocation(),
				PlayerCharacter->GetActorForwardVector(),
				bPlayerOnGround,
				TargetActor->GetActorLocation(),
				bTargetOnGround,
				WarpConfig,
				OutWarpTransform))
			{
				bMotionWarpApplied = PlayerCharacter->SetMeleeMotionWarpTarget(WarpConfig.WarpTargetName, OutWarpTransform);
			}
		}
	}

	if (!bMotionWarpApplied)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();

		const FVector PlayerLoc = PlayerCharacter->GetActorLocation();
		const FVector TargetLoc = TargetActor->GetActorLocation();
		if (FMath::IsFinite(PlayerLoc.X) && FMath::IsFinite(PlayerLoc.Y)
			&& FMath::IsFinite(TargetLoc.X) && FMath::IsFinite(TargetLoc.Y))
		{
			const FVector PlayerToTarget2D = (FVector(TargetLoc.X, TargetLoc.Y, 0.0f) - FVector(PlayerLoc.X, PlayerLoc.Y, 0.0f)).GetSafeNormal2D();
			if (!PlayerToTarget2D.IsNearlyZero() && FMath::IsFinite(PlayerToTarget2D.X) && FMath::IsFinite(PlayerToTarget2D.Y))
			{
				const float TargetYaw = PlayerToTarget2D.Rotation().Yaw;
				if (FMath::IsFinite(TargetYaw))
				{
					const FRotator TargetRot(0.0f, TargetYaw, 0.0f);
					PlayerCharacter->SetActorRotation(TargetRot);
				}
			}
		}
	}

	BindTargetDelegates(TargetActor, ThisActivationToken);

	// Setup WaitGameplayEvent Task routed through ActiveContext (fail-closed)
	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Backstab.Hit")), false);
	if (!HitEventTag.IsValid() || !ActiveContext)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': HitEventTag is invalid or ActiveContext is missing."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false, false);
	if (!WaitHitEventTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': failed to create WaitGameplayEvent task."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask->EventReceived.AddDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnHitEventReceived);
	WaitHitEventTask->ReadyForActivation();

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestInvalidateWaitHitEventTaskAfterReady && IsValid(WaitHitEventTask))
	{
		WaitHitEventTask->EndTask();
		WaitHitEventTask = nullptr;
	}
#endif

	// A synchronous callback may already have ended the Ability. Preserve that
	// terminal path; an invalid Task while the Ability is still active is a
	// startup failure and must converge through the normal cleanup path.
	if (bEndAbilityInProgress || !IsActive())
	{
		return;
	}
	if (!IsValid(WaitHitEventTask) || !WaitHitEventTask->IsActive() || WaitHitEventTask->IsFinished())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': WaitGameplayEvent task became inactive during startup."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestSkipMontageTaskActivation)
	{
		return;
	}
#endif

	// Setup PlayMontageAndWait Task routed through ActiveContext
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("BackstabExecutionMontageTask"),
		ExecutionMontage);

	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnMontageCancelled);

	MontageTask->ReadyForActivation();

	if (bEndAbilityInProgress || !IsActive())
	{
		return;
	}
	if (!IsValid(MontageTask) || !MontageTask->IsActive() || MontageTask->IsFinished())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution activation aborted for '%s': Montage task became inactive during startup."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ExecutionMontage || !AnimInstance->Montage_IsActive(ExecutionMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bEndAbilityInProgress || !IsActive())
	{
		return;
	}

	// Montage confirmed playing: cancel active guard, matching Light/Melee Skill guard arbitration
	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
}

void UPlayerBackstabExecutionAbility::HandleMontageCompleted(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || bEndAbilityInProgress)
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerBackstabExecutionAbility::HandleMontageBlendOut(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || bEndAbilityInProgress)
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerBackstabExecutionAbility::HandleMontageInterrupted(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || bEndAbilityInProgress)
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerBackstabExecutionAbility::HandleMontageCancelled(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || bEndAbilityInProgress)
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerBackstabExecutionAbility::HandleHitEventReceived(FGameplayEventData Payload, uint32 InToken)
{
	if (InToken != CurrentActivationToken || bEndAbilityInProgress || bDamageEventConsumed || !IsActive())
	{
		return;
	}

	const FGameplayTag ExpectedHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Backstab.Hit")), false);
	if (!ExpectedHitTag.IsValid() || Payload.EventTag != ExpectedHitTag)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || PlayerCharacter->IsActorBeingDestroyed())
	{
		return;
	}

	if (Payload.Instigator != PlayerCharacter || Payload.Target != PlayerCharacter)
	{
		return;
	}

	// Strict Montage verification: OptionalObject must be valid and exactly match ExecutionMontage
	if (!ExecutionMontage || Payload.OptionalObject != ExecutionMontage)
	{
		return;
	}

	AEnemyCharacter* TargetActor = ReservedTarget.Get();
	if (!ValidateTargetPrerequisites(PlayerCharacter, TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Verify distance boundary at hit frame (fail-closed if target escaped range)
	const FVector PlayerLoc = PlayerCharacter->GetActorLocation();
	const FVector TargetLoc = TargetActor->GetActorLocation();
	if (!FMath::IsFinite(PlayerLoc.X) || !FMath::IsFinite(PlayerLoc.Y) || !FMath::IsFinite(PlayerLoc.Z)
		|| !FMath::IsFinite(TargetLoc.X) || !FMath::IsFinite(TargetLoc.Y) || !FMath::IsFinite(TargetLoc.Z))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const float Dist2D = FVector::Dist2D(PlayerLoc, TargetLoc);
	if (!FMath::IsFinite(Dist2D) || Dist2D < MinExecutionDistance || Dist2D > MaxExecutionDistance)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	bDamageEventConsumed = true;

	const FVector Normal2D = (FVector(PlayerLoc.X, PlayerLoc.Y, 0.0f) - FVector(TargetLoc.X, TargetLoc.Y, 0.0f)).GetSafeNormal2D();
	if (Normal2D.IsNearlyZero()
		|| !FMath::IsFinite(Normal2D.X) || !FMath::IsFinite(Normal2D.Y) || !FMath::IsFinite(Normal2D.Z))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FMeleeHitRequest HitRequest;
	HitRequest.SourceActor = PlayerCharacter;
	HitRequest.SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	HitRequest.DamageGameplayEffectClass = DamageGameplayEffectClass;
	HitRequest.AbilityLevel = GetAbilityLevel();
	HitRequest.SourceObject = this;
	HitRequest.HitResult.HitObjectHandle = FActorInstanceHandle(TargetActor);
	HitRequest.HitResult.ImpactPoint = TargetLoc;
	HitRequest.HitResult.Location = TargetLoc;
	HitRequest.HitResult.ImpactNormal = Normal2D;
	HitRequest.HitResult.Normal = Normal2D;

	const bool bHitApplied = FMeleeHitResolver::TryResolveHit(HitRequest);
	if (!bHitApplied)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Backstab execution hit resolution failed for '%s' against target '%s'."),
			*GetNameSafe(PlayerCharacter), *GetNameSafe(TargetActor));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
}

void UPlayerBackstabExecutionAbility::HandleTargetDestroyed(AActor* DestroyedActor, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || bEndAbilityInProgress)
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerBackstabExecutionAbility::HandleTargetTagChanged(const FGameplayTag Tag, int32 NewCount, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	if (NewCount > 0)
	{
		// Target died or became stunned -> terminate ability
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UPlayerBackstabExecutionAbility::BindTargetDelegates(AEnemyCharacter* TargetActor, uint32 InToken)
{
	UnbindTargetDelegates();

	if (!IsValid(TargetActor) || !ActiveContext)
	{
		return;
	}

	TargetActor->OnDestroyed.AddUniqueDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnTargetDestroyed);

	if (UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent())
	{
		BoundTargetASC = TargetASC;

		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (StunnedTag.IsValid())
		{
			TargetStunnedTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(
				StunnedTag,
				EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UPlayerBackstabExecutionAbility::HandleTargetTagChanged, InToken);
		}

		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		if (DeadTag.IsValid())
		{
			TargetDeadTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(
				DeadTag,
				EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UPlayerBackstabExecutionAbility::HandleTargetTagChanged, InToken);
		}
	}
}

void UPlayerBackstabExecutionAbility::UnbindTargetDelegates()
{
	if (ReservedTarget.IsValid() && ActiveContext)
	{
		ReservedTarget->OnDestroyed.RemoveDynamic(ActiveContext, &UPlayerBackstabExecutionContext::OnTargetDestroyed);
	}

	if (UAbilitySystemComponent* TargetASC = BoundTargetASC.Get())
	{
		if (TargetStunnedTagDelegateHandle.IsValid())
		{
			const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
			TargetASC->RegisterGameplayTagEvent(StunnedTag, EGameplayTagEventType::NewOrRemoved).Remove(TargetStunnedTagDelegateHandle);
			TargetStunnedTagDelegateHandle.Reset();
		}

		if (TargetDeadTagDelegateHandle.IsValid())
		{
			const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
			TargetASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved).Remove(TargetDeadTagDelegateHandle);
			TargetDeadTagDelegateHandle.Reset();
		}
	}

	BoundTargetASC.Reset();
}

void UPlayerBackstabExecutionAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bEndAbilityInProgress)
	{
		return;
	}

	bEndAbilityInProgress = true;

	// 1. Enter terminal state, unbind delegates, and invalidate callback context
	UnbindTargetDelegates();

	if (ActiveContext)
	{
		ActiveContext->OwningAbility.Reset();
		ActiveContext = nullptr;
	}

	// 2. End active tasks and stop animation montage
	if (IsValid(WaitHitEventTask))
	{
		WaitHitEventTask->EndTask();
	}
	WaitHitEventTask = nullptr;

	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
	}
	MontageTask = nullptr;

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UAnimInstance* AnimInstance = PlayerCharacter->GetMesh() ? PlayerCharacter->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (ExecutionMontage && AnimInstance->Montage_IsPlaying(ExecutionMontage))
			{
				AnimInstance->Montage_Stop(0.2f, ExecutionMontage);
			}
		}

		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}

	ReservedTarget.Reset();
	bDamageEventConsumed = false;

	// 3. Complete ability lifecycle teardown
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
