#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

UPlayerFrontExecutionAbility::UPlayerFrontExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FGameplayTag ExecutionFrontAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Front")), false);
	const FGameplayTag CancelableByDodgeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	const FGameplayTag CancelableByDefenseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);
	const FGameplayTag CancelableByReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false);
	const FGameplayTag TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	if (ExecutionFrontAbilityTag.IsValid()) AbilityTags.AddTag(ExecutionFrontAbilityTag);
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

bool UPlayerFrontExecutionAbility::CanActivateAbility(
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

	if (!FMath::IsFinite(MaxFrontAngleDegrees) || MaxFrontAngleDegrees < 0.0f || MaxFrontAngleDegrees > 90.0f)
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
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		return false;
	}

	return true;
}

bool UPlayerFrontExecutionAbility::ValidateTargetPrerequisites(
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

	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	if (!StunnedTag.IsValid() || !TargetASC->HasMatchingGameplayTag(StunnedTag))
	{
		return false;
	}

	if (!TargetActor->IsPoiseBroken())
	{
		return false;
	}

	bool bHasActiveStanceBreak = false;
	for (const FGameplayAbilitySpec& Spec : TargetASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA<UEnemyStanceBreakAbility>() && Spec.IsActive())
		{
			bHasActiveStanceBreak = true;
			break;
		}
	}

	return bHasActiveStanceBreak;
}

