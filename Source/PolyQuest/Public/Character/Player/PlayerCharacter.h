// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Character/BaseCharacter.h"
#include "GameplayTagContainer.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UCombatLoadoutDefinition;
class UGameplayEffect;
class UInputAction;
class UInputComponent;
class UAIPerceptionStimuliSourceComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Player-specific camera and input layer built on the shared GAS character.
 */
UCLASS(Abstract)
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

	/** Direct ability-slot inputs; index zero is Input.AbilitySlot.1. */
	UPROPERTY(EditAnywhere, Category="Input", meta=(EditFixedSize))
	TArray<UInputAction*> AbilitySlotActions;

	/** Input action used to request the Dodge ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DodgeAction;

	/** Physical hold input that requests ground Sprint while directional movement remains active. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SprintAction;

	/** Continuous periodic GameplayEffect that recovers Stamina when its tag requirements allow it. */
	UPROPERTY(EditDefaultsOnly, Category="GAS|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenGameplayEffectClass;

	/** The authored combat routes applied to this player at BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Loadout", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCombatLoadoutDefinition> InitialCombatLoadout;

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

	/** Request the Dodge ability through the character ASC. */
	void Dodge(const FInputActionValue& Value);

	/** Clears cached directional input after movement input ends. */
	void ClearMoveInput(const FInputActionValue& Value);

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
	UFUNCTION(BlueprintCallable, Category="Combat|Loadout")
	bool SetActiveCombatLoadout(UCombatLoadoutDefinition* NewCombatLoadout);

	/** Returns whether this physical combat input is currently held. */
	UFUNCTION(BlueprintPure, Category="Combat|Input")
	bool IsCombatInputHeld(FGameplayTag InputIntentTag) const;

	/** Returns the elapsed hold duration for an active physical combat input. */
	UFUNCTION(BlueprintPure, Category="Combat|Input")
	float GetCombatInputHeldDuration(FGameplayTag InputIntentTag) const;

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
	void HandleAbilitySlotStarted(const FInputActionValue& Value, int32 SlotIndex);
	void HandleAbilitySlotCompleted(const FInputActionValue& Value, int32 SlotIndex);
	void HandleAbilitySlotCanceled(const FInputActionValue& Value, int32 SlotIndex);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);
	void HandleSprintCanceled(const FInputActionValue& Value);
	void HandleCombatInputStarted(const FGameplayTag& InputIntentTag);
	void HandleCombatInputEnded(const FGameplayTag& InputIntentTag, bool bWasCanceled);
	void SendCombatInputEvent(const FGameplayTag& EventTag, const FGameplayTag& InputIntentTag, float HeldDuration);
	void RequestAbilityForInputIntent(const FGameplayTag& InputIntentTag);
	FGameplayTag GetAbilitySlotInputIntentTag(int32 SlotIndex) const;
	void TryStartSprint();
	void BindSprintStateEvents();
	void UnbindSprintStateEvents();
	void OnSprintRelevantTagChanged(const FGameplayTag Tag, int32 NewCount);
	void GetCameraPlanarAxes(FVector& OutForwardDirection, FVector& OutRightDirection) const;
	void UpdateActionFacingRotationMode();

	bool IsMovementInputBlocked() const;

	UPROPERTY(Transient)
	TObjectPtr<UCombatLoadoutDefinition> ActiveCombatLoadout;

	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	TMap<FGameplayTag, float> HeldCombatInputStartTimes;
	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag AimInputTag;
	TArray<FGameplayTag> AbilitySlotInputTags;
	FGameplayTag InputPressedEventTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;
	FGameplayTag MovementInputBlockedTag;
	FGameplayTag SprintAbilityTag;
	FGameplayTag SprintStateTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag DodgingStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag StunnedStateTag;
	FActiveGameplayEffectHandle SprintJumpAirSpeedEffectHandle;
	FDelegateHandle MovementInputBlockedTagChangedHandle;
	FDelegateHandle AttackingStateTagChangedHandle;
	FDelegateHandle DodgingStateTagChangedHandle;
	FDelegateHandle DeadStateTagChangedHandle;
	FDelegateHandle StunnedStateTagChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SprintStateBoundAbilitySystemComponent;
	bool bSprintInputHeld = false;
	bool bSprintRequiresReleaseAfterExhaustion = false;
	bool bStaminaRegenEffectApplied = false;
};
