#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

void UPlayerFrontExecutionContext::OnMontageCompleted()
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageCompleted(Token);
	}
}

void UPlayerFrontExecutionContext::OnMontageBlendOut()
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageBlendOut(Token);
	}
}

void UPlayerFrontExecutionContext::OnMontageInterrupted()
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageInterrupted(Token);
	}
}

void UPlayerFrontExecutionContext::OnMontageCancelled()
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleMontageCancelled(Token);
	}
}

void UPlayerFrontExecutionContext::OnHitEventReceived(FGameplayEventData Payload)
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleHitEventReceived(Payload, Token);
	}
}

void UPlayerFrontExecutionContext::OnTargetDestroyed(AActor* DestroyedActor)
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleTargetDestroyed(DestroyedActor, Token);
	}
}

UPlayerFrontExecutionAbility::UPlayerFrontExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	MinExecutionDistance = 0.0f;
	MaxExecutionDistance = 250.0f;
	WarpStopDistance = 190.0f;

	const FGameplayTag ExecutionFrontAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Front")), false);
	const FGameplayTag TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	AbilityTags.AddTag(ExecutionFrontAbilityTag);
	AbilityTags.AddTag(TeardownOnUnpossessTag);

	const FGameplayTag AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag PlayerLockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
	const FGameplayTag InvulnerableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag BlockMovementTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag BlockJumpTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);

	ActivationOwnedTags.AddTag(AttackingStateTag);
	ActivationOwnedTags.AddTag(PlayerLockedStateTag);
	ActivationOwnedTags.AddTag(InvulnerableStateTag);
	ActivationOwnedTags.AddTag(BlockMovementTag);
	ActivationOwnedTags.AddTag(BlockJumpTag);

	const FGameplayTag DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	const FGameplayTag ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	const FGameplayTag ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	const FGameplayTag SprintingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
	const FGameplayTag DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag ExhaustedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag VictimLockedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);

	ActivationBlockedTags.AddTag(AttackingStateTag);
	ActivationBlockedTags.AddTag(DodgingStateTag);
	ActivationBlockedTags.AddTag(ParryingStateTag);
	ActivationBlockedTags.AddTag(ChargingStateTag);
	ActivationBlockedTags.AddTag(SprintingStateTag);
	ActivationBlockedTags.AddTag(DeadStateTag);
	ActivationBlockedTags.AddTag(ExhaustedStateTag);
	ActivationBlockedTags.AddTag(StunnedStateTag);
	ActivationBlockedTags.AddTag(PlayerLockedStateTag);
	ActivationBlockedTags.AddTag(VictimLockedStateTag);
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

	const UWeaponEquipmentComponent* Equipment = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UMeleeWeaponDefinition* MeleeWeapon = Equipment ? Cast<UMeleeWeaponDefinition>(Equipment->GetCurrentMainHandWeapon()) : nullptr;
	if (!MeleeWeapon)
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

	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	if (VictimLockedTag.IsValid() && TargetASC->HasMatchingGameplayTag(VictimLockedTag))
	{
		return false;
	}

	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	if (!StunnedTag.IsValid() || !TargetASC->HasMatchingGameplayTag(StunnedTag))
	{
		return false;
	}

	return true;
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

	return TestEvaluateFrontGeometryVectors(
		PlayerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		TargetActor->GetActorForwardVector(),
		MinExecutionDistance,
		MaxExecutionDistance,
		MaxFrontAngleDegrees,
		OutDist2D,
		OutAngleDegrees);
}

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
	OutDist2D = 0.0f;
	OutAngleDegrees = 0.0f;

	if (!FMath::IsFinite(MinDist) || !FMath::IsFinite(MaxDist) || MinDist < 0.0f || MaxDist <= MinDist
		|| !FMath::IsFinite(MaxAngle) || MaxAngle < 0.0f || MaxAngle > 90.0f
		|| PlayerLoc.ContainsNaN() || TargetLoc.ContainsNaN() || TargetForward.ContainsNaN())
	{
		return false;
	}

	const FVector2D PLoc2D(PlayerLoc.X, PlayerLoc.Y);
	const FVector2D TLoc2D(TargetLoc.X, TargetLoc.Y);
	const float Dist = FVector2D::Distance(PLoc2D, TLoc2D);
	OutDist2D = Dist;

	if (Dist < MinDist || Dist > MaxDist)
	{
		return false;
	}

	FVector2D TForward2D(TargetForward.X, TargetForward.Y);
	if (!TForward2D.Normalize())
	{
		return false;
	}

	FVector2D TargetToPlayer2D = PLoc2D - TLoc2D;
	if (!TargetToPlayer2D.Normalize())
	{
		return false;
	}

	const float DotProduct = FMath::Clamp(FVector2D::DotProduct(TForward2D, TargetToPlayer2D), -1.0f, 1.0f);
	const float AngleRad = FMath::Acos(DotProduct);
	const float AngleDeg = FMath::RadiansToDegrees(AngleRad);
	OutAngleDegrees = AngleDeg;

	return AngleDeg <= MaxAngle;
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

