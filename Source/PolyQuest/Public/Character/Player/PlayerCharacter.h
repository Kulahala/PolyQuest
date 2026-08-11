// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
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

	/** Input action used to request the light attack ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LightAttackAction;

	/** Input action used to request the Dodge ability. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DodgeAction;

	/** Continuous periodic GameplayEffect that recovers Stamina when its tag requirements allow it. */
	UPROPERTY(EditDefaultsOnly, Category="GAS|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenGameplayEffectClass;

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

	/** Request the light attack ability through the character ASC. */
	void LightAttack(const FInputActionValue& Value);

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

	/** Returns the current camera-relative Dodge direction, defaulting to camera forward. */
	FVector GetDodgeWorldDirection() const;

	/** Returns the player camera boom. */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns the player follow camera. */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	bool IsDodging() const;

	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	bool bStaminaRegenEffectApplied = false;
};
