// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Player/PlayerCharacter.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"

#include "Combat/Input/CombatLoadoutDefinition.h"
#include "PolyQuest.h"

APlayerCharacter::APlayerCharacter()
{
	AbilitySlotActions.SetNum(4);
	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	AimInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Aim")), false);
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.1")), false));
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.2")), false));
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.3")), false));
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.4")), false));
	InputPressedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Pressed")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);

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
	SetActiveCombatLoadout(InitialCombatLoadout);

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
		if (PrimaryAttackAction)
		{
			EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::HandlePrimaryAttackStarted);
			EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Completed, this, &APlayerCharacter::HandlePrimaryAttackCompleted);
			EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Canceled, this, &APlayerCharacter::HandlePrimaryAttackCanceled);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no PrimaryAttackAction configured."), *GetNameSafe(this));
		}

		if (AimAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::HandleAimActionStarted);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::HandleAimActionCompleted);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &APlayerCharacter::HandleAimActionCanceled);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no AimAction configured."), *GetNameSafe(this));
		}

		for (int32 SlotIndex = 0; SlotIndex < AbilitySlotActions.Num(); ++SlotIndex)
		{
			if (!AbilitySlotActions[SlotIndex])
			{
				continue;
			}

			EnhancedInputComponent->BindAction(AbilitySlotActions[SlotIndex], ETriggerEvent::Started, this, &APlayerCharacter::HandleAbilitySlotStarted, SlotIndex);
			EnhancedInputComponent->BindAction(AbilitySlotActions[SlotIndex], ETriggerEvent::Completed, this, &APlayerCharacter::HandleAbilitySlotCompleted, SlotIndex);
			EnhancedInputComponent->BindAction(AbilitySlotActions[SlotIndex], ETriggerEvent::Canceled, this, &APlayerCharacter::HandleAbilitySlotCanceled, SlotIndex);
		}

		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &APlayerCharacter::Dodge);
	}
	else
	{
		UE_LOG(LogPolyQuest, Error, TEXT("'%s' Failed to find an Enhanced Input component! The player character requires Enhanced Input."), *GetNameSafe(this));
	}
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

bool APlayerCharacter::SetActiveCombatLoadout(UCombatLoadoutDefinition* NewCombatLoadout)
{
	if (!NewCombatLoadout)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' rejected a null CombatLoadout."), *GetNameSafe(this));
		return false;
	}

	if (!NewCombatLoadout->IsRouteTableValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' rejected CombatLoadout '%s' because its input routes contain an invalid or duplicate input intent."), *GetNameSafe(this), *GetNameSafe(NewCombatLoadout));
		return false;
	}

	ActiveCombatLoadout = NewCombatLoadout;
	return true;
}

bool APlayerCharacter::IsCombatInputHeld(FGameplayTag InputIntentTag) const
{
	return InputIntentTag.IsValid() && HeldCombatInputStartTimes.Contains(InputIntentTag);
}

float APlayerCharacter::GetCombatInputHeldDuration(FGameplayTag InputIntentTag) const
{
	const float* StartTime = HeldCombatInputStartTimes.Find(InputIntentTag);
	const UWorld* World = GetWorld();
	return StartTime && World ? FMath::Max(World->GetTimeSeconds() - *StartTime, 0.0f) : 0.0f;
}

void APlayerCharacter::HandlePrimaryAttackStarted(const FInputActionValue&)
{
	HandleCombatInputStarted(PrimaryAttackInputTag);
}

void APlayerCharacter::HandlePrimaryAttackCompleted(const FInputActionValue&)
{
	HandleCombatInputEnded(PrimaryAttackInputTag, false);
}

void APlayerCharacter::HandlePrimaryAttackCanceled(const FInputActionValue&)
{
	HandleCombatInputEnded(PrimaryAttackInputTag, true);
}

void APlayerCharacter::HandleAimActionStarted(const FInputActionValue&)
{
	HandleCombatInputStarted(AimInputTag);
}

