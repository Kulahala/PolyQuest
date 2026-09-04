#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Execution/ExecutionSnapAlignment.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

namespace
{
	bool IsFrontExecutionAnimationFromMontage(const UAnimMontage* Montage, const UObject* AnimationObject)
	{
		if (!Montage || !AnimationObject)
		{
			return false;
		}
		if (AnimationObject == Montage)
		{
			return true;
		}
		if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationObject))
		{
			for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
			{
				for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
				{
					if (Segment.GetAnimReference() == SequenceBase)
					{
						return true;
					}
				}
			}
		}
		return false;
	}
}

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

void UPlayerFrontExecutionContext::OnReleaseRequestEventReceived(FGameplayEventData Payload)
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleReleaseRequestEventReceived(Payload, Token);
	}
}

void UPlayerFrontExecutionContext::OnVictimStartEventReceived(FGameplayEventData Payload)
{
	if (UPlayerFrontExecutionAbility* Ability = OwningAbility.Get())
	{
		Ability->HandleVictimStartEventReceived(Payload, Token);
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

bool UPlayerFrontExecutionAbility::TryResolveExecutionMontage(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UMeleeWeaponDefinition*& OutWeaponDef,
	UAnimMontage*& OutMontage) const
{
	OutWeaponDef = nullptr;
	OutMontage = nullptr;

	const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!PlayerCharacter || PlayerCharacter->IsActorBeingDestroyed())
	{
		return false;
	}

	const UWeaponEquipmentComponent* Equipment = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UMeleeWeaponDefinition* MeleeWeapon = Equipment ? Equipment->GetEquippedMainHandMelee() : nullptr;
	if (!MeleeWeapon)
	{
		return false;
	}

	UAnimMontage* Montage = MeleeWeapon->FrontExecutionMontage;
	if (!Montage)
	{
		return false;
	}

	OutWeaponDef = MeleeWeapon;
	OutMontage = Montage;
	return true;
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

	if (!DamageGameplayEffectClass)
	{
		return false;
	}

	const UMeleeWeaponDefinition* ResolvedWeapon = nullptr;
	UAnimMontage* ResolvedMontage = nullptr;
	if (!TryResolveExecutionMontage(ActorInfo, ResolvedWeapon, ResolvedMontage))
	{
		return false;
	}

	if (!ResolvedWeapon || !FExecutionSnapAlignment::IsExecutionDistanceRangeValid(
			ResolvedWeapon->MinExecutionDistance,
			ResolvedWeapon->MaxExecutionDistance,
			ResolvedWeapon->ExecutionSnapDistance))
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
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, ResolvedWeapon->MinExecutionDistance, ResolvedWeapon->MaxExecutionDistance, Dist2D, AngleDegrees))
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
	const UMeleeWeaponDefinition* MeleeWeapon = Equipment ? Equipment->GetEquippedMainHandMelee() : nullptr;
	if (!MeleeWeapon)
	{
		return false;
	}

	if (!FExecutionSnapAlignment::IsExecutionDistanceRangeValid(
			MeleeWeapon->MinExecutionDistance,
			MeleeWeapon->MaxExecutionDistance,
			MeleeWeapon->ExecutionSnapDistance))
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

	if (TargetActor->IsDead() || TargetActor->IsDeathPending())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return false;
	}

	const float TargetHealth = TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	if (TargetHealth <= 0.0f)
	{
		return false;
	}

	const FGameplayTag DeathPendingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	if (DeathPendingTag.IsValid() && TargetASC->HasMatchingGameplayTag(DeathPendingTag))
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
	const float MinDist,
	const float MaxDist,
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
		MinDist,
		MaxDist,
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