namespace
{
	bool EvaluateFrontGeometryCore(
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

		const float RawDot = TargetForward2D | TargetToPlayer2D;
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

bool UPlayerFrontExecutionAbility::CheckFrontGeometry(
	const APlayerCharacter* PlayerCharacter,
	const AEnemyCharacter* TargetActor,
	float& OutDist2D,
	float& OutAngleDegrees) const
{
	OutDist2D = 0.0f;
	OutAngleDegrees = 0.0f;

	if (!PlayerCharacter || !TargetActor)
	{
		return false;
	}

	return EvaluateFrontGeometryCore(
		PlayerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		TargetActor->GetActorForwardVector(),
		MinExecutionDistance,
		MaxExecutionDistance,
		MaxFrontAngleDegrees,
		OutDist2D,
		OutAngleDegrees);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
	const FVector& PlayerLoc,
	const FVector& TargetLoc,
	const FVector& TargetForward,
	float MinDist,
	float MaxDist,
	float MaxAngle,
	float& OutDist2D,
	float& OutAngleDegrees)
{
	return EvaluateFrontGeometryCore(
		PlayerLoc, TargetLoc, TargetForward,
		MinDist, MaxDist, MaxAngle,
		OutDist2D, OutAngleDegrees);
}
#endif

void UPlayerFrontExecutionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bEndAbilityInProgress = false;
	bDamageEventConsumed = false;
	ReservedTarget.Reset();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. Re-validate ActorInfo, Avatar, ASC, World
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
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
		UE_LOG(LogPolyQuest, Warning, TEXT("Front execution activation aborted for '%s': missing ExecutionMontage or DamageGameplayEffectClass."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Re-validate distance and angle configuration bounds ([0, 90] contract)
	if (!FMath::IsFinite(MinExecutionDistance) || !FMath::IsFinite(MaxExecutionDistance)
		|| !FMath::IsFinite(MaxFrontAngleDegrees)
		|| MinExecutionDistance < 0.0f || MaxExecutionDistance <= MinExecutionDistance
		|| MaxFrontAngleDegrees < 0.0f || MaxFrontAngleDegrees > 90.0f)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Front execution activation aborted for '%s': invalid execution config bounds."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 4. Re-validate equipped weapon: must have melee weapon in main hand
	const UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UMeleeWeaponDefinition* MeleeWeapon = EquipmentComp ? Cast<UMeleeWeaponDefinition>(EquipmentComp->GetCurrentMainHandWeapon()) : nullptr;
	if (!MeleeWeapon)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Front execution activation aborted for '%s': equipped main hand weapon is not a melee weapon."), *GetNameSafe(PlayerCharacter));
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

	// 6. Re-validate front geometry
	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReservedTarget = TargetActor;

	// Motion-Warp vs Direct Yaw alignment
	bool bWarpApplied = false;
	if (bUseMotionWarping)
	{
		FMeleeMotionWarpConfig WarpConfig;
		WarpConfig.bUseMotionWarping = bUseMotionWarping;
		WarpConfig.WarpTargetName = WarpTargetName;
		WarpConfig.MinTriggerDistance = MinTriggerDistance;
		WarpConfig.WarpStopDistance = WarpStopDistance;
		WarpConfig.MaxTriggerDistance = MaxTriggerDistance;
		WarpConfig.MaxWarpAngleDegrees = MaxWarpAngleDegrees;

		if (FMeleeMotionWarpingLifecycle::IsConfigValid(WarpConfig))
		{
			const UCharacterMovementComponent* PlayerMoveComp = PlayerCharacter->GetCharacterMovement();
			const bool bPlayerOnGround = PlayerMoveComp && PlayerMoveComp->IsMovingOnGround();
			const UCharacterMovementComponent* TargetMoveComp = TargetActor->GetCharacterMovement();
			const bool bTargetOnGround = TargetMoveComp && TargetMoveComp->IsMovingOnGround();

			FTransform WarpTransform;
			if (FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
				PlayerCharacter->GetActorLocation(),
				PlayerCharacter->GetActorForwardVector(),
				bPlayerOnGround,
				TargetActor->GetActorLocation(),
				bTargetOnGround,
				WarpConfig,
				WarpTransform))
			{
				bWarpApplied = PlayerCharacter->SetMeleeMotionWarpTarget(WarpConfig.WarpTargetName, WarpTransform);
			}
		}
	}

	if (!bWarpApplied)
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

	BindTargetDelegates(TargetActor);

	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Front.Hit")), false);
	if (!HitEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Front execution activation aborted for '%s': HitEventTag 'Event.Action.Execution.Front.Hit' is invalid."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag);
	if (!WaitHitEventTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Front execution activation aborted for '%s': failed to create WaitGameplayEvent task."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask->EventReceived.AddDynamic(this, &UPlayerFrontExecutionAbility::OnHitEventReceived);
	WaitHitEventTask->ReadyForActivation();

	if (bEndAbilityInProgress || !IsActive() || !IsValid(WaitHitEventTask))
	{
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("FrontExecutionMontageTask"),
		ExecutionMontage);

	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UPlayerFrontExecutionAbility::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UPlayerFrontExecutionAbility::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UPlayerFrontExecutionAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UPlayerFrontExecutionAbility::OnMontageCancelled);

#if WITH_DEV_AUTOMATION_TESTS
	if (!bTestSkipMontageTaskActivation)
	{
		MontageTask->ReadyForActivation();
		if (bEndAbilityInProgress || !IsActive() || !IsValid(MontageTask))
		{
			return;
		}

		USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh();
		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!AnimInstance || !ExecutionMontage || !AnimInstance->Montage_IsActive(ExecutionMontage))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}
#else
	MontageTask->ReadyForActivation();
	if (bEndAbilityInProgress || !IsActive() || !IsValid(MontageTask))
	{
		return;
	}

	USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ExecutionMontage || !AnimInstance->Montage_IsActive(ExecutionMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
#endif

	if (bEndAbilityInProgress || !IsActive())
	{
		return;
	}

	// Montage confirmed playing: cancel active guard, matching Light/Melee Skill guard arbitration
	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
}

void UPlayerFrontExecutionAbility::BindTargetDelegates(AEnemyCharacter* TargetActor)
{
	UnbindTargetDelegates();

	if (!IsValid(TargetActor))
	{
		return;
	}

	TargetActor->OnDestroyed.AddDynamic(this, &UPlayerFrontExecutionAbility::OnTargetDestroyed);

	UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	BoundTargetASC = TargetASC;

	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	if (StunnedTag.IsValid())
	{
		TargetStunnedTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(
			StunnedTag,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UPlayerFrontExecutionAbility::OnTargetTagChanged);
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	if (DeadTag.IsValid())
	{
		TargetDeadTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(
			DeadTag,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UPlayerFrontExecutionAbility::OnTargetTagChanged);
	}
}

void UPlayerFrontExecutionAbility::UnbindTargetDelegates()
{
	if (UAbilitySystemComponent* TargetASC = BoundTargetASC.Get())
	{
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (StunnedTag.IsValid() && TargetStunnedTagDelegateHandle.IsValid())
		{
			TargetASC->RegisterGameplayTagEvent(StunnedTag, EGameplayTagEventType::NewOrRemoved).Remove(TargetStunnedTagDelegateHandle);
			TargetStunnedTagDelegateHandle.Reset();
		}

		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		if (DeadTag.IsValid() && TargetDeadTagDelegateHandle.IsValid())
		{
			TargetASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved).Remove(TargetDeadTagDelegateHandle);
			TargetDeadTagDelegateHandle.Reset();
		}
	}
	BoundTargetASC.Reset();

	if (AEnemyCharacter* TargetActor = ReservedTarget.Get())
	{
		TargetActor->OnDestroyed.RemoveDynamic(this, &UPlayerFrontExecutionAbility::OnTargetDestroyed);
	}
}

void UPlayerFrontExecutionAbility::OnTargetTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);

	if ((Tag == StunnedTag && NewCount <= 0) || (Tag == DeadTag && NewCount > 0))
	{
		if (IsActive() && !bEndAbilityInProgress)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
}

void UPlayerFrontExecutionAbility::OnTargetDestroyed(AActor*)
{
	if (IsActive() && !bEndAbilityInProgress)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPlayerFrontExecutionAbility::OnHitEventReceived(FGameplayEventData Payload)
{
	if (bEndAbilityInProgress || bDamageEventConsumed || !IsActive())
	{
		return;
	}

	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Front.Hit")), false);
	if (Payload.EventTag != HitEventTag)
	{
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || Payload.Instigator != Avatar || Payload.Target != Avatar)
	{
		return;
	}

	if (Payload.OptionalObject != ExecutionMontage)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	AEnemyCharacter* TargetActor = ReservedTarget.Get();

	if (!ValidateTargetPrerequisites(PlayerCharacter, TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FVector TargetLoc = TargetActor->GetActorLocation();
	const FVector PlayerLoc = PlayerCharacter->GetActorLocation();
	if (!FMath::IsFinite(TargetLoc.X) || !FMath::IsFinite(TargetLoc.Y) || !FMath::IsFinite(TargetLoc.Z)
		|| !FMath::IsFinite(PlayerLoc.X) || !FMath::IsFinite(PlayerLoc.Y) || !FMath::IsFinite(PlayerLoc.Z))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	bDamageEventConsumed = true;

	FMeleeHitRequest HitRequest;
	HitRequest.SourceActor = PlayerCharacter;
	HitRequest.SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	HitRequest.DamageGameplayEffectClass = DamageGameplayEffectClass;
	HitRequest.AbilityLevel = GetAbilityLevel();
	HitRequest.SourceObject = this;
	HitRequest.HitResult.HitObjectHandle = FActorInstanceHandle(TargetActor);
	HitRequest.HitResult.ImpactPoint = TargetLoc;
	HitRequest.HitResult.Location = TargetLoc;

	const FVector Normal2D = (FVector(PlayerLoc.X, PlayerLoc.Y, 0.0f) - FVector(TargetLoc.X, TargetLoc.Y, 0.0f)).GetSafeNormal2D();
	if (Normal2D.IsNearlyZero()
		|| !FMath::IsFinite(Normal2D.X) || !FMath::IsFinite(Normal2D.Y) || !FMath::IsFinite(Normal2D.Z))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	HitRequest.HitResult.ImpactNormal = Normal2D;
	HitRequest.HitResult.Normal = Normal2D;

	const bool bHitApplied = FMeleeHitResolver::TryResolveHit(HitRequest);
	if (!bHitApplied)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Front execution hit resolution failed for '%s' against target '%s'."),
			*GetNameSafe(PlayerCharacter), *GetNameSafe(TargetActor));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
}

void UPlayerFrontExecutionAbility::OnMontageCompleted()
{
	if (IsActive() && !bEndAbilityInProgress)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPlayerFrontExecutionAbility::OnMontageBlendOut()
{
	if (IsActive() && !bEndAbilityInProgress)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPlayerFrontExecutionAbility::OnMontageInterrupted()
{
	if (IsActive() && !bEndAbilityInProgress)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UPlayerFrontExecutionAbility::OnMontageCancelled()
{
	if (IsActive() && !bEndAbilityInProgress)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UPlayerFrontExecutionAbility::EndAbility(
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

	UnbindTargetDelegates();

	if (WaitHitEventTask)
	{
		WaitHitEventTask->EndTask();
		WaitHitEventTask = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (ActorInfo && ActorInfo->GetAnimInstance() && ExecutionMontage)
	{
		if (ActorInfo->GetAnimInstance()->Montage_IsActive(ExecutionMontage))
		{
			ActorInfo->GetAnimInstance()->Montage_Stop(0.2f, ExecutionMontage);
		}
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();
	}

	ReservedTarget.Reset();
	bDamageEventConsumed = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPlayerFrontExecutionAbility::TestEvaluateFrontGeometry(
	const APlayerCharacter* Player,
	const AEnemyCharacter* Target,
	float& OutDist2D,
	float& OutAngleDegrees) const
{
	return CheckFrontGeometry(Player, Target, OutDist2D, OutAngleDegrees);
}
#endif
