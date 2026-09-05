// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Character/BaseCharacter.h"
#include "Character/Player/PlayerLockOnTargeting.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UCameraShakeBase;
class UGameplayEffect;
class UInputAction;
class UInputComponent;
class UPlayerGuardAbility;
class UPlayerParryAbility;
class AActor;
class AWorldWeaponPickup;
class UGameplayAbility;
class UMeleeWeaponDefinition;
class UWeaponEquipmentComponent;
class UAIPerceptionStimuliSourceComponent;
class UMotionWarpingComponent;
class USpringArmComponent;
class AController;
class AEnemyCharacter;
class APlayerCameraManager;
class APlayerController;
class USoundBase;
class UPlayerCombatFeedbackDataAsset;
class UMaterialParameterCollection;
enum class EHitReactionTier : uint8;
struct FGameplayEffectSpec;
struct FInputActionValue;
struct FOnAttributeChangeData;

/**
 * Player-specific camera and input layer built on the shared GAS character.
 */
UCLASS(Blueprintable)
class POLYQUEST_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	/** Fixed-world camera boom for the oblique Perspective composition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera attached to the spring arm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;

	/** Registers this player as the explicit Sight source for the first enemy fixture. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Perception", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAIPerceptionStimuliSourceComponent> SightStimuliSource;

	/** Motion Warping component owned by this Player for root-motion contact assists. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

protected:
	/** Input action used to start and stop jumping. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Input action used for movement. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Retained authored gamepad-look action; this fixed-camera route does not bind it. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Retained authored mouse-look action; this fixed-camera route does not bind it. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Shared physical input that routes to the equipped weapon's primary combat ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* PrimaryAttackAction;

	/** Shared physical input that expresses aim intent and may route to a future Aim ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AimAction;

	/** Shared physical input that expresses held Guard intent through the active Defense Profile. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* GuardAction;

	/** Shared physical input that expresses Parry intent through the active Defense Profile. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ParryAction;

	/** Direct ability-slot inputs; index zero is Input.AbilitySlot.1. */
	UPROPERTY(EditAnywhere, Category="Input", meta=(EditFixedSize))
	TArray<UInputAction*> AbilitySlotActions;

	/** Shared physical input: release before the threshold requests Dodge; a held press resolves to Sprint. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DodgeSprintAction;

	/** Input action used for world interaction (such as equipment pickup). */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	/** Middle-mouse action that acquires or clears the local screen-space lock target. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LockOnAction;

	/** Mouse-wheel axis action that cycles a currently valid lock target. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* TargetCycleAction;

	/** Hold duration at which the shared Dodge/Sprint input resolves to Sprint intent. */
	UPROPERTY(EditDefaultsOnly, Category="Input", meta=(ClampMin="0.01", UIMin="0.01", ToolTip="共享闪避/冲刺按键长按判定时间阈值（秒）；短于此时间释放触发闪避，达到此时间触发冲刺。"))
	float DodgeSprintHoldThresholdSeconds = 0.15f;

	/** Continuous periodic GameplayEffect that recovers Stamina when its tag requirements allow it. */
	UPROPERTY(EditDefaultsOnly, Category="GAS|Stamina", meta=(ToolTip="玩家常驻周期性恢复体力的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> StaminaRegenGameplayEffectClass;

	/** Infinite move-speed effect retained while the Player is recovering from Stamina exhaustion. */
	UPROPERTY(EditDefaultsOnly, Category="GAS|Stamina", meta=(ToolTip="玩家体力耗尽力竭期间施加的移速限制 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> ExhaustionMoveSpeedGameplayEffectClass;

	/** Required default weapon equipped once at BeginPlay; missing is a fail-visible configuration error. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "玩家 BeginPlay 时默认装备的武器定义资产（MeleeWeaponDefinition）。"))
	TObjectPtr<UMeleeWeaponDefinition> DefaultEquippedWeapon;

	/** Minimum duration in seconds before exhaustion recovery can clear the exhausted state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Stamina", meta = (ClampMin = "0.0"))
	float ExhaustionMinimumDurationSeconds = 3.0f;

	/** 更新摄像机到角色的视线遮挡状态，并动态驱动 MPC_PlayerGlobals（PlayerPosition 与平滑 TunnelRadius） */
	void UpdateSeeThroughOcclusion(float DeltaSeconds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	TObjectPtr<UMaterialParameterCollection> PlayerGlobalsMPC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float MaxTunnelRadius = 280.0f;

	/** 遮挡发生时透视开孔展开插值速度（默认 20.0f，极速响应视野） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float TunnelRadiusOpenInterpSpeed = 20.0f;

	/** 遮挡解除时透视孔洞收拢插值速度（默认 6.0f，从容淡出杜绝抽搐） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float TunnelRadiusCloseInterpSpeed = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float SeeThroughChestZOffset = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	TEnumAsByte<ECollisionChannel> SeeThroughTraceChannel = ECC_Visibility;

	/** 视线遮挡检测扫掠球体半径（模拟角色体宽，默认 25cm） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float SeeThroughSweepRadius = 25.0f;

	/** 视线扫掠起点前推安全距离（避免误判镜头跟前近景结构，默认 150cm） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float SeeThroughNearClipOffset = 150.0f;

	/** 天花板大范围消隐半径（默认 1500cm，游戏运行时动态推送到 MPC，保持编辑模式下实体可见便于地编） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rendering|SeeThrough")
	float MaxCeilingRadius = 1500.0f;

	float CurrentTunnelRadius = 0.0f;

public:
	APlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PawnClientRestart() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	/** Initialize Enhanced Input bindings for the player pawn. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Convert the Enhanced Input movement action to gameplay movement. */
	void Move(const FInputActionValue& Value);

	/** Retained legacy look route; fixed-camera v1 intentionally ignores it. */
	void Look(const FInputActionValue& Value);

	/** Clears cached directional input after movement input ends. */
	void ClearMoveInput(const FInputActionValue& Value);

	/** Handles an interact-start input from the InteractAction. */
	void HandleInteractStarted(const FInputActionValue& Value);

	/** Optional presentation hook called when a world pickup interaction concludes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Equipment", meta = (DisplayName = "On World Pickup Interaction Result"))
	void OnWorldPickupInteractionResult(bool bSuccess, UWeaponDefinition* PickedUpWeapon);

public:
	/** Handles movement input from controls or UI interfaces. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Fixed-camera v1 intentionally ignores controller look input. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles a jump-start input from controls or UI interfaces. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles a jump-end input from controls or UI interfaces. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** True when the ability class is granted by this character's StartupAbilities; C++-only equipment preflight query. */
	bool IsStartupAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const;

	/** Returns whether this physical combat input is currently held. */
	UFUNCTION(BlueprintPure, Category="Combat|Input")
	bool IsCombatInputHeld(FGameplayTag InputIntentTag) const;

	/** Returns the elapsed hold duration for an active physical combat input. */
	UFUNCTION(BlueprintPure, Category="Combat|Input")
	float GetCombatInputHeldDuration(FGameplayTag InputIntentTag) const;

	/** True when held Guard input and current character state permit a new Guard Ability request. */
	bool CanAttemptGuard() const;

	/** Resolves one valid incoming melee contact through the active Guard Ability. */
	bool TryGuardIncomingMeleeHit(AActor* AttackingActor, float GuardStaminaDamage, const FHitResult& HitResult);

	/** Resolves one valid incoming contact through the active Parry first (if allowed), then the active Guard. */
	bool TryResolveIncomingDefense(
		AActor* AttackingActor,
		float GuardStaminaDamage,
		const FHitResult& HitResult,
		bool bAllowParry);

	/** Returns the configured player combat feedback data asset, or nullptr if unset or invalid type. */
	UPlayerCombatFeedbackDataAsset* GetPlayerCombatFeedbackData() const;

	/** Triggers the configured Big hit camera shake on successful melee Parry. */
	void TriggerParrySuccessCameraShake();

	/** Triggers attacker-side camera shake feedback when this Player inflicts Small/Big/Launch damage on an enemy. */
	void TriggerAttackerImpactCameraShake(EHitReactionTier ReactionTier);

	/** Triggers attacker-side camera shake feedback when this Player inflicts execution impact on an enemy. */
	void TriggerExecutionImpactCameraShake();

	/** Cancels a live Parry after an external airborne transition; it never clears Guard resume eligibility. */
	void CancelActiveParry();

	/** Cancels a live Guard only after the caller has confirmed its own action Montage started. */
	void CancelActiveGuardAfterConfirmedAction(bool bResumeAfterAttack);

	/** Clears the one-shot pre-held-Guard resume qualification and its pending retry. */
	void ClearGuardResumeEligibility();

	/** Makes Guard require physical RMB release after a successfully started Guard Break. */
	void MarkGuardRequiresReleaseAfterGuardBreak();

	/** Resolves the current action-facing direction from camera-relative movement or actor forward. */
	FVector GetActionWorldDirection() const;

	/** Applies the current action-facing direction as horizontal actor yaw once at action startup. */
	void ApplyActionFacing();

	/** Applies one lock-aware attack facing, falling back to the current action-facing direction. */
	void ApplyLockAwareActionFacing();

	/** Applies Dodge facing: camera-relative movement wins; without movement input, a valid lock wins. */
	void ApplyDodgeFacing();

	/** C++-only raw nullable lock read; gameplay callers requiring current validity use ResolveValidLockedTarget(). */
	AEnemyCharacter* GetLockedTarget() const { return LockedTarget.Get(); }

	/** Validates the current lock once and returns its surviving or death-retargeted Enemy; may clear the lock. */
	AEnemyCharacter* ResolveValidLockedTarget();

	/** Sets a one-shot melee motion warp target on this Player. C++ narrow bridge. */
	bool SetMeleeMotionWarpTarget(FName WarpTargetName, const FTransform& TargetTransform);

	/** Clears all melee motion warp targets from this Player. C++ narrow bridge. */
	void ClearMeleeMotionWarpTargets();

	/** Clears melee motion warp targets if the current locked target matches InvalidatedTarget. */
	void ClearMeleeMotionWarpTargetsForInvalidatedTarget(const class AEnemyCharacter* InvalidatedTarget);

	/** True only when physical Sprint intent and current movement state permit a new Sprint request. */
	bool CanAttemptSprint() const;

	/** Returns whether the ASC currently owns the real active Sprint state tag. */
	bool HasActiveSprint() const;

	/** True only when current physical input still qualifies PrimaryAttack for the active Sprint route. */
	bool ShouldRequestSprintAttack() const;

	/** Cancels the active Sprint ability without storing a second Sprint-state boolean. */
	void CancelSprintAbility();

	/** Applies the air-only Sprint Jump speed effect until landing. */
	bool ApplySprintJumpAirSpeed(TSubclassOf<UGameplayEffect> SprintJumpAirSpeedGameplayEffectClass);

	/** Removes the air-only Sprint Jump speed effect after landing, teardown, or a failed takeoff. */
	void ClearSprintJumpAirSpeed();

	/** Closes the exhausted-until-release gate when a Sprint drain reaches zero. */
	void MarkSprintRequiresReleaseAfterExhaustion();

	/** Registers an active requester for continuous horizontal Bow aiming. */
	bool RegisterBowAimRequester(const UObject* Requester);

	/** Unregisters a Bow aiming requester; only the active requester can unregister. */
	void UnregisterBowAimRequester(const UObject* Requester);

	/** Returns true if a valid Bow aim requester is active. */
	bool HasActiveBowAimRequester() const;

	/** Attempts to get the current horizontal Bow aim direction. */
	bool TryGetBowAimWorldDirection(FVector& OutDirection) const;

	/** Registers an overlapping world weapon pickup candidate. */
	void RegisterWorldPickupCandidate(AWorldWeaponPickup* Pickup);

	/** Unregisters an overlapping world weapon pickup candidate. */
	void UnregisterWorldPickupCandidate(AWorldWeaponPickup* Pickup);

	/** Arbitrates nearest valid candidate and synchronizes controller prompt. */
	void RefreshWorldPickupInteractionPrompt();

	/** Clears interaction candidates, timer, and hides prompt. */
	void ClearWorldPickupInteractionState();

	/** Returns the currently arbitrated world pickup candidate. */
	AWorldWeaponPickup* GetCurrentWorldPickupCandidate() const { return CurrentWorldPickupCandidate.Get(); }

	/** Handles character movement updates to refresh pickup candidates when moving. */
	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	/**
	 * Intersects a 3D ray with a horizontal plane at Z = PlaneZ.
	 * Returns true if the ray intersects the plane at T > 0 with finite coordinates.
	 */
	static bool CalculateRayPlaneIntersection(const FVector& WorldOrigin, const FVector& WorldDirection, float PlaneZ, FVector& OutIntersectionPoint);

	virtual void Tick(float DeltaSeconds) override;

	/** Returns the player camera boom. */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns the player follow camera. */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	float GetExhaustionMinimumDurationSeconds() const { return ExhaustionMinimumDurationSeconds; }
	void SetExhaustionMinimumDurationSeconds(float InDuration) { ExhaustionMinimumDurationSeconds = FMath::Max(0.0f, InDuration); }

#if WITH_DEV_AUTOMATION_TESTS
	/** Configures all authored startup inputs as one pre-BeginPlay native fixture operation. */
	void ConfigureTestStartupFixture(
		UMeleeWeaponDefinition* InDefaultEquippedWeapon,
		TSubclassOf<UGameplayEffect> InStaminaRegenGameplayEffectClass,
		TSubclassOf<UGameplayEffect> InExhaustionMoveSpeedGameplayEffectClass,
		UInputAction* InTestInputAction);
	bool HasTestStaminaRegenEffectApplied() const { return bStaminaRegenEffectApplied; }
	bool IsTestExhaustionActive() const { return bExhaustionActive; }
	bool HasTestExhaustionRecoveryTimer() const { return ExhaustionRecoveryTimerHandle.IsValid(); }
	bool HasTestExhaustionMoveSpeedEffect() const { return ExhaustionMoveSpeedEffectHandle.IsValid(); }
	int32 GetTestWorldPickupCandidateCount() const { return WorldPickupCandidates.Num(); }
	bool HasTestFormerOwnerInteractionTimer() const { return FormerOwnerInteractionRefreshTimerHandle.IsValid(); }
	void TriggerTestSeedWorldPickupCandidates() { SeedWorldPickupCandidates(); }
	void TriggerTestHandleInteractStarted();
	int32 GetTestHitFeedbackCameraShakeStartCount() const { return TestHitFeedbackCameraShakeStartCount; }
	UCameraShakeBase* GetTestLastHitFeedbackCameraShake() const { return TestLastHitFeedbackCameraShake.Get(); }
	UCameraShakeBase* GetTestActiveHitFeedbackCameraShake() const { return ActiveHitFeedbackCameraShake.Get(); }

	void SetTestLockedTarget(AEnemyCharacter* InTarget) { SetLockedTarget(InTarget); }
	void SetTestCurrentMoveInput(const FVector2D& InInput) { CurrentMoveInput = InInput; }
	bool TriggerTestAcquireLockOnTarget() { return TryAcquireLockOnTarget(); }
	bool TriggerTestValidateCurrentLockedTarget() { return ValidateCurrentLockedTarget() != ELockOnValidationResult::Cleared; }
	bool TriggerTestCanRetainExecutionLockedTarget(AEnemyCharacter* InTarget)
	{
		return CanRetainExecutionLockedTarget(InTarget, GetAbilitySystemComponent());
	}
	void TriggerTestTargetCycle(float InAxisValue);
	void SetTestLockOnProjectionHook(TFunction<bool(const FVector&, FVector2D&, FVector2D&)> InHook) { TestLockOnProjectionHook = MoveTemp(InHook); }
	void SetTestLockOnCursorPosition(const FVector2D& InPosition) { TestLockOnCursorPosition = InPosition; }
	void SetTestBypassLockOnValidation(const bool bBypass) { bTestBypassLockOnValidation = bBypass; }
	void SetTestBypassReceivedHitAudioPlayback(const bool bBypass) { bTestBypassReceivedHitAudioPlayback = bBypass; }
	int32 GetTestReceivedHitSoundDispatchCount() const { return TestReceivedHitSoundDispatchCount; }
	FVector GetTestLastReceivedHitSoundLocation() const { return TestLastReceivedHitSoundLocation; }
	bool HasTestMeleeMotionWarpTarget(FName WarpTargetName, FTransform* OutTransform = nullptr) const;
	int32 GetTestMeleeMotionWarpTargetCount() const;
	void TriggerTestUnPossessed() { UnPossessed(); }
	void TriggerTestEndPlay(const EEndPlayReason::Type Reason = EEndPlayReason::Destroyed) { EndPlay(Reason); }
	void TriggerTestOnMovementModeChanged(EMovementMode PrevMode, uint8 CustomMode = 0) { OnMovementModeChanged(PrevMode, CustomMode); }
#endif

private:
	enum class ELockOnValidationResult : uint8
	{
		Valid,
		RetargetedAfterDeath,
		Cleared
	};

	void HandlePrimaryAttackStarted(const FInputActionValue& Value);
	void HandlePrimaryAttackCompleted(const FInputActionValue& Value);
	void HandlePrimaryAttackCanceled(const FInputActionValue& Value);
	void HandleAimActionStarted(const FInputActionValue& Value);
	void HandleAimActionCompleted(const FInputActionValue& Value);
	void HandleAimActionCanceled(const FInputActionValue& Value);
	void HandleGuardActionStarted(const FInputActionValue& Value);
	void HandleGuardActionCompleted(const FInputActionValue& Value);
	void HandleGuardActionCanceled(const FInputActionValue& Value);
	void HandleParryActionStarted(const FInputActionValue& Value);
	void HandleParryActionCompleted(const FInputActionValue& Value);
	void HandleParryActionCanceled(const FInputActionValue& Value);
	void HandleAbilitySlotStarted(const FInputActionValue& Value, int32 SlotIndex);
	void HandleAbilitySlotCompleted(const FInputActionValue& Value, int32 SlotIndex);
	void HandleAbilitySlotCanceled(const FInputActionValue& Value, int32 SlotIndex);
	void HandleDodgeSprintStarted(const FInputActionValue& Value);
	void HandleDodgeSprintCompleted(const FInputActionValue& Value);
	void HandleDodgeSprintCanceled(const FInputActionValue& Value);
	void HandleDodgeSprintThresholdElapsed();
	void HandleLockOnStarted(const FInputActionValue& Value);
	void HandleTargetCycleTriggered(const FInputActionValue& Value);
	void RequestDodgeAbility();
	void ResumeGuardAfterAttack();
	void ClearDodgeSprintInputState();
	void HandleCombatInputStarted(const FGameplayTag& InputIntentTag);
	void HandleCombatInputEnded(const FGameplayTag& InputIntentTag, bool bWasCanceled);
	void SendCombatInputEvent(const FGameplayTag& EventTag, const FGameplayTag& InputIntentTag, float HeldDuration);
	void RequestAbilityForInputIntent(const FGameplayTag& InputIntentTag);
	FGameplayTag GetAbilitySlotInputIntentTag(int32 SlotIndex) const;
	UPlayerGuardAbility* FindActiveGuardAbility() const;
	UPlayerParryAbility* FindActiveParryAbility() const;
	void TryStartSprint();
	void BindSprintStateEvents();
	void UnbindSprintStateEvents();
	void OnSprintRelevantTagChanged(const FGameplayTag Tag, int32 NewCount);
	void GetCameraPlanarAxes(FVector& OutForwardDirection, FVector& OutRightDirection) const;
	void UpdateActionFacingRotationMode();
	bool CanApplyLockedLocomotionFacing() const;
	void UpdateLockedLocomotionFacing(float DeltaSeconds);
	bool TryAcquireLockOnTarget();
	bool BuildLockOnCandidates(TArray<FPlayerLockOnCandidate>& OutCandidates, FVector2D& OutPlayerScreenPosition) const;
	bool TryProjectLockOnWorldPoint(const APlayerController* PlayerController, const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize, float MarginRatio = 0.0f) const;
	ELockOnValidationResult ValidateCurrentLockedTarget();
	bool CanRetainExecutionLockedTarget(const AEnemyCharacter* CurrentTarget, const UAbilitySystemComponent* SourceASC) const;
	bool CacheCurrentLockedTargetCandidate();
	bool TryRetargetAfterLockedTargetDeath(AEnemyCharacter* DeadTarget);
	bool TryGetLockedTargetDirection(FVector& OutDirection);
	bool TryGetLockedTargetDirectionUnchecked(FVector& OutDirection) const;
	void SetLockedTarget(AEnemyCharacter* NewTarget, const FPlayerLockOnCandidate* Candidate = nullptr);
	void ClearLockedTarget();

	bool IsMovementInputBlocked() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWeaponEquipmentComponent> WeaponEquipment;

	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	TMap<FGameplayTag, float> HeldCombatInputStartTimes;
	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag AimInputTag;
	FGameplayTag GuardInputTag;
	FGameplayTag ParryInputTag;
	TArray<FGameplayTag> AbilitySlotInputTags;
	FGameplayTag InputPressedEventTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;
	FGameplayTag MovementInputBlockedTag;
	FGameplayTag SprintAbilityTag;
	FGameplayTag SprintStateTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag DodgingStateTag;
	FGameplayTag GuardingStateTag;
	FGameplayTag ParryingStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag SmallHitReactingStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag ExhaustedStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag GuardAbilityTag;
	FGameplayTag ParryAbilityTag;
	FGameplayTag SmallHitReactionEventTag;
	FGameplayTag BigHitReactionEventTag;
	FGameplayTag LaunchReactionEventTag;
	FActiveGameplayEffectHandle SprintJumpAirSpeedEffectHandle;
	FActiveGameplayEffectHandle ExhaustionMoveSpeedEffectHandle;
	FDelegateHandle MovementInputBlockedTagChangedHandle;
	FDelegateHandle AttackingStateTagChangedHandle;
	FDelegateHandle DodgingStateTagChangedHandle;
	FDelegateHandle GuardingStateTagChangedHandle;
	FDelegateHandle ParryingStateTagChangedHandle;
	FDelegateHandle DeadStateTagChangedHandle;
	FDelegateHandle StunnedStateTagChangedHandle;
	FDelegateHandle HealthAttributeChangedHandle;
	FDelegateHandle StaminaAttributeChangedHandle;
	FDelegateHandle ExhaustionDeadStateTagChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SprintStateBoundAbilitySystemComponent;
	TWeakObjectPtr<UAbilitySystemComponent> HealthBoundAbilitySystemComponent;
	TWeakObjectPtr<UAbilitySystemComponent> ExhaustionBoundAbilitySystemComponent;
	FTimerHandle DodgeSprintHoldTimerHandle;
	FTimerHandle GuardResumeTimerHandle;
	FTimerHandle ExhaustionRecoveryTimerHandle;
	float DodgeSprintInputPressedTime = 0.0f;
	bool bDodgeSprintInputHeld = false;
	bool bDodgeSprintResolvedToSprint = false;
	/** Resolved long-press intent consumed by the existing Sprint ability lifecycle. */
	bool bSprintInputHeld = false;
	bool bSprintRequiresReleaseAfterExhaustion = false;
	bool bGuardResumeEligibleAfterAttack = false;
	bool bGuardRequiresReleaseAfterBreak = false;
	bool bStaminaRegenEffectApplied = false;
	bool bExhaustionActive = false;
	bool bExhaustionMinimumDurationElapsed = false;
	bool bHasLoggedMissingCombatFeedbackData = false;
	TWeakObjectPtr<APlayerCameraManager> ActiveHitFeedbackCameraManager;
	TWeakObjectPtr<UCameraShakeBase> ActiveHitFeedbackCameraShake;
	TSubclassOf<UCameraShakeBase> ActiveHitFeedbackCameraShakeClass;

#if WITH_DEV_AUTOMATION_TESTS
	int32 TestHitFeedbackCameraShakeStartCount = 0;
	TWeakObjectPtr<UCameraShakeBase> TestLastHitFeedbackCameraShake;
	int32 TestReceivedHitSoundDispatchCount = 0;
	FVector TestLastReceivedHitSoundLocation = FVector::ZeroVector;
	bool bTestBypassReceivedHitAudioPlayback = false;
#endif

	void BindHealthEvents();
	void UnbindHealthEvents();
	void OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void TriggerHitFeedbackCameraShake(EHitReactionTier ReactionTier);
	void TriggerReceivedHitSound(const FGameplayEffectSpec& EffectSpec);
	TSubclassOf<UCameraShakeBase> ResolveHitFeedbackCameraShakeClass(EHitReactionTier ReactionTier);
	TSubclassOf<UCameraShakeBase> ResolveAttackerImpactCameraShakeClass(EHitReactionTier ReactionTier);
	TSubclassOf<UCameraShakeBase> ResolveExecutionImpactCameraShakeClass();
	void StartHitFeedbackCameraShakeInstance(TSubclassOf<UCameraShakeBase> ResolvedClass);
	void ClearActiveHitFeedbackCameraShake();
	void BindExhaustionStateEvents();
	void UnbindExhaustionStateEvents();
	void OnStaminaAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void OnExhaustionDeadStateTagChanged(const FGameplayTag Tag, int32 NewCount);
	void BeginExhaustion();
	void OnExhaustionMinimumDurationElapsed();
	void TryClearExhaustionAfterRecovery();
	void ClearExhaustionState();
	void ApplyExhaustionMoveSpeedEffect();

	bool TryCalculateMousePlaneIntersection(FVector& OutIntersectionPoint) const;
	void UpdateBowAimFacing();
	void ApplyBowAimFacing(const FVector& AimDirection);
	void SeedWorldPickupCandidates();

	TWeakObjectPtr<const UObject> ActiveBowAimRequester;
	FVector LastValidBowAimDirection = FVector::ZeroVector;
	bool bHasValidBowAimDirection = false;
	TWeakObjectPtr<AEnemyCharacter> LockedTarget;
	TOptional<FPlayerLockOnCandidate> LastValidLockedTargetCandidate;
	TSet<TWeakObjectPtr<AWorldWeaponPickup>> WorldPickupCandidates;
	TWeakObjectPtr<AWorldWeaponPickup> CurrentWorldPickupCandidate;
	FTimerHandle FormerOwnerInteractionRefreshTimerHandle;

#if WITH_DEV_AUTOMATION_TESTS
public:
	TFunction<bool(const FVector&, FVector2D&, FVector2D&)> TestLockOnProjectionHook;
	TOptional<FVector2D> TestLockOnCursorPosition;
	bool bTestBypassLockOnValidation = false;
	void TestClearLockedTarget() { ClearLockedTarget(); }
	void TriggerTestRequestAbilityForInputIntent(const FGameplayTag& InputIntentTag) { RequestAbilityForInputIntent(InputIntentTag); }
	void TriggerTestHandleCombatInputStarted(const FGameplayTag& InputIntentTag) { HandleCombatInputStarted(InputIntentTag); }
	void TriggerTestHandleCombatInputEnded(const FGameplayTag& InputIntentTag, bool bWasCanceled = false) { HandleCombatInputEnded(InputIntentTag, bWasCanceled); }
	void TestUnPossessed() { UnPossessed(); }
#endif
};