bool UPlayerFrontExecutionAbility::TryApplyExecutionSnap(
	APlayerCharacter* PlayerCharacter,
	AEnemyCharacter* TargetActor,
	const FVector& TargetForwardSnapshot)
{
	if (!PlayerCharacter || !TargetActor)
	{
		return false;
	}

	UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
	UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement();
	if (!PlayerCapsule || !MoveComp)
	{
		return false;
	}

	if (!bHasActiveExecutionDistanceSnapshot || !FExecutionSnapAlignment::IsSnapDistanceValid(ActiveExecutionSnapDistance))
	{
		return false;
	}

	const FVector OriginalLocation = PlayerCharacter->GetActorLocation();
	const FRotator OriginalRotation = PlayerCharacter->GetActorRotation();

	FTransform TargetTransform;
	if (!FExecutionSnapAlignment::TryBuildTransform(
		OriginalLocation,
		TargetActor->GetActorLocation(),
		TargetForwardSnapshot,
		ActiveExecutionSnapDistance,
		EExecutionSnapSide::Front,
		TargetTransform))
	{
		return false;
	}

	const bool bWasIgnoringTarget = PlayerCapsule->GetMoveIgnoreActors().Contains(TargetActor);
	if (!bWasIgnoringTarget)
	{
		PlayerCapsule->IgnoreActorWhenMoving(TargetActor, true);
	}

	FHitResult HitResult;
	PlayerCharacter->SetActorLocationAndRotation(
		TargetTransform.GetLocation(),
		TargetTransform.Rotator(),
		/*bSweep=*/true,
		&HitResult,
		ETeleportType::TeleportPhysics);

	if (!bWasIgnoringTarget)
	{
		PlayerCapsule->IgnoreActorWhenMoving(TargetActor, false);
	}

	bool bBlocked = false;
	if (HitResult.bBlockingHit && HitResult.GetActor() != TargetActor)
	{
		bBlocked = true;
	}

	constexpr float LocationTolerance = 1.0f; // 1.0 cm
	constexpr float RotationToleranceDegrees = 1.0f; // 1.0 deg

	const FVector ActualLocation = PlayerCharacter->GetActorLocation();
	const float DistanceDelta = FVector::Dist(ActualLocation, TargetTransform.GetLocation());
	const bool bLocationOk = (DistanceDelta <= LocationTolerance);

	const float ActualYaw = PlayerCharacter->GetActorRotation().Yaw;
	const float TargetYaw = TargetTransform.Rotator().Yaw;
	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, ActualYaw));
	const bool bRotationOk = (YawDelta <= RotationToleranceDegrees);

	if (!bBlocked && bLocationOk && bRotationOk)
	{
		return true;
	}

	PlayerCharacter->SetActorLocationAndRotation(
		OriginalLocation,
		OriginalRotation,
		/*bSweep=*/false,
		nullptr,
		ETeleportType::TeleportPhysics);

	MoveComp->StopMovementImmediately();
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPlayerFrontExecutionAbility::TestEvaluateFrontGeometry(
	const APlayerCharacter* Player,
	const AEnemyCharacter* Target,
	float& OutDist2D,
	float& OutAngleDegrees) const
{
	const UWeaponEquipmentComponent* Equip = Player ? Player->FindComponentByClass<UWeaponEquipmentComponent>() : nullptr;
	const UMeleeWeaponDefinition* Weapon = Equip ? Equip->GetEquippedMainHandMelee() : nullptr;
	const float MinDist = Weapon ? Weapon->MinExecutionDistance : 0.0f;
	const float MaxDist = Weapon ? Weapon->MaxExecutionDistance : 250.0f;
	return CheckFrontGeometry(Player, Target, MinDist, MaxDist, OutDist2D, OutAngleDegrees);
}

void UPlayerFrontExecutionAbility::TestTriggerHitEvent(const FGameplayEventData& Payload)
{
	HandleHitEventReceived(Payload, CurrentActivationToken);
}
#endif

bool UPlayerFrontExecutionAbility::CommitAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	OUT FGameplayTagContainer* OptionalRelevantTags)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestForceCommitAbilityFailure)
	{
		return false;
	}
