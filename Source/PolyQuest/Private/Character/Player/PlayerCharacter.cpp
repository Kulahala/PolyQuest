// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Player/PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "PolyQuest.h"

APlayerCharacter::APlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (bStaminaRegenEffectApplied || !HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const UGameplayEffect* StaminaRegenEffect = StaminaRegenGameplayEffectClass
		? StaminaRegenGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !StaminaRegenEffect)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot apply Stamina regeneration without an ASC and configured GameplayEffect."), *GetNameSafe(this));
		return;
	}

	const FActiveGameplayEffectHandle RegenEffectHandle = CharacterASC->ApplyGameplayEffectToSelf(
		StaminaRegenEffect,
		1.0f,
		CharacterASC->MakeEffectContext());
	bStaminaRegenEffectApplied = RegenEffectHandle.IsValid();

	if (!bStaminaRegenEffectApplied)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' failed to apply its configured Stamina regeneration GameplayEffect."), *GetNameSafe(this));
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoJumpEnd);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &APlayerCharacter::ClearMoveInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &APlayerCharacter::ClearMoveInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::LightAttack);
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &APlayerCharacter::Dodge);
	}
	else
	{
		UE_LOG(LogPolyQuest, Error, TEXT("'%s' Failed to find an Enhanced Input component! The player character requires Enhanced Input."), *GetNameSafe(this));
	}
}

void APlayerCharacter::LightAttack(const FInputActionValue&)
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot request a light attack without an Ability System Component."), *GetNameSafe(this));
		return;
	}

	const FGameplayTag LightAttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false);
	if (!LightAttackTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot request a light attack because the Ability.Attack.Light tag is invalid."), *GetNameSafe(this));
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(LightAttackTag);
	CharacterASC->TryActivateAbilitiesByTag(AbilityTags);
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	CurrentMoveInput = MovementVector;
	DoMove(MovementVector.X, MovementVector.Y);
}

void APlayerCharacter::ClearMoveInput(const FInputActionValue&)
{
	CurrentMoveInput = FVector2D::ZeroVector;
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void APlayerCharacter::DoMove(float Right, float Forward)
{
	if (IsDodging())
	{
		return;
	}

	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void APlayerCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void APlayerCharacter::DoJumpStart()
{
	if (!IsDodging())
	{
		Jump();
	}
}

void APlayerCharacter::DoJumpEnd()
{
	StopJumping();
}

void APlayerCharacter::Dodge(const FInputActionValue&)
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot request a Dodge without an Ability System Component."), *GetNameSafe(this));
		return;
	}

	const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Dodge")), false);
	if (!DodgeTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot request a Dodge because the Ability.Dodge tag is invalid."), *GetNameSafe(this));
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(DodgeTag);
	CharacterASC->TryActivateAbilitiesByTag(AbilityTags);
}

FVector APlayerCharacter::GetDodgeWorldDirection() const
{
	const FRotator ControlRotation = GetController() ? GetController()->GetControlRotation() : GetActorRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector DesiredDirection = ForwardDirection * CurrentMoveInput.Y + RightDirection * CurrentMoveInput.X;

	return DesiredDirection.IsNearlyZero() ? ForwardDirection : DesiredDirection.GetSafeNormal();
}

bool APlayerCharacter::IsDodging() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const FGameplayTag DodgingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	return CharacterASC && DodgingTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingTag);
}