void UPlayerFrontExecutionAbility::TestTriggerHitEvent(const FGameplayEventData& Payload)
{
	HandleHitEventReceived(Payload, CurrentActivationToken);
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

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	if (!PlayerCharacter || !CharacterASC || PlayerCharacter->IsActorBeingDestroyed())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AEnemyCharacter* TargetActor = PlayerCharacter->GetLockedTarget();
	if (!ValidateTargetPrerequisites(PlayerCharacter, TargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. Commit Ability (Cost / Cooldown check)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Create and initialize synchronized execution session
	ActiveExecutionContext = NewObject<UExecutionLockContext>(this);
	++CurrentActivationToken;
	const uint32 ActivationToken = CurrentActivationToken;

	const FGameplayTag FrontRequestTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
	ActiveExecutionContext->InitializeSession(this, PlayerCharacter, CharacterASC, TargetActor, FrontRequestTag, ActivationToken);

	// Synchronously request victim execution ability on target ASC
	FGameplayEventData RequestPayload;
	RequestPayload.EventTag = FrontRequestTag;
	RequestPayload.Instigator = PlayerCharacter;
	RequestPayload.Target = TargetActor;
	RequestPayload.OptionalObject = ActiveExecutionContext;
	TargetASC->HandleGameplayEvent(FrontRequestTag, &RequestPayload);

	if (!ActiveExecutionContext->IsVictimAccepted())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReservedTarget = TargetActor;

	// Setup Motion Warping or fallback horizontal Yaw facing
	FMeleeMotionWarpConfig WarpConfig;
	WarpConfig.bUseMotionWarping = bUseMotionWarping;
	WarpConfig.WarpTargetName = WarpTargetName;
	WarpConfig.MinTriggerDistance = MinExecutionDistance;
	WarpConfig.WarpStopDistance = WarpStopDistance;
	WarpConfig.MaxTriggerDistance = MaxExecutionDistance;
	WarpConfig.MaxWarpAngleDegrees = MaxWarpAngleDegrees;

	const UCharacterMovementComponent* PlayerMoveComp = PlayerCharacter->GetCharacterMovement();
	const bool bPlayerOnGround = PlayerMoveComp && PlayerMoveComp->IsMovingOnGround();
	const UCharacterMovementComponent* TargetMoveComp = TargetActor->GetCharacterMovement();
	const bool bTargetOnGround = TargetMoveComp && TargetMoveComp->IsMovingOnGround();

	FTransform WarpTransform;
	if (bUseMotionWarping && FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform(
		PlayerCharacter->GetActorLocation(),
		PlayerCharacter->GetActorForwardVector(),
		bPlayerOnGround,
		TargetActor->GetActorLocation(),
		bTargetOnGround,
		WarpConfig,
		WarpTransform))
	{
		if (!PlayerCharacter->SetMeleeMotionWarpTarget(WarpTargetName, WarpTransform))
		{
			PlayerCharacter->ClearMeleeMotionWarpTargets();
			const FVector ToTarget = (TargetActor->GetActorLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				PlayerCharacter->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
			}
		}
	}
	else
	{
		const FVector ToTarget = (TargetActor->GetActorLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			PlayerCharacter->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
		}
	}

	ActiveContext = NewObject<UPlayerFrontExecutionContext>(this);
	ActiveContext->OwningAbility = this;
	ActiveContext->Token = ActivationToken;

	BindTargetDelegates(TargetActor, ActivationToken);

	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Front.Hit")), false);
	if (!HitEventTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false, false);
	if (!WaitHitEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask->EventReceived.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnHitEventReceived);

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestSkipMontageTaskActivation)
	{
		WaitHitEventTask->ReadyForActivation();
		if (bTestEndAbilityDuringTaskReady && IsActive())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		}
		if (!IsActive())
		{
			return;
		}
		if (bTestInvalidateWaitHitEventTaskAfterReady)
		{
			WaitHitEventTask = nullptr;
		}
		if (!ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, ActivationToken) || !WaitHitEventTask || !WaitHitEventTask->IsActive())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		return;
	}