#endif

	if (!ActiveExecutionWeaponDefinition || !ActiveExecutionMontage || !bHasActiveExecutionDistanceSnapshot)
	{
		return false;
	}

	const UMeleeWeaponDefinition* LiveWeaponDef = nullptr;
	UAnimMontage* LiveMontage = nullptr;
	if (!TryResolveExecutionMontage(ActorInfo, LiveWeaponDef, LiveMontage))
	{
		return false;
	}

	if (LiveWeaponDef != ActiveExecutionWeaponDefinition || LiveMontage != ActiveExecutionMontage)
	{
		return false;
	}

	if (LiveWeaponDef->MinExecutionDistance != ActiveMinExecutionDistance
		|| LiveWeaponDef->MaxExecutionDistance != ActiveMaxExecutionDistance
		|| LiveWeaponDef->ExecutionSnapDistance != ActiveExecutionSnapDistance)
	{
		return false;
	}

	return Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
}

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

	const UMeleeWeaponDefinition* ResolvedWeapon = nullptr;
	UAnimMontage* ResolvedMontage = nullptr;
	if (!TryResolveExecutionMontage(ActorInfo, ResolvedWeapon, ResolvedMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ResolvedWeapon || !FExecutionSnapAlignment::IsExecutionDistanceRangeValid(
			ResolvedWeapon->MinExecutionDistance,
			ResolvedWeapon->MaxExecutionDistance,
			ResolvedWeapon->ExecutionSnapDistance))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveExecutionWeaponDefinition = ResolvedWeapon;
	ActiveExecutionMontage = ResolvedMontage;
	ActiveMinExecutionDistance = ResolvedWeapon->MinExecutionDistance;
	ActiveMaxExecutionDistance = ResolvedWeapon->MaxExecutionDistance;
	ActiveExecutionSnapDistance = ResolvedWeapon->ExecutionSnapDistance;
	bHasActiveExecutionDistanceSnapshot = true;

	AEnemyCharacter* TargetActor = PlayerCharacter->GetLockedTarget();
	if (!ValidateTargetPrerequisites(PlayerCharacter, TargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, ActiveMinExecutionDistance, ActiveMaxExecutionDistance, Dist2D, AngleDegrees))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector TargetForwardAtActivation = TargetActor->GetActorForwardVector();

	UAbilitySystemComponent* TargetASC = TargetActor->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag ReleaseRequestTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Release")), false);
	const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	if (!HitEventTag.IsValid() || !ReleaseRequestTag.IsValid() || !VictimStartTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. Commit Ability BEFORE victim handshake. Failure must never cancel victim actions or establish lock.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 2. Create and initialize synchronized execution session
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
	bReleaseRequestLatched = false;
	bVictimReleaseExpected = false;
	bVictimStartForwarded = false;

	// One-shot deterministic execution snap alignment
	if (!TryApplyExecutionSnap(PlayerCharacter, TargetActor, TargetForwardAtActivation))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveContext = NewObject<UPlayerFrontExecutionContext>(this);
	ActiveContext->OwningAbility = this;
	ActiveContext->Token = ActivationToken;

	BindTargetDelegates(TargetActor, ActivationToken);

	// 3. Create Montage, Hit Event, Release Request, VictimStart tasks
	WaitVictimStartEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, VictimStartTag, nullptr, false, false);
	WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false, true);
	WaitReleaseRequestEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ReleaseRequestTag, nullptr, false, false);
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ActiveExecutionMontage, 1.0f);

	if (!WaitVictimStartEventTask || !WaitHitEventTask || !WaitReleaseRequestEventTask || !MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitVictimStartEventTask->EventReceived.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnVictimStartEventReceived);
	WaitHitEventTask->EventReceived.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnHitEventReceived);
	WaitReleaseRequestEventTask->EventReceived.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnReleaseRequestEventReceived);
	MontageTask->OnCompleted.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(ActiveContext.Get(), &UPlayerFrontExecutionContext::OnMontageCancelled);

	// 4. Activate VictimStart, canonical Hit & Release listener tasks first
	WaitVictimStartEventTask->ReadyForActivation();
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestEndAbilityDuringTaskReady && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
	if (bTestInvalidateWaitVictimStartEventTaskAfterReady)
	{
		WaitVictimStartEventTask = nullptr;
	}
