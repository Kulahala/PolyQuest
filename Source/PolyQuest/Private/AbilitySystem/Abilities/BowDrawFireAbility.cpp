#include "AbilitySystem/Abilities/BowDrawFireAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "Combat/Projectile/CombatProjectileTargeting.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "PolyQuest.h"

UBowDrawFireAbility::UBowDrawFireAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FGameplayTag PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	const FGameplayTag AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag JumpBlockTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);

	if (PrimaryAttackAbilityTag.IsValid())
	{
		AbilityTags.AddTag(PrimaryAttackAbilityTag);
	}
	if (AttackingStateTag.IsValid())
	{
		AbilityTags.AddTag(AttackingStateTag);
	}

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));

	if (AttackingStateTag.IsValid())
	{
		ActivationOwnedTags.AddTag(AttackingStateTag);
	}
	if (JumpBlockTag.IsValid())
	{
		ActivationOwnedTags.AddTag(JumpBlockTag);
	}

	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	DrawReadyEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Bow.DrawReady")), false);
	ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Bow.Release")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	DodgeCancelWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	DodgeCancelWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	RateWindowBeginEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	RateWindowEndEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
	DodgeCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	DefenseCancelableStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
	ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
}

bool UBowDrawFireAbility::CanActivateAbility(
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

	const APlayerCharacter* PlayerCharacter = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!PlayerCharacter)
	{
		return false;
	}

	if (!PrimaryAttackInputTag.IsValid() || !PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag))
	{
		return false;
	}

	const UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UBowWeaponDefinition* BowDef = EquipmentComp ? Cast<UBowWeaponDefinition>(EquipmentComp->GetCurrentMainHandWeapon()) : nullptr;
	if (!BowDef || !BowDef->DefaultProjectileDefinition)
	{
		return false;
	}

	return true;
}

void UBowDrawFireAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 1. Reset transient runtime state first before any early return/failure branch
	bEndAbilityInProgress = false;
	bDodgeCancelable = false;
	bChargingApplied = false;
	bRateWindowApplied = false;
	bSpawnedProjectile = false;
	bReleaseRequested = false;
	MobileBowMoveSpeedEffectHandle.Invalidate();
	BowState = EBowState::Inactive;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!PlayerCharacter)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility failed activation: missing APlayerCharacter avatar."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 2. Validate required gameplay tags
	if (!PrimaryAttackInputTag.IsValid() || !DrawReadyEventTag.IsValid() || !ReleaseEventTag.IsValid()
		|| !InputReleasedEventTag.IsValid() || !InputCanceledEventTag.IsValid()
		|| !DodgeCancelWindowBeginEventTag.IsValid() || !DodgeCancelWindowEndEventTag.IsValid()
		|| !RateWindowBeginEventTag.IsValid() || !RateWindowEndEventTag.IsValid()
		|| !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid()
		|| !ChargingStateTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed activation: missing required gameplay tags."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Fail-closed if Primary attack input is not held upon activation
	if (!PlayerCharacter->IsCombatInputHeld(PrimaryAttackInputTag))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed activation: Primary attack input is not held."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 4. Validate Bow weapon definition & projectile definition
	const UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UBowWeaponDefinition* BowDef = EquipmentComp ? Cast<UBowWeaponDefinition>(EquipmentComp->GetCurrentMainHandWeapon()) : nullptr;
	if (!BowDef || !BowDef->DefaultProjectileDefinition)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed activation: equipped weapon is not a valid Bow with DefaultProjectileDefinition."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 5. Validate MobileBowMoveSpeedGameplayEffectClass is configured
	if (!MobileBowMoveSpeedGameplayEffectClass)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed activation: missing MobileBowMoveSpeedGameplayEffectClass."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 6. Validate Bow Montage and required section names
	if (!BowMontage
		|| DrawSectionName.IsNone() || BowMontage->GetSectionIndex(DrawSectionName) == INDEX_NONE
		|| HoldSectionName.IsNone() || BowMontage->GetSectionIndex(HoldSectionName) == INDEX_NONE
		|| ReleaseSectionName.IsNone() || BowMontage->GetSectionIndex(ReleaseSectionName) == INDEX_NONE)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed activation: invalid BowMontage or missing Draw/Hold/Release sections."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 7. Create Montage Task and Event Wait Tasks
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		BowMontage,
		1.0f,
		DrawSectionName);

	WaitDrawReadyTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DrawReadyEventTag);
	WaitReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ReleaseEventTag);
	WaitInputReleasedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputReleasedEventTag);
	WaitInputCanceledTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, InputCanceledEventTag);
	DodgeCancelWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowBeginEventTag);
	DodgeCancelWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DodgeCancelWindowEndEventTag);
	RateWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowBeginEventTag);
	RateWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RateWindowEndEventTag);

	if (!MontageTask || !WaitDrawReadyTask || !WaitReleaseTask || !WaitInputReleasedTask || !WaitInputCanceledTask
		|| !DodgeCancelWindowBeginTask || !DodgeCancelWindowEndTask || !RateWindowBeginTask || !RateWindowEndTask)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed to create required ability tasks."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UBowDrawFireAbility::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UBowDrawFireAbility::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UBowDrawFireAbility::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UBowDrawFireAbility::OnMontageCancelled);

	WaitDrawReadyTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnDrawReadyEvent);
	WaitDrawReadyTask->ReadyForActivation();

	WaitReleaseTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnReleaseAnimEvent);
	WaitReleaseTask->ReadyForActivation();

	WaitInputReleasedTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnInputReleased);
	WaitInputReleasedTask->ReadyForActivation();

	WaitInputCanceledTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnInputCanceled);
	WaitInputCanceledTask->ReadyForActivation();

	DodgeCancelWindowBeginTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnDodgeCancelWindowBegin);
	DodgeCancelWindowBeginTask->ReadyForActivation();

	DodgeCancelWindowEndTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnDodgeCancelWindowEnd);
	DodgeCancelWindowEndTask->ReadyForActivation();

	RateWindowBeginTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnRateWindowBegin);
	RateWindowBeginTask->ReadyForActivation();

	RateWindowEndTask->EventReceived.AddDynamic(this, &UBowDrawFireAbility::OnRateWindowEnd);
	RateWindowEndTask->ReadyForActivation();

	PlayerCharacter->RegisterBowAimRequester(this);
	MontageTask->ReadyForActivation();

	if (bEndAbilityInProgress)
	{
		return;
	}

	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsActive(BowMontage))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed to start BowMontage."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartMobileBowMoveSpeedEffect())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility on '%s' failed to apply MobileBowMoveSpeedGameplayEffect."), *GetNameSafe(PlayerCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BowState = EBowState::Drawing;
	bReleaseRequested = false;
}

bool UBowDrawFireAbility::IsValidAvatarEventPayload(const FGameplayEventData& Payload) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar != nullptr && Payload.Instigator == Avatar && Payload.Target == Avatar;
}

