// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Character/BaseCharacter.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UCombatLoadoutDefinition;
class UGameplayEffect;
class UInputAction;
class UInputComponent;
class UPlayerGuardAbility;
class UPlayerParryAbility;
class AActor;
class UGameplayAbility;
class UMeleeWeaponDefinition;
class UWeaponEquipmentComponent;
class UAIPerceptionStimuliSourceComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Player-specific camera and input layer built on the shared GAS character.
 */
UCLASS(Blueprintable)
class POLYQUEST_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

	/** Fixed-world camera boom for the oblique Perspective composition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera attached to the spring arm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;

	/** Registers this player as the explicit Sight source for the first enemy fixture. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Perception", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAIPerceptionStimuliSourceComponent> SightStimuliSource;

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

	/** Shared physical input that routes to this loadout's primary combat ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* PrimaryAttackAction;

	/** Shared physical input that expresses aim intent and may route to a future Aim ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AimAction;

	/** Shared physical input that expresses held Guard intent through the active Combat Loadout. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* GuardAction;

	/** Shared physical input that expresses Parry intent through the active Combat Loadout. */
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

	/** Hold duration at which the shared Dodge/Sprint input resolves to Sprint intent. */
	UPROPERTY(EditDefaultsOnly, Category="Input", meta=(ClampMin="0.01", UIMin="0.01"))
	float DodgeSprintHoldThresholdSeconds = 0.15f;

	/** Continuous periodic GameplayEffect that recovers Stamina when its tag requirements allow it. */
	UPROPERTY(EditDefaultsOnly, Category="GAS|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenGameplayEffectClass;

	/** The authored combat routes applied to this player at BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Loadout", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatLoadoutDefinition> InitialCombatLoadout;

	/** Required default weapon equipped once at BeginPlay; missing is a fail-visible configuration error. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMeleeWeaponDefinition> DefaultEquippedWeapon;

public:
	APlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/** Sets the authored routes used by future combat input starts without interrupting active abilities. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool SetActiveCombatLoadout(UCombatLoadoutDefinition* NewCombatLoadout);

	/** Returns the loadout currently routing combat input; C++-only narrow read for the equipment snapshot. */
	UCombatLoadoutDefinition* GetActiveCombatLoadout() const { return ActiveCombatLoadout; }

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
	bool TryGuardIncomingMeleeHit(AActor* AttackingActor, float GuardStaminaDamage);

	/** Resolves one valid incoming melee contact through the active Parry first, then the active Guard. */
	bool TryResolveIncomingDefense(AActor* AttackingActor, float GuardStaminaDamage);

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

	/** Returns the player camera boom. */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns the player follow camera. */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
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

	bool IsMovementInputBlocked() const;

	UPROPERTY(Transient)
	TObjectPtr<UCombatLoadoutDefinition> ActiveCombatLoadout;

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
	FGameplayTag DeadStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag GuardAbilityTag;
	FGameplayTag ParryAbilityTag;
	FActiveGameplayEffectHandle SprintJumpAirSpeedEffectHandle;
	FDelegateHandle MovementInputBlockedTagChangedHandle;
	FDelegateHandle AttackingStateTagChangedHandle;
	FDelegateHandle DodgingStateTagChangedHandle;
	FDelegateHandle GuardingStateTagChangedHandle;
	FDelegateHandle ParryingStateTagChangedHandle;
	FDelegateHandle DeadStateTagChangedHandle;
	FDelegateHandle StunnedStateTagChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SprintStateBoundAbilitySystemComponent;
	FTimerHandle DodgeSprintHoldTimerHandle;
	FTimerHandle GuardResumeTimerHandle;
	float DodgeSprintInputPressedTime = 0.0f;
	bool bDodgeSprintInputHeld = false;
	bool bDodgeSprintResolvedToSprint = false;
	/** Resolved long-press intent consumed by the existing Sprint ability lifecycle. */
	bool bSprintInputHeld = false;
	bool bSprintRequiresReleaseAfterExhaustion = false;
	bool bGuardResumeEligibleAfterAttack = false;
	bool bGuardRequiresReleaseAfterBreak = false;
	bool bStaminaRegenEffectApplied = false;
};