#endif
	if (!IsActive())
	{
		return;
	}
	if (!ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, ActivationToken) || !WaitVictimStartEventTask || !WaitVictimStartEventTask->IsActive())
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

	WaitReleaseRequestEventTask->ReadyForActivation();
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestEndAbilityDuringTaskReady && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
	if (bTestInvalidateWaitReleaseRequestTaskAfterReady)
	{
		WaitReleaseRequestEventTask = nullptr;
	}
#endif
	if (!IsActive())
	{
		return;
	}
	if (!ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, ActivationToken) || !WaitReleaseRequestEventTask || !WaitReleaseRequestEventTask->IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 5. All listeners active -> activate Montage task
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestSkipMontageTaskActivation)
	{
		return;
	}
#endif

	MontageTask->ReadyForActivation();
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestEndAbilityDuringTaskReady && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
#endif
	if (!IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, ActivationToken) || !MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimInstance* AnimInstance = PlayerCharacter->GetMesh() ? PlayerCharacter->GetMesh()->GetAnimInstance() : nullptr;
	const bool bMontageActive = AnimInstance && AnimInstance->Montage_IsActive(ActiveExecutionMontage);
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

	const FGameplayTag CanonicalHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	if (!CanonicalHitTag.IsValid() || !Payload.EventTag.MatchesTagExact(CanonicalHitTag))
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || Payload.Instigator != PlayerCharacter || Payload.Target != PlayerCharacter)
	{
		return;
	}

	if (!IsFrontExecutionAnimationFromMontage(ActiveExecutionMontage, Payload.OptionalObject.Get()))
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponentFromActorInfo();
	AEnemyCharacter* TargetActor = ReservedTarget.Get();

	if (!CharacterASC || !TargetActor || TargetActor->IsDead() || TargetActor->IsActorBeingDestroyed())
	{
		return;
	}

	float Dist2D = 0.0f;
	float AngleDegrees = 0.0f;
	if (!CheckFrontGeometry(PlayerCharacter, TargetActor, ActiveMinExecutionDistance, ActiveMaxExecutionDistance, Dist2D, AngleDegrees))
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
		if (bReleaseRequestLatched)
		{
			SendFormalReleaseToVictim(false, true);
		}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UPlayerFrontExecutionAbility::HandleReleaseRequestEventReceived(FGameplayEventData Payload, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}

	const FGameplayTag ReleaseRequestTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Release")), false);
	if (Payload.EventTag != ReleaseRequestTag)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || Payload.Instigator != PlayerCharacter || Payload.Target != PlayerCharacter)
	{
		return;
	}

	if (!IsFrontExecutionAnimationFromMontage(ActiveExecutionMontage, Payload.OptionalObject.Get()))
	{
		return;
	}

	const bool bHitResolved = bDamageEventConsumed && (
		ActiveExecutionContext->GetHitState() == EExecutionSessionHitState::NonLethal ||
		ActiveExecutionContext->GetHitState() == EExecutionSessionHitState::DeathPending);

	if (bHitResolved)
	{
		SendFormalReleaseToVictim(false, true);
	}
	else
	{
		bReleaseRequestLatched = true;
	}
}