void UBowDrawFireAbility::OnDrawReadyEvent(FGameplayEventData Payload)
{
	if (BowState != EBowState::Drawing || !IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	BowState = EBowState::Holding;

	if (bReleaseRequested)
	{
		TriggerRelease();
		return;
	}

	SetCharging(true);
}

void UBowDrawFireAbility::OnInputReleased(FGameplayEventData Payload)
{
	if (bEndAbilityInProgress)
	{
		return;
	}

	if (PrimaryAttackInputTag.IsValid() && !Payload.InstigatorTags.HasTagExact(PrimaryAttackInputTag))
	{
		return;
	}

	if (!IsValidAvatarEventPayload(Payload))
	{
		return;
	}

	if (BowState == EBowState::Drawing)
	{
		// Early release during draw: will transition to release once DrawReady is received.
		bReleaseRequested = true;
	}
	else if (BowState == EBowState::Holding)
	{
		TriggerRelease();
	}
}

void UBowDrawFireAbility::OnInputCanceled(FGameplayEventData Payload)
{
	if (bEndAbilityInProgress)
	{
		return;
	}

	if (PrimaryAttackInputTag.IsValid() && !Payload.InstigatorTags.HasTagExact(PrimaryAttackInputTag))
	{
		return;
	}

	if (!IsValidAvatarEventPayload(Payload))
	{
		return;
	}

	// Input cancellation (e.g. window focus loss) aborts without firing.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBowDrawFireAbility::TriggerRelease()
{
	if (BowState != EBowState::Holding || bEndAbilityInProgress)
	{
		return;
	}

	SetCharging(false);
	BowState = EBowState::Releasing;

	if (!ReleaseSectionName.IsNone())
	{
		MontageJumpToSection(ReleaseSectionName);
	}
}

void UBowDrawFireAbility::OnReleaseAnimEvent(FGameplayEventData Payload)
{
	if (BowState != EBowState::Releasing || bSpawnedProjectile || !IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	SpawnProjectile();
}

void UBowDrawFireAbility::SpawnProjectile()
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	if (!PlayerCharacter || !World)
	{
		return;
	}

	const UWeaponEquipmentComponent* EquipmentComp = PlayerCharacter->FindComponentByClass<UWeaponEquipmentComponent>();
	const UBowWeaponDefinition* BowDef = EquipmentComp ? Cast<UBowWeaponDefinition>(EquipmentComp->GetCurrentMainHandWeapon()) : nullptr;
	if (!BowDef || !BowDef->DefaultProjectileDefinition)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility::SpawnProjectile failed: invalid Bow definition or missing DefaultProjectileDefinition."));
		return;
	}

	const UProjectileDefinition* ProjDef = BowDef->DefaultProjectileDefinition;

	FTransform LaunchSocketTransform;
	if (!EquipmentComp->TryGetEquippedMainHandDisplaySocketTransform(BowDef->LaunchSocketName, LaunchSocketTransform))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility::SpawnProjectile failed: launch socket '%s' transform could not be resolved on '%s'."),
			*BowDef->LaunchSocketName.ToString(), *GetNameSafe(PlayerCharacter));
		return;
	}

	const FVector LaunchLocation = LaunchSocketTransform.GetLocation();

	// 1. Resolve pointer direction or fallback to action facing
	FVector PointerDirection;
	if (!PlayerCharacter->TryGetBowAimWorldDirection(PointerDirection))
	{
		PointerDirection = PlayerCharacter->GetActionWorldDirection();
	}

	FVector LaunchDirection = PointerDirection;
	TWeakObjectPtr<AActor> SelectedTarget = nullptr;
	FVector TargetAimPoint = FVector::ZeroVector;

	// 2. Target assistance evaluation on release: a validated Player lock wins, otherwise keep B2 auto-selection.
	if (ProjDef->bEnableTargetAssist)
	{
		if (AEnemyCharacter* LockedTarget = PlayerCharacter->ResolveValidLockedTarget())
		{
			const FVector LockedTargetAimPoint = FCombatProjectileTargeting::GetTargetAimPoint(LockedTarget);
			if (!LockedTargetAimPoint.ContainsNaN())
			{
				SelectedTarget = LockedTarget;
				TargetAimPoint = LockedTargetAimPoint;
			}
		}

		if (!SelectedTarget.IsValid())
		{
			FCombatProjectileTargetFilter Filter;
			Filter.MaxHorizontalDistance = ProjDef->TargetAssistMaxDistance;
			Filter.MaxAngleDegrees = ProjDef->TargetAssistMaxAngleDegrees;
			Filter.MaxHeightDelta = ProjDef->TargetAssistMaxHeightDelta;
			Filter.MaxPitchDegrees = ProjDef->TargetAssistMaxPitchDegrees;

#if WITH_DEV_AUTOMATION_TESTS
			Filter.TestScreenProjectionHook = TestTargetAssistScreenProjectionHook;
#endif

			FCombatProjectileTargetCandidate Candidate;
			const bool bFoundTarget = FCombatProjectileTargeting::TryFindBestTargetCandidate(
				World,
				PlayerCharacter,
				GetAbilitySystemComponentFromActorInfo(),
				LaunchLocation,
				PointerDirection,
				Filter,
				Candidate);

			if (bFoundTarget && Candidate.TargetActor.IsValid())
			{
				SelectedTarget = Candidate.TargetActor;
				TargetAimPoint = Candidate.AimPoint;
			}
		}
	}

	const FRotator LaunchRotation = LaunchDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerCharacter;
	SpawnParams.Instigator = PlayerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ACombatProjectile::StaticClass();
	ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>(
		ClassToSpawn,
		LaunchLocation,
		LaunchRotation,
		SpawnParams);

	if (!Projectile)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility::SpawnProjectile failed to spawn projectile actor."));
		return;
	}

	FCombatProjectileLaunchRequest LaunchRequest;
	LaunchRequest.Definition = ProjDef;
	LaunchRequest.SourceActor = PlayerCharacter;
	LaunchRequest.SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	LaunchRequest.AbilityLevel = GetAbilityLevel();
	LaunchRequest.InitialFlightDirection = LaunchDirection;
	LaunchRequest.TargetActor = SelectedTarget;
	LaunchRequest.InitialTargetAimPoint = TargetAimPoint;
	LaunchRequest.bEnableLimitedHoming = ProjDef->bEnableLimitedHoming;
	LaunchRequest.HomingStartDelaySeconds = ProjDef->HomingStartDelaySeconds;
	LaunchRequest.HomingDurationSeconds = ProjDef->HomingDurationSeconds;
	LaunchRequest.HomingTurnRateDegreesPerSecond = ProjDef->HomingTurnRateDegreesPerSecond;
	LaunchRequest.HomingMaxTotalTurnDegrees = ProjDef->HomingMaxTotalTurnDegrees;
	LaunchRequest.TargetAssistMaxDistance = ProjDef->TargetAssistMaxDistance;
	LaunchRequest.TargetAssistMaxHeightDelta = ProjDef->TargetAssistMaxHeightDelta;

	if (!Projectile->InitializeProjectile(LaunchRequest))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("UBowDrawFireAbility::SpawnProjectile: projectile failed to initialize with definition."));
		return;
	}

