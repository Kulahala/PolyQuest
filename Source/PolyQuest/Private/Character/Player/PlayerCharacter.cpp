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
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "PolyQuest.h"

APlayerCharacter::APlayerCharacter()
{
	AbilitySlotActions.SetNum(4);
	CombatTeamTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	AimInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Aim")), false);
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.1")), false));
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.2")), false));
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.3")), false));
	AbilitySlotInputTags.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.AbilitySlot.4")), false));
	InputPressedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Pressed")), false);
	InputReleasedEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	InputCanceledEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	MovementInputBlockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	SprintAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Sprint")), false);
	SprintStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

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
	CameraBoom->TargetArmLength = 2000.0f;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-55.0f, -45.0f, 0.0f));
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 18.0f;
	CameraBoom->CameraLagMaxDistance = 75.0f;
	CameraBoom->bUseCameraLagSubstepping = true;
	CameraBoom->CameraLagMaxTimeStep = 1.0f / 60.0f;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 60.0f;

	SightStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("SightStimuliSource"));
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetActiveCombatLoadout(InitialCombatLoadout);
	BindSprintStateEvents();
	if (SightStimuliSource)
	{
		SightStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	}

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

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SightStimuliSource)
	{
		SightStimuliSource->UnregisterFromPerceptionSystem();
	}

	ClearDodgeSprintInputState();
	CancelSprintAbility();
	UnbindSprintStateEvents();
	ClearSprintJumpAirSpeed();

	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (!GetCharacterMovement()->IsMovingOnGround())
	{
		CancelSprintAbility();
		return;
	}

	ClearSprintJumpAirSpeed();
	TryStartSprint();
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

		if (DodgeSprintAction)
		{
			EnhancedInputComponent->BindAction(DodgeSprintAction, ETriggerEvent::Started, this, &APlayerCharacter::HandleDodgeSprintStarted);
			EnhancedInputComponent->BindAction(DodgeSprintAction, ETriggerEvent::Completed, this, &APlayerCharacter::HandleDodgeSprintCompleted);
			EnhancedInputComponent->BindAction(DodgeSprintAction, ETriggerEvent::Canceled, this, &APlayerCharacter::HandleDodgeSprintCanceled);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no DodgeSprintAction configured."), *GetNameSafe(this));
		}
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
	if (MovementVector.IsNearlyZero())
	{
		CancelSprintAbility();
	}
	else
	{
		TryStartSprint();
	}
}