void UPlayerFrontExecutionAbility::HandleVictimStartEventReceived(FGameplayEventData Payload, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || !ActiveExecutionContext->IsCurrent(this, InToken))
	{
		return;
	}

	if (bVictimStartForwarded)
	{
		return;
	}

	const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	if (!VictimStartTag.IsValid() || Payload.EventTag != VictimStartTag)
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter || Payload.Instigator != PlayerCharacter || Payload.Target != PlayerCharacter)
	{
		return;
	}

	if (!IsFrontExecutionAnimationFromMontage(ActiveExecutionMontage, Payload.OptionalObject.Get()))
	{
		return;
	}

	if (ActiveExecutionContext->IsReleaseSent() || ActiveExecutionContext->IsVictimReleased() || bVictimReleaseExpected)
	{
		return;
	}

	AActor* TargetActor = ReservedTarget.Get() ? ReservedTarget.Get() : ActiveExecutionContext->GetTargetActor();
	UAbilitySystemComponent* TargetASC = BoundTargetASC.Get() ? BoundTargetASC.Get() : ActiveExecutionContext->GetVictimASC();
	if (!IsValid(TargetActor) || !TargetASC || TargetActor != ActiveExecutionContext->GetTargetActor() || TargetASC != ActiveExecutionContext->GetVictimASC())
	{
		return;
	}

	bVictimStartForwarded = true;

	FGameplayEventData ForwardPayload;
	ForwardPayload.EventTag = VictimStartTag;
	ForwardPayload.Instigator = PlayerCharacter;
	ForwardPayload.Target = TargetActor;
	ForwardPayload.OptionalObject = ActiveExecutionContext;
	ForwardPayload.OptionalObject2 = Payload.OptionalObject;
	TargetASC->HandleGameplayEvent(VictimStartTag, &ForwardPayload);
}