#if !(UE_BUILD_SHIPPING)
	DrawDebugDirectionalArrow(World, LaunchLocation, LaunchLocation + LaunchDirection * 500.0f, 60.0f, FColor::Magenta, false, 2.5f, 0, 3.5f);
	if (SelectedTarget.IsValid())
	{
		DrawDebugSphere(World, TargetAimPoint, 25.0f, 16, FColor::Green, false, 2.5f, 0, 3.0f);
		DrawDebugLine(World, LaunchLocation, TargetAimPoint, FColor::Green, false, 2.5f, 0, 1.5f);
	}
#endif

	bSpawnedProjectile = true;
}

void UBowDrawFireAbility::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBowDrawFireAbility::OnMontageBlendOut()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBowDrawFireAbility::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBowDrawFireAbility::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBowDrawFireAbility::EndAbility(
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

	ClearMobileBowMoveSpeedEffect();
	SetCharging(false);
	SetDodgeCancelable(false);
	RestoreBaselineMontageRate();

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		PlayerCharacter->UnregisterBowAimRequester(this);
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (WaitDrawReadyTask)
	{
		WaitDrawReadyTask->EndTask();
		WaitDrawReadyTask = nullptr;
	}

	if (WaitReleaseTask)
	{
		WaitReleaseTask->EndTask();
		WaitReleaseTask = nullptr;
	}

	if (WaitInputReleasedTask)
	{
		WaitInputReleasedTask->EndTask();
		WaitInputReleasedTask = nullptr;
	}

	if (WaitInputCanceledTask)
	{
		WaitInputCanceledTask->EndTask();
		WaitInputCanceledTask = nullptr;
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

	if (RateWindowBeginTask)
	{
		RateWindowBeginTask->EndTask();
		RateWindowBeginTask = nullptr;
	}

	if (RateWindowEndTask)
	{
		RateWindowEndTask->EndTask();
		RateWindowEndTask = nullptr;
	}

	if (BowMontage && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		MontageStop(0.1f);
	}

	BowState = EBowState::Inactive;
	bReleaseRequested = false;
	bSpawnedProjectile = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UBowDrawFireAbility::IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bEndAbilityInProgress || !BowMontage || !AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return false;
	}

	const UObject* PayloadObject = Payload.OptionalObject.Get();
	if (!PayloadObject)
	{
		return false;
	}

	if (PayloadObject == BowMontage.Get())
	{
		return true;
	}

	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PayloadObject))
	{
		for (const FSlotAnimationTrack& Track : BowMontage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (Segment.GetAnimReference() == Sequence)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void UBowDrawFireAbility::OnDodgeCancelWindowBegin(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(true);
	}
}

void UBowDrawFireAbility::OnDodgeCancelWindowEnd(FGameplayEventData Payload)
{
	if (IsGameplayEventFromActiveMontage(Payload))
	{
		SetDodgeCancelable(false);
	}
}

void UBowDrawFireAbility::OnRateWindowBegin(FGameplayEventData Payload)
{
	if (bRateWindowApplied || !IsGameplayEventFromActiveMontage(Payload) || Payload.EventMagnitude <= 0.0f)
	{
		return;
	}

	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && BowMontage && AnimInstance->Montage_IsActive(BowMontage.Get()))
	{
		AnimInstance->Montage_SetPlayRate(BowMontage.Get(), Payload.EventMagnitude);
		bRateWindowApplied = true;
	}
}