void APlayerCharacter::ClearMoveInput(const FInputActionValue&)
{
	CurrentMoveInput = FVector2D::ZeroVector;
	CancelSprintAbility();
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void APlayerCharacter::DoMove(float Right, float Forward)
{
	if (IsMovementInputBlocked())
	{
		return;
	}

	FVector ForwardDirection;
	FVector RightDirection;
	GetCameraPlanarAxes(ForwardDirection, RightDirection);
	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void APlayerCharacter::DoLook(float, float)
{
}

void APlayerCharacter::DoJumpStart()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const FGameplayTag JumpAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Jump")), false);
	if (!CharacterASC || !JumpAbilityTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot request Jump without an ASC and valid Ability.Movement.Jump tag."), *GetNameSafe(this));
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(JumpAbilityTag);
	CharacterASC->TryActivateAbilitiesByTag(AbilityTags);
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

void APlayerCharacter::HandleDodgeSprintStarted(const FInputActionValue&)
{
	if (bDodgeSprintInputHeld)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || IsActorBeingDestroyed())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(DodgeSprintHoldTimerHandle);
	bDodgeSprintInputHeld = true;
	bDodgeSprintResolvedToSprint = false;
	bSprintInputHeld = false;
	DodgeSprintInputPressedTime = World->GetTimeSeconds();

	const float ThresholdSeconds = FMath::Max(DodgeSprintHoldThresholdSeconds, 0.01f);
	if (DodgeSprintHoldThresholdSeconds <= 0.0f)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has an invalid DodgeSprintHoldThresholdSeconds; clamping to %.2f seconds."), *GetNameSafe(this), ThresholdSeconds);
	}

	World->GetTimerManager().SetTimer(
		DodgeSprintHoldTimerHandle,
		this,
		&APlayerCharacter::HandleDodgeSprintThresholdElapsed,
		ThresholdSeconds,
		false);
}

void APlayerCharacter::HandleDodgeSprintCompleted(const FInputActionValue&)
{
	if (!bDodgeSprintInputHeld)
	{
		ClearDodgeSprintInputState();
		CancelSprintAbility();
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || IsActorBeingDestroyed())
	{
		ClearDodgeSprintInputState();
		CancelSprintAbility();
		return;
	}

	const float HeldDuration = FMath::Max(World->GetTimeSeconds() - DodgeSprintInputPressedTime, 0.0f);
	const float ThresholdSeconds = FMath::Max(DodgeSprintHoldThresholdSeconds, 0.01f);
	// Treat a same-frame threshold release as the long-press branch despite timer/clock float rounding.
	const bool bResolvedToSprint = bDodgeSprintResolvedToSprint || HeldDuration + KINDA_SMALL_NUMBER >= ThresholdSeconds;

	ClearDodgeSprintInputState();
	if (bResolvedToSprint)
	{
		CancelSprintAbility();
		return;
	}

	RequestDodgeAbility();
}

void APlayerCharacter::HandleDodgeSprintCanceled(const FInputActionValue&)
{
	ClearDodgeSprintInputState();
	CancelSprintAbility();
}

void APlayerCharacter::HandleDodgeSprintThresholdElapsed()
{
	if (!bDodgeSprintInputHeld || IsActorBeingDestroyed())
	{
		return;
	}

	bDodgeSprintResolvedToSprint = true;
	bSprintInputHeld = true;
	TryStartSprint();
}

void APlayerCharacter::ClearDodgeSprintInputState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DodgeSprintHoldTimerHandle);
	}

	DodgeSprintHoldTimerHandle.Invalidate();
	DodgeSprintInputPressedTime = 0.0f;
	bDodgeSprintInputHeld = false;
	bDodgeSprintResolvedToSprint = false;
	bSprintInputHeld = false;
	bSprintRequiresReleaseAfterExhaustion = false;
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

	if (InputIntentTag == PrimaryAttackInputTag && ShouldRequestSprintAttack())
	{
		FGameplayTag SprintAttackAbilityTag;
		if (ActiveCombatLoadout->TryGetSprintAttackAbilityTag(SprintAttackAbilityTag) && SprintAttackAbilityTag.IsValid())
		{
			UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
			if (CharacterASC)
			{
				FGameplayTagContainer SprintAttackAbilityTags;
				SprintAttackAbilityTags.AddTag(SprintAttackAbilityTag);
				if (CharacterASC->TryActivateAbilitiesByTag(SprintAttackAbilityTags))
				{
					UE_LOG(LogPolyQuest, Verbose, TEXT("CombatInput: owner='%s', intent='%s', ability='%s', activationRequested=true."), *GetNameSafe(this), *InputIntentTag.ToString(), *SprintAttackAbilityTag.ToString());
					return;
				}
			}
		}
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

void APlayerCharacter::RequestDodgeAbility()
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

FVector APlayerCharacter::GetActionWorldDirection() const
{
	FVector ForwardDirection;
	FVector RightDirection;
	GetCameraPlanarAxes(ForwardDirection, RightDirection);
	const FVector DesiredDirection = ForwardDirection * CurrentMoveInput.Y + RightDirection * CurrentMoveInput.X;
	if (!DesiredDirection.IsNearlyZero())
	{
		return DesiredDirection.GetSafeNormal2D();
	}

	const FVector ActorForwardDirection = GetActorForwardVector().GetSafeNormal2D();
	return ActorForwardDirection.IsNearlyZero() ? ForwardDirection : ActorForwardDirection;
}

void APlayerCharacter::ApplyActionFacing()
{
	const FVector ActionDirection = GetActionWorldDirection();
	if (!ActionDirection.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, ActionDirection.Rotation().Yaw, 0.0f));
	}
}

void APlayerCharacter::GetCameraPlanarAxes(FVector& OutForwardDirection, FVector& OutRightDirection) const
{
	const FRotator CameraRotation = CameraBoom ? CameraBoom->GetComponentRotation() : GetActorRotation();
	const FRotator CameraYawRotation(0.0f, CameraRotation.Yaw, 0.0f);
	OutForwardDirection = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X);
	OutRightDirection = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y);
}

void APlayerCharacter::UpdateActionFacingRotationMode()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!MovementComponent)
	{
		return;
	}

	const bool bIsAttacking = CharacterASC && AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag);
	const bool bIsDodging = CharacterASC && DodgingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingStateTag);
	MovementComponent->bOrientRotationToMovement = !bIsAttacking && !bIsDodging;
}

bool APlayerCharacter::IsMovementInputBlocked() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	return CharacterASC && MovementInputBlockedTag.IsValid() && CharacterASC->HasMatchingGameplayTag(MovementInputBlockedTag);
}

bool APlayerCharacter::CanAttemptSprint() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	return bSprintInputHeld && !bSprintRequiresReleaseAfterExhaustion && !CurrentMoveInput.IsNearlyZero()
		&& CharacterASC && CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) > 0.0f
		&& MovementComponent && MovementComponent->IsMovingOnGround()
		&& !IsMovementInputBlocked()
		&& !(AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag))
		&& !(DodgingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingStateTag))
		&& !(DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag))
		&& !(StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag));
}

