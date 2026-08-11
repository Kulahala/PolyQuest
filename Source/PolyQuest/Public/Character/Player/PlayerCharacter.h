// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "GameplayTagContainer.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UCombatLoadoutDefinition;
class UGameplayEffect;
class UInputAction;
class UInputComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Player-specific camera and input layer built on the shared GAS character.
 */
UCLASS(Abstract)
class POLYQUEST_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera attached to the spring arm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;

protected:
	/** Input action used to start and stop jumping. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Input action used for movement. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Input action used for gamepad look. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Input action used for mouse look. */
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

	/** Initialize Enhanced Input bindings for the player pawn. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Convert the Enhanced Input movement action to gameplay movement. */
	void Move(const FInputActionValue& Value);

	/** Convert the Enhanced Input look action to controller input. */
	void Look(const FInputActionValue& Value);

	/** Request the Dodge ability through the character ASC. */
	void Dodge(const FInputActionValue& Value);

	/** Clears cached directional input after movement input ends. */
	void ClearMoveInput(const FInputActionValue& Value);

public:
	/** Handles movement input from controls or UI interfaces. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look input from controls or UI interfaces. */
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

	/** Returns the current camera-relative Dodge direction, defaulting to camera forward. */
	FVector GetDodgeWorldDirection() const;

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
	void HandleCombatInputStarted(const FGameplayTag& InputIntentTag);
	void HandleCombatInputEnded(const FGameplayTag& InputIntentTag, bool bWasCanceled);
	void SendCombatInputEvent(const FGameplayTag& EventTag, const FGameplayTag& InputIntentTag, float HeldDuration);
	void RequestAbilityForInputIntent(const FGameplayTag& InputIntentTag);
	FGameplayTag GetAbilitySlotInputIntentTag(int32 SlotIndex) const;

	bool IsDodging() const;

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
	bool bStaminaRegenEffectApplied = false;
};