void UBowDrawFireAbility::OnRateWindowEnd(FGameplayEventData Payload)
{
	if (!IsGameplayEventFromActiveMontage(Payload))
	{
		return;
	}

	RestoreBaselineMontageRate();
}

void UBowDrawFireAbility::RestoreBaselineMontageRate()
{
	if (!bRateWindowApplied)
	{
		return;
	}

	bRateWindowApplied = false;
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && BowMontage && AnimInstance->Montage_IsActive(BowMontage.Get()))
	{
		AnimInstance->Montage_SetPlayRate(BowMontage.Get(), 1.0f);
	}
}

void UBowDrawFireAbility::SetCharging(bool bShouldCharge)
{
	if (bShouldCharge)
	{
		if (bEndAbilityInProgress || bChargingApplied)
		{
			return;
		}

		if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
		{
			if (ChargingStateTag.IsValid())
			{
				AbilitySystemComponent->AddLooseGameplayTag(ChargingStateTag);
				bChargingApplied = true;
			}
		}
		return;
	}

	if (!bChargingApplied)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		if (ChargingStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(ChargingStateTag);
		}
	}

	bChargingApplied = false;
}

void UBowDrawFireAbility::SetDodgeCancelable(bool bShouldBeCancelable)
{
	if (bShouldBeCancelable)
	{
		if (bEndAbilityInProgress || bDodgeCancelable)
		{
			return;
		}

		UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
		if (!AbilitySystemComponent || !DodgeCancelableStateTag.IsValid() || !DefenseCancelableStateTag.IsValid())
		{
			return;
		}

		AbilitySystemComponent->AddLooseGameplayTag(DodgeCancelableStateTag);
		AbilitySystemComponent->AddLooseGameplayTag(DefenseCancelableStateTag);
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
		if (DefenseCancelableStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(DefenseCancelableStateTag);
		}
	}

	bDodgeCancelable = false;
}

bool UBowDrawFireAbility::StartMobileBowMoveSpeedEffect()
{
	if (MobileBowMoveSpeedEffectHandle.IsValid())
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
	const UGameplayEffect* MoveSpeedEffect = MobileBowMoveSpeedGameplayEffectClass
		? MobileBowMoveSpeedGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;

	if (!AbilitySystemComponent || !PlayerCharacter || !MoveSpeedEffect)
	{
		return false;
	}

	MobileBowMoveSpeedEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
		MoveSpeedEffect,
		GetAbilityLevel(),
		AbilitySystemComponent->MakeEffectContext());

	if (!MobileBowMoveSpeedEffectHandle.IsValid())
	{
		return false;
	}

	PlayerCharacter->CancelSprintAbility();
	return true;
}

void UBowDrawFireAbility::ClearMobileBowMoveSpeedEffect()
{
	if (!MobileBowMoveSpeedEffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(MobileBowMoveSpeedEffectHandle);
	}

	MobileBowMoveSpeedEffectHandle.Invalidate();
}