void APlayerCharacter::HandleAimActionCompleted(const FInputActionValue&)
{
	HandleCombatInputEnded(AimInputTag, false);
}

void APlayerCharacter::HandleAimActionCanceled(const FInputActionValue&)
{
	HandleCombatInputEnded(AimInputTag, true);
}

void APlayerCharacter::HandleAbilitySlotStarted(const FInputActionValue&, int32 SlotIndex)
{
	HandleCombatInputStarted(GetAbilitySlotInputIntentTag(SlotIndex));
}

void APlayerCharacter::HandleAbilitySlotCompleted(const FInputActionValue&, int32 SlotIndex)
{
	HandleCombatInputEnded(GetAbilitySlotInputIntentTag(SlotIndex), false);
}

void APlayerCharacter::HandleAbilitySlotCanceled(const FInputActionValue&, int32 SlotIndex)
{
	HandleCombatInputEnded(GetAbilitySlotInputIntentTag(SlotIndex), true);
}

void APlayerCharacter::HandleCombatInputStarted(const FGameplayTag& InputIntentTag)
{
	if (!InputIntentTag.IsValid() || HeldCombatInputStartTimes.Contains(InputIntentTag))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	HeldCombatInputStartTimes.Add(InputIntentTag, World->GetTimeSeconds());
	SendCombatInputEvent(InputPressedEventTag, InputIntentTag, 0.0f);
	RequestAbilityForInputIntent(InputIntentTag);
}

void APlayerCharacter::HandleCombatInputEnded(const FGameplayTag& InputIntentTag, bool bWasCanceled)
{
	const float* StartTime = HeldCombatInputStartTimes.Find(InputIntentTag);
	if (!StartTime)
	{
		return;
	}

	const float HeldDuration = GetCombatInputHeldDuration(InputIntentTag);
	HeldCombatInputStartTimes.Remove(InputIntentTag);
	SendCombatInputEvent(bWasCanceled ? InputCanceledEventTag : InputReleasedEventTag, InputIntentTag, HeldDuration);
}

void APlayerCharacter::SendCombatInputEvent(const FGameplayTag& EventTag, const FGameplayTag& InputIntentTag, float HeldDuration)
{
	if (!EventTag.IsValid() || !InputIntentTag.IsValid())
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = this;
	EventData.Target = this;
	EventData.InstigatorTags.AddTag(InputIntentTag);
	EventData.EventMagnitude = HeldDuration;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventTag, EventData);

	UE_LOG(LogPolyQuest, Verbose, TEXT("CombatInput: owner='%s', event='%s', intent='%s', held=%.3f."), *GetNameSafe(this), *EventTag.ToString(), *InputIntentTag.ToString(), HeldDuration);
}

void APlayerCharacter::RequestAbilityForInputIntent(const FGameplayTag& InputIntentTag)
{
	if (!ActiveCombatLoadout)
	{
		return;
	}

	FGameplayTag AbilityTag;
	if (!ActiveCombatLoadout->TryGetAbilityTagForInputIntent(InputIntentTag, AbilityTag) || !AbilityTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot route '%s' without an Ability System Component."), *GetNameSafe(this), *InputIntentTag.ToString());
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AbilityTag);
	const bool bActivated = CharacterASC->TryActivateAbilitiesByTag(AbilityTags);
	UE_LOG(LogPolyQuest, Verbose, TEXT("CombatInput: owner='%s', intent='%s', ability='%s', activationRequested=%s."), *GetNameSafe(this), *InputIntentTag.ToString(), *AbilityTag.ToString(), bActivated ? TEXT("true") : TEXT("false"));
}

FGameplayTag APlayerCharacter::GetAbilitySlotInputIntentTag(int32 SlotIndex) const
{
	return AbilitySlotInputTags.IsValidIndex(SlotIndex) ? AbilitySlotInputTags[SlotIndex] : FGameplayTag();
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