#endif

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ExecutionMontage, 1.0f);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageCancelled);

	MontageTask->ReadyForActivation();
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestEndAbilityDuringTaskReady && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
#endif
	if (!IsActive())
	{
		return;
	}
	if (!ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, ActivationToken) || !MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitHitEventTask->ReadyForActivation();
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestEndAbilityDuringTaskReady && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
	if (bTestInvalidateWaitHitEventTaskAfterReady)
	{
		WaitHitEventTask = nullptr;
	}
#endif
	if (!IsActive())
	{
		return;
	}
	if (!ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, ActivationToken) || !WaitHitEventTask || !WaitHitEventTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimInstance* AnimInstance = PlayerCharacter->GetMesh() ? PlayerCharacter->GetMesh()->GetAnimInstance() : nullptr;
	const bool bMontageActive = AnimInstance && AnimInstance->Montage_IsActive(ExecutionMontage);
	if (!bMontageActive)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerCharacter->CancelActiveGuardAfterConfirmedAction(true);
}

void UPlayerFrontExecutionAbility::HandleMontageCompleted(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerFrontExecutionAbility::HandleMontageBlendOut(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerFrontExecutionAbility::HandleMontageInterrupted(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerFrontExecutionAbility::HandleMontageCancelled(uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerFrontExecutionAbility::HandleHitEventReceived(FGameplayEventData Payload, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}

	if (bDamageEventConsumed)
	{
		return;
	}

	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Front.Hit")), false);
	if (Payload.EventTag != HitEventTag)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (Payload.Instigator != PlayerCharacter || Payload.Target != PlayerCharacter)
	{
		return;
	}

	if (Payload.OptionalObject != ExecutionMontage)
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* TargetActor = ReservedTarget.Get();

	if (!PlayerCharacter || !CharacterASC || !TargetActor || TargetActor->IsDead() || TargetActor->IsActorBeingDestroyed())
	{
		return;
	}

	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, Dist2D, AngleDegrees))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	FMeleeHitRequest HitRequest;
	HitRequest.SourceActor = PlayerCharacter;
	HitRequest.SourceAbilitySystemComponent = CharacterASC;
	HitRequest.DamageGameplayEffectClass = DamageGameplayEffectClass;
	HitRequest.AbilityLevel = GetAbilityLevel();
	HitRequest.SourceObject = this;
	HitRequest.ExecutionContext = ActiveExecutionContext;

	HitRequest.HitResult.HitObjectHandle = FActorInstanceHandle(TargetActor);
	HitRequest.HitResult.Location = TargetActor->GetActorLocation();
	HitRequest.HitResult.ImpactPoint = TargetActor->GetActorLocation();

	if (FMeleeHitResolver::TryResolveHit(HitRequest))
	{
		bDamageEventConsumed = true;
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UPlayerFrontExecutionAbility::HandleTargetDestroyed(AActor* DestroyedActor, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || ActiveExecutionContext->GetSourceActivationToken() != InToken)
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerFrontExecutionAbility::HandleTargetTagChanged(const FGameplayTag Tag, int32 NewCount, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || ActiveExecutionContext->GetSourceActivationToken() != InToken)
	{
		return;
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

	if (Tag == DeadTag && NewCount > 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	else if (Tag == VictimLockedTag && NewCount == 0 && !bEndAbilityInProgress)
	{
		// VictimLocked was removed from target prematurely
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	else if (Tag == StunnedTag && NewCount == 0 && !bDamageEventConsumed && !bEndAbilityInProgress)
	{
		// Stun ended before hit was resolved
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UPlayerFrontExecutionAbility::BindTargetDelegates(AEnemyCharacter* TargetActor, uint32 InToken)
{
	UnbindTargetDelegates();

	if (!TargetActor)
	{
		return;
	}

	TargetActor->OnDestroyed.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnTargetDestroyed);

	UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (TargetASC)
	{
		BoundTargetASC = TargetASC;
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);

		if (StunnedTag.IsValid())
		{
			TargetStunnedTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(StunnedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UPlayerFrontExecutionAbility::HandleTargetTagChanged, InToken);
		}

		if (DeadTag.IsValid())
		{
			TargetDeadTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UPlayerFrontExecutionAbility::HandleTargetTagChanged, InToken);
		}

		if (VictimLockedTag.IsValid())
		{
			TargetVictimLockedTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(VictimLockedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UPlayerFrontExecutionAbility::HandleTargetTagChanged, InToken);
		}
	}
}

void UPlayerFrontExecutionAbility::UnbindTargetDelegates()
{
	if (AEnemyCharacter* Target = ReservedTarget.Get())
	{
		if (ActiveContext)
		{
			Target->OnDestroyed.RemoveDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnTargetDestroyed);
		}
	}

	if (UAbilitySystemComponent* TargetASC = BoundTargetASC.Get())
	{
		if (TargetStunnedTagDelegateHandle.IsValid())
		{
			const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
			TargetASC->UnregisterGameplayTagEvent(TargetStunnedTagDelegateHandle, StunnedTag, EGameplayTagEventType::NewOrRemoved);
			TargetStunnedTagDelegateHandle.Reset();
		}

		if (TargetDeadTagDelegateHandle.IsValid())
		{
			const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
			TargetASC->UnregisterGameplayTagEvent(TargetDeadTagDelegateHandle, DeadTag, EGameplayTagEventType::NewOrRemoved);
			TargetDeadTagDelegateHandle.Reset();
		}

		if (TargetVictimLockedTagDelegateHandle.IsValid())
		{
			const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			TargetASC->UnregisterGameplayTagEvent(TargetVictimLockedTagDelegateHandle, VictimLockedTag, EGameplayTagEventType::NewOrRemoved);
			TargetVictimLockedTagDelegateHandle.Reset();
		}
	}

	BoundTargetASC = nullptr;
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

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UAbilitySystemComponent* TargetASC = BoundTargetASC.Get() ? BoundTargetASC.Get() : (ActiveExecutionContext ? ActiveExecutionContext->GetVictimASC() : nullptr);
	AActor* TargetActor = ReservedTarget.Get() ? ReservedTarget.Get() : (ActiveExecutionContext ? ActiveExecutionContext->GetTargetActor() : nullptr);

	// 1. Unbind target delegates first so Release and tag removal does not trigger callbacks
	UnbindTargetDelegates();

	// 2. Send Release GameplayEvent to target
	const FGameplayTag ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	if (ActiveExecutionContext && TargetASC && !ActiveExecutionContext->IsReleaseSent())
	{
		ActiveExecutionContext->MarkReleaseSent();
		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseEventTag;
		ReleasePayload.Instigator = PlayerCharacter;
		ReleasePayload.Target = TargetActor;
		ReleasePayload.OptionalObject = ActiveExecutionContext;
		TargetASC->HandleGameplayEvent(ReleaseEventTag, &ReleasePayload);
	}

	// 3. Invalidate callback context & execution session
	if (ActiveContext)
	{
		ActiveContext->OwningAbility.Reset();
		ActiveContext = nullptr;
	}

	if (ActiveExecutionContext)
	{
		ActiveExecutionContext->InvalidateSession();
		ActiveExecutionContext = nullptr;
	}

	// 4. End tasks and stop montage
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

	if (PlayerCharacter)
	{
		PlayerCharacter->ClearMeleeMotionWarpTargets();

		if (UAnimInstance* AnimInstance = PlayerCharacter->GetMesh() ? PlayerCharacter->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (ExecutionMontage && AnimInstance->Montage_IsActive(ExecutionMontage))
			{
				AnimInstance->Montage_Stop(0.2f, ExecutionMontage);
			}
		}
	}

	ReservedTarget = nullptr;
	bDamageEventConsumed = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEndAbilityInProgress = false;
}