void UPlayerFrontExecutionAbility::HandleTargetDestroyed(AActor* DestroyedActor, uint32 InToken)
{
	if (InToken != CurrentActivationToken || !IsActive() || !ActiveExecutionContext || ActiveExecutionContext->GetSourceActivationToken() != InToken)
	{
		return;
	}

	if (bVictimReleaseExpected || (ActiveExecutionContext && ActiveExecutionContext->IsVictimReleased()))
	{
		ReservedTarget = nullptr;
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

	if (bVictimReleaseExpected || (ActiveExecutionContext && ActiveExecutionContext->IsVictimReleased()))
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
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	else if (Tag == StunnedTag && NewCount == 0 && !bDamageEventConsumed && !bEndAbilityInProgress)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool UPlayerFrontExecutionAbility::SendFormalReleaseToVictim(bool bWasCancelled, bool bRequireResolvedHit)
{
	if (!ActiveExecutionContext || ActiveExecutionContext->IsReleaseSent() || ActiveExecutionContext->IsVictimReleased())
	{
		return false;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	AActor* TargetActor = ReservedTarget.Get() ? ReservedTarget.Get() : ActiveExecutionContext->GetTargetActor();
	UAbilitySystemComponent* TargetASC = BoundTargetASC.Get() ? BoundTargetASC.Get() : ActiveExecutionContext->GetVictimASC();
	const FGameplayTag ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);

	auto FailSafeCleanupVictim = [this, TargetASC]()
	{
		UnbindTargetDelegates();
		bVictimReleaseExpected = true;

		if (ActiveExecutionContext)
		{
			if (UEnemyVictimExecutionAbility* VictimAbility = Cast<UEnemyVictimExecutionAbility>(ActiveExecutionContext->GetVictimAbility()))
			{
				if (VictimAbility->IsActive())
				{
					VictimAbility->CancelAbility(VictimAbility->GetCurrentAbilitySpecHandle(), VictimAbility->GetCurrentActorInfo(), VictimAbility->GetCurrentActivationInfo(), true);
				}
			}

			ActiveExecutionContext->InvalidateSession();
		}

		if (TargetASC)
		{
			const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
			if (VictimLockedTag.IsValid() && TargetASC->HasMatchingGameplayTag(VictimLockedTag))
			{
				TargetASC->RemoveLooseGameplayTag(VictimLockedTag);
			}
			if (InvulnerableTag.IsValid() && TargetASC->HasMatchingGameplayTag(InvulnerableTag))
			{
				TargetASC->RemoveLooseGameplayTag(InvulnerableTag);
			}
		}
	};

	if (!PlayerCharacter || !TargetActor || !TargetASC || !ReleaseEventTag.IsValid())
	{
		FailSafeCleanupVictim();
		return false;
	}

	UnbindTargetDelegates();
	bVictimReleaseExpected = true;

	if (!ActiveExecutionContext->TryBeginRelease(this, CurrentActivationToken, bWasCancelled, bRequireResolvedHit))
	{
		FailSafeCleanupVictim();
		return false;
	}

	FGameplayEventData ReleasePayload;
	ReleasePayload.EventTag = ReleaseEventTag;
	ReleasePayload.Instigator = PlayerCharacter;
	ReleasePayload.Target = TargetActor;
	ReleasePayload.OptionalObject = ActiveExecutionContext;
	TargetASC->HandleGameplayEvent(ReleaseEventTag, &ReleasePayload);

	if (!ActiveExecutionContext->IsVictimReleased())
	{
		FailSafeCleanupVictim();
	}

	return true;
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
	if (!IsActive() || bEndAbilityInProgress)
	{
		return;
	}

	bEndAbilityInProgress = true;

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);

	// 1. Fallback release to victim if not already sent
	if (ActiveExecutionContext && !ActiveExecutionContext->IsReleaseSent())
	{
		const bool bHadResolvedHit = (ActiveExecutionContext->GetHitState() == EExecutionSessionHitState::NonLethal ||
			ActiveExecutionContext->GetHitState() == EExecutionSessionHitState::DeathPending);
		const bool bIsFallbackCancel = bWasCancelled || !bHadResolvedHit;
		SendFormalReleaseToVictim(bIsFallbackCancel, /*bRequireResolvedHit=*/false);
	}

	// 2. Unbind target delegates
	UnbindTargetDelegates();

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
	if (WaitVictimStartEventTask)
	{
		WaitVictimStartEventTask->EndTask();
		WaitVictimStartEventTask = nullptr;
	}

	if (WaitHitEventTask)
	{
		WaitHitEventTask->EndTask();
		WaitHitEventTask = nullptr;
	}

	if (WaitReleaseRequestEventTask)
	{
		WaitReleaseRequestEventTask->EndTask();
		WaitReleaseRequestEventTask = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (PlayerCharacter)
	{
		if (UAnimInstance* AnimInstance = PlayerCharacter->GetMesh() ? PlayerCharacter->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (ActiveExecutionMontage && AnimInstance->Montage_IsActive(ActiveExecutionMontage))
			{
				AnimInstance->Montage_Stop(0.2f, ActiveExecutionMontage);
			}
		}
	}

	ReservedTarget = nullptr;
	bDamageEventConsumed = false;
	bReleaseRequestLatched = false;
	bVictimReleaseExpected = false;
	bVictimStartForwarded = false;
	ActiveMinExecutionDistance = 0.0f;
	ActiveMaxExecutionDistance = 0.0f;
	ActiveExecutionSnapDistance = 0.0f;
	bHasActiveExecutionDistanceSnapshot = false;
	ActiveExecutionWeaponDefinition = nullptr;
	ActiveExecutionMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEndAbilityInProgress = false;
}

#if WITH_DEV_AUTOMATION_TESTS
void UPlayerFrontExecutionAbility::SetTestExecutionMontage(UAnimMontage* Montage)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!Player || Player->IsActorBeingDestroyed())
	{
		return;
	}

	UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
	if (UMeleeWeaponDefinition* MeleeWeapon = EquipComp ? EquipComp->GetEquippedMainHandMelee() : nullptr)
	{
		MeleeWeapon->FrontExecutionMontage = Montage;
	}
}

void UPlayerFrontExecutionAbility::SetTestExecutionDistances(float InMin, float InMax)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!Player || Player->IsActorBeingDestroyed())
	{
		return;
	}

	UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
	if (UMeleeWeaponDefinition* MeleeWeapon = EquipComp ? EquipComp->GetEquippedMainHandMelee() : nullptr)
	{
		MeleeWeapon->MinExecutionDistance = InMin;
		MeleeWeapon->MaxExecutionDistance = InMax;
	}
}

void UPlayerFrontExecutionAbility::TestTriggerReleaseRequestEvent(const FGameplayEventData& Payload)
{
	HandleReleaseRequestEventReceived(Payload, CurrentActivationToken);
}

void UPlayerFrontExecutionAbility::TestTriggerVictimStartEvent(const FGameplayEventData& Payload)
{
	HandleVictimStartEventReceived(Payload, CurrentActivationToken);
}
#endif