bool APlayerCharacter::HasActiveSprint() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	return CharacterASC && SprintStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(SprintStateTag);
}

void APlayerCharacter::CancelSprintAbility()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || !SprintAbilityTag.IsValid() || !HasActiveSprint())
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(SprintAbilityTag);
	CharacterASC->CancelAbilities(&AbilityTags, nullptr);
}

bool APlayerCharacter::ApplySprintJumpAirSpeed(TSubclassOf<UGameplayEffect> SprintJumpAirSpeedGameplayEffectClass)
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const UGameplayEffect* SprintJumpAirSpeedEffect = SprintJumpAirSpeedGameplayEffectClass
		? SprintJumpAirSpeedGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !SprintJumpAirSpeedEffect)
	{
		return false;
	}

	ClearSprintJumpAirSpeed();
	SprintJumpAirSpeedEffectHandle = CharacterASC->ApplyGameplayEffectToSelf(
		SprintJumpAirSpeedEffect,
		1.0f,
		CharacterASC->MakeEffectContext());
	return SprintJumpAirSpeedEffectHandle.IsValid();
}

void APlayerCharacter::MarkSprintRequiresReleaseAfterExhaustion()
{
	bSprintRequiresReleaseAfterExhaustion = true;
}

void APlayerCharacter::TryStartSprint()
{
	if (!CanAttemptSprint() || HasActiveSprint() || !SprintAbilityTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent())
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(SprintAbilityTag);
		CharacterASC->TryActivateAbilitiesByTag(AbilityTags);
	}
}

void APlayerCharacter::BindSprintStateEvents()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || SprintStateBoundAbilitySystemComponent.Get() == CharacterASC)
	{
		return;
	}

	UnbindSprintStateEvents();
	SprintStateBoundAbilitySystemComponent = CharacterASC;
	if (MovementInputBlockedTag.IsValid())
	{
		MovementInputBlockedTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(MovementInputBlockedTag)
			.AddUObject(this, &APlayerCharacter::OnSprintRelevantTagChanged);
	}
	if (AttackingStateTag.IsValid())
	{
		AttackingStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(AttackingStateTag)
			.AddUObject(this, &APlayerCharacter::OnSprintRelevantTagChanged);
	}
	if (DodgingStateTag.IsValid())
	{
		DodgingStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(DodgingStateTag)
			.AddUObject(this, &APlayerCharacter::OnSprintRelevantTagChanged);
	}
	if (DeadStateTag.IsValid())
	{
		DeadStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(DeadStateTag)
			.AddUObject(this, &APlayerCharacter::OnSprintRelevantTagChanged);
	}
	if (StunnedStateTag.IsValid())
	{
		StunnedStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(StunnedStateTag)
			.AddUObject(this, &APlayerCharacter::OnSprintRelevantTagChanged);
	}

	UpdateActionFacingRotationMode();
}

void APlayerCharacter::UnbindSprintStateEvents()
{
	UAbilitySystemComponent* CharacterASC = SprintStateBoundAbilitySystemComponent.Get();
	if (CharacterASC)
	{
		if (MovementInputBlockedTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(MovementInputBlockedTagChangedHandle, MovementInputBlockedTag);
		}
		if (AttackingStateTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(AttackingStateTagChangedHandle, AttackingStateTag);
		}
		if (DodgingStateTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(DodgingStateTagChangedHandle, DodgingStateTag);
		}
		if (DeadStateTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(DeadStateTagChangedHandle, DeadStateTag);
		}
		if (StunnedStateTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(StunnedStateTagChangedHandle, StunnedStateTag);
		}
	}

	MovementInputBlockedTagChangedHandle.Reset();
	AttackingStateTagChangedHandle.Reset();
	DodgingStateTagChangedHandle.Reset();
	DeadStateTagChangedHandle.Reset();
	StunnedStateTagChangedHandle.Reset();
	SprintStateBoundAbilitySystemComponent.Reset();
}

void APlayerCharacter::OnSprintRelevantTagChanged(const FGameplayTag, int32 NewCount)
{
	UpdateActionFacingRotationMode();

	if (NewCount > 0)
	{
		CancelSprintAbility();
		return;
	}

	TryStartSprint();
}

void APlayerCharacter::ClearSprintJumpAirSpeed()
{
	if (!SprintJumpAirSpeedEffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent())
	{
		CharacterASC->RemoveActiveGameplayEffect(SprintJumpAirSpeedEffectHandle);
	}

	SprintJumpAirSpeedEffectHandle.Invalidate();
}

bool APlayerCharacter::ShouldRequestSprintAttack() const
{
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	return HasActiveSprint() && !CurrentMoveInput.IsNearlyZero() && MovementComponent && MovementComponent->IsMovingOnGround();
}
