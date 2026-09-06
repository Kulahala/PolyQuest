// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Player/PlayerCharacter.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraModifier_FovPunch.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerLockOnTargeting.h"
#include "Combat/Projectile/CombatProjectileTargeting.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "UObject/ConstructorHelpers.h"

#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Equipment/WorldWeaponPickup.h"
#include "Combat/Feedback/CombatFeedbackDataAsset.h"
#include "Combat/Melee/CombatTeamAgent.h"
#include "Combat/Reaction/HitReactionClassifier.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameplayEffectExtension.h"
#include "PolyQuest.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float LockOnRetentionMarginRatio = 0.15f;
}

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AbilitySlotActions.SetNum(4);
	CombatTeamTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	AimInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Aim")), false);
	GuardInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Guard")), false);
	ParryInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Parry")), false);
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
	GuardingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
	ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	SmallHitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	ExhaustedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	GuardAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false);
	ParryAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Parry")), false);
	SmallHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Small")), false);
	BigHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Big")), false);
	LaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Launch")), false);

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 800.0f, 0.0f);
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
	CameraBoom->CameraLagSpeed = 8.0f;
	CameraBoom->CameraLagMaxDistance = 0.0f;
	CameraBoom->bUseCameraLagSubstepping = true;
	CameraBoom->CameraLagMaxTimeStep = 1.0f / 60.0f;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 60.0f;

	SightStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("SightStimuliSource"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
	MotionWarpingComponent->bSearchForWindowsInAnimsWithinMontages = false;
	WeaponEquipment = CreateDefaultSubobject<UWeaponEquipmentComponent>(TEXT("WeaponEquipment"));

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> PlayerGlobalsMPCObj(TEXT("/Game/_Materials/SeeThrough/MPC_PlayerGlobals.MPC_PlayerGlobals"));
	if (PlayerGlobalsMPCObj.Succeeded())
	{
		PlayerGlobalsMPC = PlayerGlobalsMPCObj.Object;
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerGlobalsMPC)
	{
		if (UWorld* World = GetWorld())
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, PlayerGlobalsMPC, FName(TEXT("CeilingRadius")), MaxCeilingRadius);
		}
	}

	BindSprintStateEvents();
	BindHealthEvents();
	BindExhaustionStateEvents();

	if (WeaponEquipment)
	{
		if (DefaultEquippedWeapon)
		{
			WeaponEquipment->EquipWeapon(DefaultEquippedWeapon);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no DefaultEquippedWeapon configured; the player starts without melee capability."), *GetNameSafe(this));
		}
	}

	if (SightStimuliSource)
	{
		SightStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	}

	OnCharacterMovementUpdated.AddUniqueDynamic(this, &APlayerCharacter::HandleCharacterMovementUpdated);
	SeedWorldPickupCandidates();

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

#if WITH_DEV_AUTOMATION_TESTS
void APlayerCharacter::ConfigureTestStartupFixture(
	UMeleeWeaponDefinition* InDefaultEquippedWeapon,
	TSubclassOf<UGameplayEffect> InStaminaRegenGameplayEffectClass,
	TSubclassOf<UGameplayEffect> InExhaustionMoveSpeedGameplayEffectClass,
	UInputAction* InTestInputAction)
{
	DefaultEquippedWeapon = InDefaultEquippedWeapon;
	StaminaRegenGameplayEffectClass = InStaminaRegenGameplayEffectClass;
	ExhaustionMoveSpeedGameplayEffectClass = InExhaustionMoveSpeedGameplayEffectClass;
	JumpAction = InTestInputAction;
	MoveAction = InTestInputAction;
	PrimaryAttackAction = InTestInputAction;
	AimAction = InTestInputAction;
	GuardAction = InTestInputAction;
	ParryAction = InTestInputAction;
	AbilitySlotActions.Init(InTestInputAction, 4);
	DodgeSprintAction = InTestInputAction;
	InteractAction = InTestInputAction;
	LockOnAction = InTestInputAction;
	TargetCycleAction = InTestInputAction;
}

void APlayerCharacter::TriggerTestHandleInteractStarted()
{
	HandleInteractStarted(FInputActionValue());
}
#endif

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindSprintStateEvents();
	BindHealthEvents();
	BindExhaustionStateEvents();
	OnCharacterMovementUpdated.AddUniqueDynamic(this, &APlayerCharacter::HandleCharacterMovementUpdated);
	SeedWorldPickupCandidates();
}

void APlayerCharacter::UnPossessed()
{
	ClearActiveHitFeedbackCameraShake();
	ClearMeleeMotionWarpTargets();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const FGameplayTag LightAttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false);
		if (LightAttackTag.IsValid())
		{
			FGameplayTagContainer LightAttackTags;
			LightAttackTags.AddTag(LightAttackTag);
			ASC->CancelAbilities(&LightAttackTags);
		}

		const FGameplayTag TeardownOnUnpossessTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
		if (TeardownOnUnpossessTag.IsValid())
		{
			FGameplayTagContainer TeardownTags;
			TeardownTags.AddTag(TeardownOnUnpossessTag);
			ASC->CancelAbilities(&TeardownTags);
		}
	}

	OnCharacterMovementUpdated.RemoveDynamic(this, &APlayerCharacter::HandleCharacterMovementUpdated);
	ClearWorldPickupInteractionState();
	Super::UnPossessed();
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SightStimuliSource)
	{
		SightStimuliSource->UnregisterFromPerceptionSystem();
	}

	ClearActiveHitFeedbackCameraShake();
	ClearMeleeMotionWarpTargets();
	ClearDodgeSprintInputState();
	ClearGuardResumeEligibility();
	bGuardRequiresReleaseAfterBreak = false;
	CancelSprintAbility();
	ClearExhaustionState();
	UnbindExhaustionStateEvents();
	UnbindHealthEvents();
	UnbindSprintStateEvents();
	ClearSprintJumpAirSpeed();
	ClearLockedTarget();

	OnCharacterMovementUpdated.RemoveDynamic(this, &APlayerCharacter::HandleCharacterMovementUpdated);
	ClearWorldPickupInteractionState();

	ActiveBowAimRequester = nullptr;
	bHasValidBowAimDirection = false;
	LastValidBowAimDirection = FVector::ZeroVector;

	if (PlayerGlobalsMPC)
	{
		if (UWorld* World = GetWorld())
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, PlayerGlobalsMPC, FName(TEXT("TunnelRadius")), 0.0f);
			UKismetMaterialLibrary::SetScalarParameterValue(World, PlayerGlobalsMPC, FName(TEXT("CeilingRadius")), 0.0f);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->bShowMouseCursor = true;
			PC->bEnableClickEvents = true;
			PC->bEnableMouseOverEvents = true;

			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
		}
	}
}

void APlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent || !MovementComponent->IsMovingOnGround())
	{
		ClearMeleeMotionWarpTargets();

		const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
		const bool bParryStateActive = CharacterASC && ParryingStateTag.IsValid()
			&& CharacterASC->HasMatchingGameplayTag(ParryingStateTag);

		// Parry owns its confirmed MOVE_None lock. An actual fall remains an
		// external interruption and must clear the held-Guard recovery path.
		if (MovementComponent && MovementComponent->IsFalling())
		{
			CancelActiveParry();
			CancelActiveGuardAfterConfirmedAction(false);
		}
		else if (!bParryStateActive)
		{
			CancelActiveGuardAfterConfirmedAction(false);
		}
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

		if (GuardAction)
		{
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Started, this, &APlayerCharacter::HandleGuardActionStarted);
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Completed, this, &APlayerCharacter::HandleGuardActionCompleted);
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Canceled, this, &APlayerCharacter::HandleGuardActionCanceled);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no GuardAction configured."), *GetNameSafe(this));
		}

		if (ParryAction)
		{
			EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Started, this, &APlayerCharacter::HandleParryActionStarted);
			EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Completed, this, &APlayerCharacter::HandleParryActionCompleted);
			EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Canceled, this, &APlayerCharacter::HandleParryActionCanceled);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no ParryAction configured."), *GetNameSafe(this));
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

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacter::HandleInteractStarted);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no InteractAction configured."), *GetNameSafe(this));
		}

		if (LockOnAction)
		{
			EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &APlayerCharacter::HandleLockOnStarted);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no LockOnAction configured."), *GetNameSafe(this));
		}

		if (TargetCycleAction)
		{
			EnhancedInputComponent->BindAction(TargetCycleAction, ETriggerEvent::Triggered, this, &APlayerCharacter::HandleTargetCycleTriggered);
		}
		else
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' has no TargetCycleAction configured."), *GetNameSafe(this));
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

bool APlayerCharacter::CanAttemptGuard() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	return GuardInputTag.IsValid() && IsCombatInputHeld(GuardInputTag) && !bGuardRequiresReleaseAfterBreak
		&& CharacterASC && MovementComponent && MovementComponent->IsMovingOnGround()
		&& !(GuardingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(GuardingStateTag))
		&& !(DodgingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingStateTag))
		&& !(DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag))
		&& !(StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag))
		&& CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) > 0.0f;
}

bool APlayerCharacter::TryGuardIncomingMeleeHit(AActor* AttackingActor, float GuardStaminaDamage, const FHitResult& HitResult)
{
	if (!AttackingActor)
	{
		return false;
	}

	if (UPlayerGuardAbility* GuardAbility = FindActiveGuardAbility())
	{
		return GuardAbility->TryGuardMeleeHit(AttackingActor, GuardStaminaDamage, HitResult);
	}

	return false;
}

UPlayerGuardAbility* APlayerCharacter::FindActiveGuardAbility() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || !GuardAbilityTag.IsValid() || !GuardingStateTag.IsValid()
		|| !CharacterASC->HasMatchingGameplayTag(GuardingStateTag))
	{
		return nullptr;
	}

	FGameplayTagContainer GuardAbilityTags;
	GuardAbilityTags.AddTag(GuardAbilityTag);
	TArray<FGameplayAbilitySpec*> GuardAbilitySpecs;
	CharacterASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(GuardAbilityTags, GuardAbilitySpecs, false);
	for (FGameplayAbilitySpec* GuardAbilitySpec : GuardAbilitySpecs)
	{
		UPlayerGuardAbility* GuardAbility = GuardAbilitySpec ? Cast<UPlayerGuardAbility>(GuardAbilitySpec->GetPrimaryInstance()) : nullptr;
		if (GuardAbility && GuardAbility->IsGuardActive())
		{
			return GuardAbility;
		}
	}

	return nullptr;
}

bool APlayerCharacter::TryResolveIncomingDefense(
	AActor* AttackingActor,
	float GuardStaminaDamage,
	const FHitResult& HitResult,
	bool bAllowParry)
{
	if (!AttackingActor)
	{
		return false;
	}

	if (bAllowParry)
	{
		if (UPlayerParryAbility* ParryAbility = FindActiveParryAbility())
		{
			return ParryAbility->TryParryMeleeHit(AttackingActor, HitResult);
		}
	}

	return TryGuardIncomingMeleeHit(AttackingActor, GuardStaminaDamage, HitResult);
}

void APlayerCharacter::TriggerParrySuccessCameraShake()
{
	TriggerHitFeedbackCameraShake(EHitReactionTier::Big);
	TriggerCameraFovPunch(1.8f);
}

void APlayerCharacter::TriggerAttackerImpactCameraShake(const EHitReactionTier ReactionTier)
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}

	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return;
	}

	const bool bIsDead = DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);
	if (bIsDead)
	{
		return;
	}

	const TSubclassOf<UCameraShakeBase> ResolvedClass = ResolveAttackerImpactCameraShakeClass(ReactionTier);
	StartHitFeedbackCameraShakeInstance(ResolvedClass);

	const float PunchDegrees = (ReactionTier == EHitReactionTier::Big || ReactionTier == EHitReactionTier::Launch) ? 1.5f : 0.0f;
	TriggerCameraFovPunch(PunchDegrees);
}

void APlayerCharacter::TriggerExecutionImpactCameraShake()
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}

	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return;
	}

	const bool bIsDead = DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);
	if (bIsDead)
	{
		return;
	}

	const TSubclassOf<UCameraShakeBase> ResolvedClass = ResolveExecutionImpactCameraShakeClass();
	StartHitFeedbackCameraShakeInstance(ResolvedClass);

	TriggerCameraFovPunch(2.0f);
}

void APlayerCharacter::NotifyStaminaActionRejected()
{
	if (APolyQuestPlayerController* PC = Cast<APolyQuestPlayerController>(GetController()))
	{
		PC->NotifyStaminaActionRejected();
	}
}

UPlayerParryAbility* APlayerCharacter::FindActiveParryAbility() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || !ParryAbilityTag.IsValid() || !ParryingStateTag.IsValid()
		|| !CharacterASC->HasMatchingGameplayTag(ParryingStateTag))
	{
		return nullptr;
	}

	FGameplayTagContainer ParryAbilityTags;
	ParryAbilityTags.AddTag(ParryAbilityTag);
	TArray<FGameplayAbilitySpec*> ParryAbilitySpecs;
	CharacterASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(ParryAbilityTags, ParryAbilitySpecs, false);
	for (FGameplayAbilitySpec* ParryAbilitySpec : ParryAbilitySpecs)
	{
		UPlayerParryAbility* ParryAbility = ParryAbilitySpec ? Cast<UPlayerParryAbility>(ParryAbilitySpec->GetPrimaryInstance()) : nullptr;
		if (ParryAbility && ParryAbility->IsParryActive())
		{
			return ParryAbility;
		}
	}

	return nullptr;
}

void APlayerCharacter::CancelActiveParry()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || !ParryAbilityTag.IsValid() || !ParryingStateTag.IsValid()
		|| !CharacterASC->HasMatchingGameplayTag(ParryingStateTag))
	{
		return;
	}

	FGameplayTagContainer ParryAbilityTags;
	ParryAbilityTags.AddTag(ParryAbilityTag);
	CharacterASC->CancelAbilities(&ParryAbilityTags, nullptr);
}

void APlayerCharacter::CancelActiveGuardAfterConfirmedAction(bool bResumeAfterAttack)
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const bool bWasGuarding = CharacterASC && GuardingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(GuardingStateTag);
	const bool bShouldResumeGuard = bResumeAfterAttack && FindActiveGuardAbility() && GuardInputTag.IsValid() && IsCombatInputHeld(GuardInputTag);
	if (bShouldResumeGuard)
	{
		bGuardResumeEligibleAfterAttack = true;
	}
	else
	{
		ClearGuardResumeEligibility();
	}

	if (!CharacterASC || !bWasGuarding || !GuardAbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer GuardAbilityTags;
	GuardAbilityTags.AddTag(GuardAbilityTag);
	CharacterASC->CancelAbilities(&GuardAbilityTags, nullptr);
}

void APlayerCharacter::ClearGuardResumeEligibility()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardResumeTimerHandle);
	}

	GuardResumeTimerHandle.Invalidate();
	bGuardResumeEligibleAfterAttack = false;
}

void APlayerCharacter::MarkGuardRequiresReleaseAfterGuardBreak()
{
	ClearGuardResumeEligibility();
	bGuardRequiresReleaseAfterBreak = true;
}

bool APlayerCharacter::IsStartupAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (!AbilityClass)
	{
		return false;
	}

	for (const TSubclassOf<UGameplayAbility>& StartupAbilityClass : StartupAbilities)
	{
		if (StartupAbilityClass == AbilityClass)
		{
			return true;
		}
	}

	return false;
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

void APlayerCharacter::HandleGuardActionStarted(const FInputActionValue&)
{
	HandleCombatInputStarted(GuardInputTag);
}

void APlayerCharacter::HandleGuardActionCompleted(const FInputActionValue&)
{
	bGuardRequiresReleaseAfterBreak = false;
	ClearGuardResumeEligibility();
	HandleCombatInputEnded(GuardInputTag, false);
}

void APlayerCharacter::HandleGuardActionCanceled(const FInputActionValue&)
{
	bGuardRequiresReleaseAfterBreak = false;
	ClearGuardResumeEligibility();
	HandleCombatInputEnded(GuardInputTag, true);
}

void APlayerCharacter::HandleParryActionStarted(const FInputActionValue&)
{
	HandleCombatInputStarted(ParryInputTag);
}

void APlayerCharacter::HandleParryActionCompleted(const FInputActionValue&)
{
	// Releasing Q only clears the held input record; it never interrupts a started Parry.
	HandleCombatInputEnded(ParryInputTag, false);
}

void APlayerCharacter::HandleParryActionCanceled(const FInputActionValue&)
{
	HandleCombatInputEnded(ParryInputTag, true);
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

void APlayerCharacter::HandleInteractStarted(const FInputActionValue&)
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!WeaponEquipment || IsActorBeingDestroyed() || (CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag)))
	{
		return;
	}

	AWorldWeaponPickup* Candidate = CurrentWorldPickupCandidate.Get();
	if (!Candidate || !Candidate->CanInteract(this))
	{
		RefreshWorldPickupInteractionPrompt();
		return;
	}

	UWeaponDefinition* IncomingDefinition = Candidate->GetWeaponDefinition();
	const bool bSuccess = WeaponEquipment->TryEquipWorldPickup(Candidate);
	OnWorldPickupInteractionResult(bSuccess, IncomingDefinition);

	RefreshWorldPickupInteractionPrompt();
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

	if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent())
	{
		if (CharacterASC->HasMatchingGameplayTag(ExhaustedStateTag) || CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f)
		{
			NotifyStaminaActionRejected();
			return;
		}
	}

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
	// Ability slots activate prepared equipment actions through their exact granted handles.
	for (int32 SlotIndex = 0; SlotIndex < AbilitySlotInputTags.Num(); ++SlotIndex)
	{
		if (InputIntentTag == AbilitySlotInputTags[SlotIndex])
		{
			if (WeaponEquipment)
			{
				const bool bActivated = WeaponEquipment->TryActivatePreparedSlot(SlotIndex);
				if (!bActivated)
				{
					UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
					if (CharacterASC && (CharacterASC->HasMatchingGameplayTag(ExhaustedStateTag) || CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f))
					{
						NotifyStaminaActionRejected();
					}
				}
			}
			return;
		}
	}

	// The Sprint Attack shortcut keeps only the physical-state predicate here; the
	// ability tag resolves from the equipped main hand's Base Input Profile.
	if (InputIntentTag == PrimaryAttackInputTag && ShouldRequestSprintAttack() && WeaponEquipment)
	{
		FGameplayTag SprintAttackAbilityTag;
		if (WeaponEquipment->TryGetSprintAttackAbilityTag(SprintAttackAbilityTag) && SprintAttackAbilityTag.IsValid())
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
				else if (CharacterASC->HasMatchingGameplayTag(ExhaustedStateTag) || CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f)
				{
					NotifyStaminaActionRejected();
					return;
				}
			}
		}
	}

	// Melee special execution attempts: Front execution has precedence over Backstab on non-sprint PrimaryAttack with an equipped melee main hand.
	if (InputIntentTag == PrimaryAttackInputTag && !ShouldRequestSprintAttack() && WeaponEquipment)
	{
		const UMeleeWeaponDefinition* MeleeWeapon = Cast<UMeleeWeaponDefinition>(WeaponEquipment->GetCurrentMainHandWeapon());
		if (MeleeWeapon)
		{
			if (UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent())
			{
				const FGameplayTag FrontExecutionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Front")), false);
				if (FrontExecutionTag.IsValid())
				{
					FGameplayTagContainer ExecutionTags;
					ExecutionTags.AddTag(FrontExecutionTag);
					if (CharacterASC->TryActivateAbilitiesByTag(ExecutionTags))
					{
						UE_LOG(LogPolyQuest, Verbose, TEXT("CombatInput: owner='%s', intent='%s', ability='%s', activationRequested=true."), *GetNameSafe(this), *InputIntentTag.ToString(), *FrontExecutionTag.ToString());
						return;
					}
				}

				const FGameplayTag BackstabExecutionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Backstab")), false);
				if (BackstabExecutionTag.IsValid())
				{
					FGameplayTagContainer ExecutionTags;
					ExecutionTags.AddTag(BackstabExecutionTag);
					if (CharacterASC->TryActivateAbilitiesByTag(ExecutionTags))
					{
						UE_LOG(LogPolyQuest, Verbose, TEXT("CombatInput: owner='%s', intent='%s', ability='%s', activationRequested=true."), *GetNameSafe(this), *InputIntentTag.ToString(), *BackstabExecutionTag.ToString());
						return;
					}
				}
			}
		}
	}

	// The equipment component is the single resolver for Primary and the Effective Defense Profile.
	FGameplayTag AbilityTag;
	if (!WeaponEquipment || !WeaponEquipment->TryResolveInputIntent(InputIntentTag, AbilityTag) || !AbilityTag.IsValid())
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
	if (!bActivated && (CharacterASC->HasMatchingGameplayTag(ExhaustedStateTag) || CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f))
	{
		NotifyStaminaActionRejected();
	}
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
	const bool bActivated = CharacterASC->TryActivateAbilitiesByTag(AbilityTags);
	if (!bActivated && (CharacterASC->HasMatchingGameplayTag(ExhaustedStateTag) || CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f))
	{
		NotifyStaminaActionRejected();
	}
}

void APlayerCharacter::ResumeGuardAfterAttack()
{
	GuardResumeTimerHandle.Invalidate();
	if (!bGuardResumeEligibleAfterAttack || IsActorBeingDestroyed())
	{
		ClearGuardResumeEligibility();
		return;
	}

	// A still-active Parry owns this retry; its own tag removal re-triggers this
	// function, so the qualification must survive instead of being consumed as a
	// failed retry when an attack the Parry cancelled clears its tag first.
	if (const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
		CharacterASC && ParryingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ParryingStateTag))
	{
		return;
	}

	bGuardResumeEligibleAfterAttack = false;
	if (!CanAttemptGuard())
	{
		TryStartSprint();
		return;
	}

	RequestAbilityForInputIntent(GuardInputTag);
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

void APlayerCharacter::ApplyLockAwareActionFacing()
{
	FVector LockedDirection = FVector::ZeroVector;
	if (TryGetLockedTargetDirection(LockedDirection))
	{
		SetActorRotation(FRotator(0.0f, LockedDirection.Rotation().Yaw, 0.0f));
		return;
	}

	ApplyActionFacing();
}

void APlayerCharacter::ApplyDodgeFacing()
{
	if (!CurrentMoveInput.IsNearlyZero())
	{
		ApplyActionFacing();
		return;
	}

	ApplyLockAwareActionFacing();
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSeeThroughOcclusion(DeltaSeconds);

	if (LockedTarget.IsValid())
	{
		ValidateCurrentLockedTarget();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (GEngine && LockedTarget.IsValid())
		{
			const AEnemyCharacter* TargetActor = LockedTarget.Get();
			const float Dist2D = FVector::Dist2D(GetActorLocation(), TargetActor->GetActorLocation());
			const float Dist3D = FVector::Distance(GetActorLocation(), TargetActor->GetActorLocation());
			const FString Msg = FString::Printf(TEXT("[LockOn 实时距离] 目标: %s | 水平距离(2D): %.1f cm (%.2f m) | 直线距离(3D): %.1f cm"),
				*GetNameSafe(TargetActor), Dist2D, Dist2D * 0.01f, Dist3D);
			GEngine->AddOnScreenDebugMessage(1002, 0.0f, FColor::Turquoise, Msg);
		}
#endif
	}
	else
	{
		// A destroyed target can leave a stale weak pointer without a valid actor to unhighlight.
		LockedTarget.Reset();
		LastValidLockedTargetCandidate.Reset();
	}

	if (ActiveBowAimRequester.IsValid())
	{
		UpdateBowAimFacing();
	}

	UpdateActionFacingRotationMode();
	UpdateLockedLocomotionFacing(DeltaSeconds);
}

void APlayerCharacter::HandleLockOnStarted(const FInputActionValue&)
{
	const ELockOnValidationResult ValidationResult = ValidateCurrentLockedTarget();
	if (ValidationResult == ELockOnValidationResult::Valid)
	{
		ClearLockedTarget();
		return;
	}
	if (ValidationResult == ELockOnValidationResult::RetargetedAfterDeath)
	{
		return;
	}

	TryAcquireLockOnTarget();
}

void APlayerCharacter::HandleTargetCycleTriggered(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (!FMath::IsFinite(AxisValue) || FMath::IsNearlyZero(AxisValue)
		|| ValidateCurrentLockedTarget() != ELockOnValidationResult::Valid)
	{
		return;
	}

	TArray<FPlayerLockOnCandidate> Candidates;
	FVector2D PlayerScreenPosition = FVector2D::ZeroVector;
	if (!BuildLockOnCandidates(Candidates, PlayerScreenPosition))
	{
		ClearLockedTarget();
		return;
	}

	FPlayerLockOnTargeting::SortClockwise(Candidates);
	const int32 NextIndex = FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, LockedTarget.Get(), AxisValue > 0.0f ? 1 : -1);
	if (NextIndex == INDEX_NONE)
	{
		AEnemyCharacter* CurrentTarget = LockedTarget.Get();
		const APlayerController* PlayerController = Cast<APlayerController>(GetController());
		const UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
		FVector2D TargetScreenPosition = FVector2D::ZeroVector;
		FVector2D TargetViewportSize = FVector2D::ZeroVector;

		if (CurrentTarget
			&& PlayerController
			&& SourceASC
			&& FCombatProjectileTargeting::IsValidTargetCandidate(this, SourceASC, CurrentTarget)
			&& TryProjectLockOnWorldPoint(PlayerController, FCombatProjectileTargeting::GetTargetAimPoint(CurrentTarget), TargetScreenPosition, TargetViewportSize, LockOnRetentionMarginRatio)
			&& !FPlayerLockOnTargeting::IsStrictlyWithinViewport(TargetScreenPosition, TargetViewportSize))
		{
			return;
		}

		ClearLockedTarget();
		return;
	}

	SetLockedTarget(Candidates[NextIndex].TargetActor.Get(), &Candidates[NextIndex]);
}

bool APlayerCharacter::TryAcquireLockOnTarget()
{
	TArray<FPlayerLockOnCandidate> Candidates;
	FVector2D PlayerScreenPosition = FVector2D::ZeroVector;
	if (!BuildLockOnCandidates(Candidates, PlayerScreenPosition))
	{
		return false;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	float MouseX = 0.0f;
	float MouseY = 0.0f;

#if WITH_DEV_AUTOMATION_TESTS
	if (TestLockOnCursorPosition.IsSet())
	{
		MouseX = TestLockOnCursorPosition->X;
		MouseY = TestLockOnCursorPosition->Y;
	}
	else
#endif
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	const int32 SelectedIndex = FPlayerLockOnTargeting::FindNearestToCursor(Candidates, FVector2D(MouseX, MouseY));
	if (SelectedIndex == INDEX_NONE)
	{
		return false;
	}

	SetLockedTarget(Candidates[SelectedIndex].TargetActor.Get(), &Candidates[SelectedIndex]);
	return LockedTarget.IsValid();
}

bool APlayerCharacter::BuildLockOnCandidates(TArray<FPlayerLockOnCandidate>& OutCandidates, FVector2D& OutPlayerScreenPosition) const
{
	OutCandidates.Reset();
	OutPlayerScreenPosition = FVector2D::ZeroVector;

	UWorld* World = GetWorld();
	const UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!World || !SourceASC || !PlayerController)
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (!PlayerController->IsLocalController() && !TestLockOnProjectionHook)
	{
		return false;
	}
#else
	if (!PlayerController->IsLocalController())
	{
		return false;
	}
#endif

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (!TryProjectLockOnWorldPoint(PlayerController, FCombatProjectileTargeting::GetTargetAimPoint(this), OutPlayerScreenPosition, ViewportSize, 0.0f))
	{
		return false;
	}

	for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
	{
		AEnemyCharacter* CandidateActor = *It;
		if (!CandidateActor || !FCombatProjectileTargeting::IsValidTargetCandidate(this, SourceASC, CandidateActor))
		{
			continue;
		}

		const FVector AimPoint = FCombatProjectileTargeting::GetTargetAimPoint(CandidateActor);
		FVector2D CandidateScreenPosition = FVector2D::ZeroVector;
		FVector2D CandidateViewportSize = FVector2D::ZeroVector;
		if (!TryProjectLockOnWorldPoint(PlayerController, AimPoint, CandidateScreenPosition, CandidateViewportSize, 0.0f))
		{
			continue;
		}

		float ClockwiseAngleRadians = 0.0f;
		if (!FPlayerLockOnTargeting::TryCalculateClockwiseAngle(OutPlayerScreenPosition, CandidateScreenPosition, ClockwiseAngleRadians))
		{
			continue;
		}

		const float PlayerScreenDistanceSquared = FVector2D::DistSquared(CandidateScreenPosition, OutPlayerScreenPosition);
		if (!FMath::IsFinite(PlayerScreenDistanceSquared))
		{
			continue;
		}

		FPlayerLockOnCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.TargetActor = CandidateActor;
		Candidate.ScreenPosition = CandidateScreenPosition;
		Candidate.ClockwiseAngleRadians = ClockwiseAngleRadians;
		Candidate.PlayerScreenDistanceSquared = PlayerScreenDistanceSquared;
		Candidate.StableKey = CandidateActor->GetPathName();
	}

	return true;
}

bool APlayerCharacter::TryProjectLockOnWorldPoint(
	const APlayerController* PlayerController,
	const FVector& WorldPoint,
	FVector2D& OutScreenPosition,
	FVector2D& OutViewportSize,
	const float MarginRatio) const
{
	OutScreenPosition = FVector2D::ZeroVector;
	OutViewportSize = FVector2D::ZeroVector;
	if (!FMath::IsFinite(WorldPoint.X) || !FMath::IsFinite(WorldPoint.Y) || !FMath::IsFinite(WorldPoint.Z))
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (TestLockOnProjectionHook)
	{
		return TestLockOnProjectionHook(WorldPoint, OutScreenPosition, OutViewportSize)
			&& FPlayerLockOnTargeting::IsWithinViewportWithMargin(OutScreenPosition, OutViewportSize, MarginRatio);
	}
#endif

	if (!PlayerController || !PlayerController->IsLocalController()
		|| !PlayerController->PlayerCameraManager)
	{
		return false;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	OutViewportSize = FVector2D(static_cast<float>(ViewportSizeX), static_cast<float>(ViewportSizeY));
	if (!FMath::IsFinite(OutViewportSize.X) || !FMath::IsFinite(OutViewportSize.Y)
		|| OutViewportSize.X <= 0.0f || OutViewportSize.Y <= 0.0f)
	{
		return false;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
	const float CameraDot = FVector::DotProduct(CameraForward, WorldPoint - CameraLocation);
	if (!FMath::IsFinite(CameraLocation.X) || !FMath::IsFinite(CameraLocation.Y) || !FMath::IsFinite(CameraLocation.Z)
		|| !FMath::IsFinite(CameraForward.X) || !FMath::IsFinite(CameraForward.Y) || !FMath::IsFinite(CameraForward.Z)
		|| !FMath::IsFinite(CameraDot) || CameraDot <= 0.0f
		|| !PlayerController->ProjectWorldLocationToScreen(WorldPoint, OutScreenPosition, false))
	{
		return false;
	}

	return FPlayerLockOnTargeting::IsWithinViewportWithMargin(OutScreenPosition, OutViewportSize, MarginRatio);
}

APlayerCharacter::ELockOnValidationResult APlayerCharacter::ValidateCurrentLockedTarget()
{
	AEnemyCharacter* CurrentTarget = LockedTarget.Get();
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	if (!CurrentTarget || !PlayerController || !SourceASC)
	{
		ClearLockedTarget();
		return ELockOnValidationResult::Cleared;
	}
	if (DeadStateTag.IsValid() && SourceASC->HasMatchingGameplayTag(DeadStateTag))
	{
		ClearLockedTarget();
		return ELockOnValidationResult::Cleared;
	}

	if (CurrentTarget->IsDead())
	{
		return TryRetargetAfterLockedTargetDeath(CurrentTarget)
			? ELockOnValidationResult::RetargetedAfterDeath
			: ELockOnValidationResult::Cleared;
	}

	const bool bCandidateValid = FCombatProjectileTargeting::IsValidTargetCandidate(this, SourceASC, CurrentTarget);
	const bool bCanRetainExecution = !bCandidateValid && CanRetainExecutionLockedTarget(CurrentTarget, SourceASC);

	if ((!bCandidateValid && !bCanRetainExecution) || !CacheCurrentLockedTargetCandidate())
	{
		ClearLockedTarget();
		return ELockOnValidationResult::Cleared;
	}

	return ELockOnValidationResult::Valid;
}

bool APlayerCharacter::CanRetainExecutionLockedTarget(const AEnemyCharacter* CurrentTarget, const UAbilitySystemComponent* SourceASC) const
{
	if (!CurrentTarget || !SourceASC)
	{
		return false;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const UWorld* CurrentWorld = GetWorld();
	if (!PlayerController || !CurrentWorld || CurrentWorld != CurrentTarget->GetWorld())
	{
		return false;
	}

	if (CurrentTarget->IsActorBeingDestroyed())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = CurrentTarget->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return false;
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	if (!DeadTag.IsValid() || !InvulnerableTag.IsValid())
	{
		return false;
	}

	// Target can be retained through lethal execution while in DeathPending, but not once truly Dead.
	if (IsActorBeingDestroyed() || SourceASC->HasMatchingGameplayTag(DeadTag)
		|| CurrentTarget->IsDead() || TargetASC->HasMatchingGameplayTag(DeadTag))
	{
		return false;
	}

	const FGameplayTag PlayerLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	if (!PlayerLockedTag.IsValid() || !VictimLockedTag.IsValid())
	{
		return false;
	}

	if (!SourceASC->HasMatchingGameplayTag(PlayerLockedTag) || !TargetASC->HasMatchingGameplayTag(VictimLockedTag))
	{
		return false;
	}

	// The retention branch compensates for the target's active paired execution lock (including DeathPending prior to true Dead).
	if (!TargetASC->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
	}

	if (!GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass())
		|| !CurrentTarget->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass()))
	{
		return false;
	}

	const FGameplayTag SourceTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(this);
	const FGameplayTag TargetTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(CurrentTarget);
	if (!SourceTeamTag.IsValid() || !TargetTeamTag.IsValid() || SourceTeamTag.MatchesTagExact(TargetTeamTag))
	{
		return false;
	}

	return true;
}

AEnemyCharacter* APlayerCharacter::ResolveValidLockedTarget()
{
	return ValidateCurrentLockedTarget() == ELockOnValidationResult::Cleared
		? nullptr
		: LockedTarget.Get();
}

bool APlayerCharacter::CacheCurrentLockedTargetCandidate()
{
	AEnemyCharacter* CurrentTarget = LockedTarget.Get();
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!CurrentTarget || !PlayerController)
	{
		return false;
	}

	FVector2D PlayerScreenPosition = FVector2D::ZeroVector;
	FVector2D PlayerViewportSize = FVector2D::ZeroVector;
	FVector2D TargetScreenPosition = FVector2D::ZeroVector;
	FVector2D TargetViewportSize = FVector2D::ZeroVector;
	if (!TryProjectLockOnWorldPoint(PlayerController, FCombatProjectileTargeting::GetTargetAimPoint(this), PlayerScreenPosition, PlayerViewportSize, 0.0f)
		|| !TryProjectLockOnWorldPoint(PlayerController, FCombatProjectileTargeting::GetTargetAimPoint(CurrentTarget), TargetScreenPosition, TargetViewportSize, LockOnRetentionMarginRatio))
	{
		return false;
	}

	float ClockwiseAngleRadians = 0.0f;
	const float PlayerScreenDistanceSquared = FVector2D::DistSquared(TargetScreenPosition, PlayerScreenPosition);
	if (!FPlayerLockOnTargeting::TryCalculateClockwiseAngle(PlayerScreenPosition, TargetScreenPosition, ClockwiseAngleRadians)
		|| !FMath::IsFinite(PlayerScreenDistanceSquared))
	{
		return false;
	}

	FPlayerLockOnCandidate Candidate;
	Candidate.TargetActor = CurrentTarget;
	Candidate.ScreenPosition = TargetScreenPosition;
	Candidate.ClockwiseAngleRadians = ClockwiseAngleRadians;
	Candidate.PlayerScreenDistanceSquared = PlayerScreenDistanceSquared;
	Candidate.StableKey = CurrentTarget->GetPathName();
	LastValidLockedTargetCandidate = MoveTemp(Candidate);
	return true;
}

bool APlayerCharacter::TryRetargetAfterLockedTargetDeath(AEnemyCharacter* DeadTarget)
{
	if (!DeadTarget || !LastValidLockedTargetCandidate.IsSet()
		|| LastValidLockedTargetCandidate->TargetActor.Get() != DeadTarget)
	{
		ClearLockedTarget();
		return false;
	}

	TArray<FPlayerLockOnCandidate> Candidates;
	FVector2D PlayerScreenPosition = FVector2D::ZeroVector;
	if (!BuildLockOnCandidates(Candidates, PlayerScreenPosition))
	{
		ClearLockedTarget();
		return false;
	}

	FPlayerLockOnTargeting::SortClockwise(Candidates);
	const int32 NextIndex = FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, LastValidLockedTargetCandidate.GetValue());
	if (NextIndex == INDEX_NONE)
	{
		ClearLockedTarget();
		return false;
	}

	SetLockedTarget(Candidates[NextIndex].TargetActor.Get(), &Candidates[NextIndex]);
	return LockedTarget.IsValid();
}

bool APlayerCharacter::TryGetLockedTargetDirection(FVector& OutDirection)
{
	OutDirection = FVector::ZeroVector;

#if WITH_DEV_AUTOMATION_TESTS
	if (!bTestBypassLockOnValidation && ValidateCurrentLockedTarget() == ELockOnValidationResult::Cleared)
	{
		return false;
	}
#else
	if (ValidateCurrentLockedTarget() == ELockOnValidationResult::Cleared)
	{
		return false;
	}
#endif

	return TryGetLockedTargetDirectionUnchecked(OutDirection);
}

bool APlayerCharacter::TryGetLockedTargetDirectionUnchecked(FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;
	const AEnemyCharacter* CurrentTarget = LockedTarget.Get();
	if (!CurrentTarget)
	{
		return false;
	}

	FVector Direction = CurrentTarget->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;
	if (!FMath::IsFinite(Direction.X) || !FMath::IsFinite(Direction.Y) || Direction.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutDirection = Direction.GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

void APlayerCharacter::SetLockedTarget(AEnemyCharacter* NewTarget, const FPlayerLockOnCandidate* Candidate)
{
	if (LockedTarget.Get() == NewTarget)
	{
		if (Candidate && Candidate->TargetActor.Get() == NewTarget)
		{
			LastValidLockedTargetCandidate = *Candidate;
		}
		else
		{
			CacheCurrentLockedTargetCandidate();
		}
		UpdateActionFacingRotationMode();
		return;
	}

	ClearLockedTarget();
	if (!NewTarget)
	{
		return;
	}

	LockedTarget = NewTarget;
	NewTarget->SetPlayerLockOnHighlighted(true);
	if (Candidate && Candidate->TargetActor.Get() == NewTarget)
	{
		LastValidLockedTargetCandidate = *Candidate;
	}
	else
	{
		CacheCurrentLockedTargetCandidate();
	}
	UpdateActionFacingRotationMode();
}

void APlayerCharacter::ClearLockedTarget()
{
	ClearMeleeMotionWarpTargets();

	if (AEnemyCharacter* PreviousTarget = LockedTarget.Get())
	{
		PreviousTarget->SetPlayerLockOnHighlighted(false);
	}

	LockedTarget.Reset();
	LastValidLockedTargetCandidate.Reset();
	UpdateActionFacingRotationMode();
}

void APlayerCharacter::ClearMeleeMotionWarpTargetsForInvalidatedTarget(const AEnemyCharacter* InvalidatedTarget)
{
	if (InvalidatedTarget && LockedTarget.Get() == InvalidatedTarget)
	{
		ClearMeleeMotionWarpTargets();
	}
}

bool APlayerCharacter::SetMeleeMotionWarpTarget(FName WarpTargetName, const FTransform& TargetTransform)
{
	const FVector Location = TargetTransform.GetLocation();
	if (!MotionWarpingComponent || WarpTargetName.IsNone() || !TargetTransform.IsValid()
		|| !FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z))
	{
		return false;
	}

	MotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(WarpTargetName, TargetTransform);
	return true;
}

void APlayerCharacter::ClearMeleeMotionWarpTargets()
{
	if (MotionWarpingComponent)
	{
		MotionWarpingComponent->RemoveAllWarpTargets();
	}
}

bool APlayerCharacter::RegisterBowAimRequester(const UObject* Requester)
{
	if (!Requester)
	{
		return false;
	}

	ActiveBowAimRequester = Requester;
	UpdateBowAimFacing();
	return true;
}

void APlayerCharacter::UnregisterBowAimRequester(const UObject* Requester)
{
	if (ActiveBowAimRequester.Get() == Requester)
	{
		ActiveBowAimRequester = nullptr;
		bHasValidBowAimDirection = false;
		LastValidBowAimDirection = FVector::ZeroVector;
	}
}

bool APlayerCharacter::HasActiveBowAimRequester() const
{
	return ActiveBowAimRequester.IsValid();
}

bool APlayerCharacter::TryGetBowAimWorldDirection(FVector& OutDirection) const
{
	if (bHasValidBowAimDirection && !LastValidBowAimDirection.ContainsNaN() && LastValidBowAimDirection.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		OutDirection = LastValidBowAimDirection;
		return true;
	}
	return false;
}

bool APlayerCharacter::CalculateRayPlaneIntersection(
	const FVector& WorldOrigin,
	const FVector& WorldDirection,
	const float PlaneZ,
	FVector& OutIntersectionPoint)
{
	if (WorldOrigin.ContainsNaN() || WorldDirection.ContainsNaN() || !FMath::IsFinite(PlaneZ))
	{
		return false;
	}

	if (FMath::IsNearlyZero(WorldDirection.Z, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	const float T = (PlaneZ - WorldOrigin.Z) / WorldDirection.Z;
	if (T <= 0.0f || !FMath::IsFinite(T))
	{
		return false;
	}

	OutIntersectionPoint = WorldOrigin + WorldDirection * T;
	return !OutIntersectionPoint.ContainsNaN();
}

void APlayerCharacter::UpdateSeeThroughOcclusion(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !PlayerGlobalsMPC)
	{
		return;
	}

	const FVector PlayerChestLoc = GetActorLocation() + FVector(0.f, 0.f, SeeThroughChestZOffset);

	FVector CameraLoc = FVector::ZeroVector;
	if (FollowCamera)
	{
		CameraLoc = FollowCamera->GetComponentLocation();
	}
	else if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const APlayerCameraManager* CamManager = PC->PlayerCameraManager)
		{
			CameraLoc = CamManager->GetCameraLocation();
		}
	}

	const FVector CameraToChest = PlayerChestLoc - CameraLoc;
	const float TotalDist = CameraToChest.Size();
	const FVector TraceDir = TotalDist > KINDA_SMALL_NUMBER ? (CameraToChest / TotalDist) : FVector::ForwardVector;
	const float SafeOffset = FMath::Min(SeeThroughNearClipOffset, FMath::Max(0.0f, TotalDist - 100.0f));
	const FVector SweepStartLoc = CameraLoc + TraceDir * SafeOffset;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SeeThroughOcclusionTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	FHitResult HitResult;
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(SeeThroughSweepRadius);
	const bool bHit = World->SweepSingleByChannel(
		HitResult,
		SweepStartLoc,
		PlayerChestLoc,
		FQuat::Identity,
		SeeThroughTraceChannel,
		SweepShape,
		QueryParams
	);

	const bool bIsOccluded = bHit && HitResult.GetActor() && (HitResult.GetActor() != this);
	const float TargetRadius = bIsOccluded ? MaxTunnelRadius : 0.0f;
	const float ActiveInterpSpeed = bIsOccluded ? TunnelRadiusOpenInterpSpeed : TunnelRadiusCloseInterpSpeed;

	CurrentTunnelRadius = FMath::FInterpTo(CurrentTunnelRadius, TargetRadius, DeltaSeconds, ActiveInterpSpeed);

	UKismetMaterialLibrary::SetVectorParameterValue(World, PlayerGlobalsMPC, FName(TEXT("PlayerPosition")), FLinearColor(PlayerChestLoc));
	UKismetMaterialLibrary::SetScalarParameterValue(World, PlayerGlobalsMPC, FName(TEXT("TunnelRadius")), CurrentTunnelRadius);
	UKismetMaterialLibrary::SetScalarParameterValue(World, PlayerGlobalsMPC, FName(TEXT("CeilingRadius")), MaxCeilingRadius);
}

bool APlayerCharacter::TryCalculateMousePlaneIntersection(FVector& OutIntersectionPoint) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	bool bDeprojected = PC->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection);
	if (!bDeprojected)
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			bDeprojected = UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(MouseX, MouseY), WorldOrigin, WorldDirection);
		}
	}

	if (!bDeprojected)
	{
		return false;
	}

	const float CharacterCenterZ = GetActorLocation().Z;
	return CalculateRayPlaneIntersection(WorldOrigin, WorldDirection, CharacterCenterZ, OutIntersectionPoint);
}

void APlayerCharacter::UpdateBowAimFacing()
{
	FVector IntersectionPoint = FVector::ZeroVector;
	const bool bIntersected = TryCalculateMousePlaneIntersection(IntersectionPoint);
	if (bIntersected)
	{
		FVector AimDir2D = IntersectionPoint - GetActorLocation();
		AimDir2D.Z = 0.0f;
		if (AimDir2D.SizeSquared2D() > KINDA_SMALL_NUMBER && !AimDir2D.ContainsNaN())
		{
			LastValidBowAimDirection = AimDir2D.GetSafeNormal2D();
			bHasValidBowAimDirection = true;
		}
	}

	if (bHasValidBowAimDirection)
	{
		ApplyBowAimFacing(LastValidBowAimDirection);
	}

#if !(UE_BUILD_SHIPPING)
	if (UWorld* World = GetWorld())
	{
		const FVector PlayerLoc = GetActorLocation();
		const FVector ForwardVec = GetActorForwardVector();

		// 1. Red sphere at Bow aim reference plane intersection & yellow line to intersection
		if (bIntersected)
		{
			DrawDebugSphere(World, IntersectionPoint, 20.0f, 12, FColor::Red, false, -1.0f, 0, 2.5f);
			DrawDebugLine(World, PlayerLoc, IntersectionPoint, FColor::Yellow, false, -1.0f, 0, 2.0f);
		}

		// 2. Green arrow showing current Character Capsule Forward
		DrawDebugDirectionalArrow(World, PlayerLoc + FVector(0,0,40), PlayerLoc + FVector(0,0,40) + ForwardVec * 350.0f, 50.0f, FColor::Green, false, -1.0f, 0, 3.5f);

		// 3. Cyan arrow showing calculated Bow Aim Direction
		if (bHasValidBowAimDirection)
		{
			DrawDebugDirectionalArrow(World, PlayerLoc + FVector(0,0,60), PlayerLoc + FVector(0,0,60) + LastValidBowAimDirection * 350.0f, 50.0f, FColor::Cyan, false, -1.0f, 0, 3.5f);
		}

		// 4. On-screen text with rotation angles and intersection coordinates
		if (GEngine)
		{
			const float ActorYaw = GetActorRotation().Yaw;
			const float AimYaw = LastValidBowAimDirection.Rotation().Yaw;
			const FString Msg = FString::Printf(TEXT("[Bow Aim Debug] ActorYaw: %.1f | AimYaw: %.1f | Intersected: %s | HitPoint: (%.0f, %.0f, %.0f)"),
				ActorYaw, AimYaw, bIntersected ? TEXT("TRUE") : TEXT("FALSE"),
				IntersectionPoint.X, IntersectionPoint.Y, IntersectionPoint.Z);
			GEngine->AddOnScreenDebugMessage(1001, 0.0f, FColor::Cyan, Msg);
		}
	}
#endif
}

void APlayerCharacter::ApplyBowAimFacing(const FVector& AimDirection)
{
	if (AimDirection.IsNearlyZero() || AimDirection.ContainsNaN())
	{
		return;
	}

	const FRotator TargetRotation(0.0f, AimDirection.Rotation().Yaw, 0.0f);
	SetActorRotation(TargetRotation);
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
	const bool bIsHitReacting = CharacterASC && HitReactingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(HitReactingStateTag);
	const bool bIsSmallHitReacting = CharacterASC && SmallHitReactingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(SmallHitReactingStateTag);
	const bool bIsDead = CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);
	const bool bIsStunned = CharacterASC && StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag);
	const bool bActionOwnsRotation = bIsAttacking || bIsDodging || bIsHitReacting || bIsSmallHitReacting
		|| bIsDead || bIsStunned || HasActiveBowAimRequester() || HasAnyRootMotion();
	MovementComponent->bOrientRotationToMovement = !bActionOwnsRotation && !CanApplyLockedLocomotionFacing();
}

bool APlayerCharacter::CanApplyLockedLocomotionFacing() const
{
	const AEnemyCharacter* CurrentTarget = LockedTarget.Get();
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!CurrentTarget || !CharacterASC || !MovementComponent || HasActiveSprint() || HasActiveBowAimRequester() || HasAnyRootMotion())
	{
		return false;
	}

	const bool bIsAttacking = AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag);
	const bool bIsDodging = DodgingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingStateTag);
	const bool bIsHitReacting = HitReactingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(HitReactingStateTag);
	const bool bIsSmallHitReacting = SmallHitReactingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(SmallHitReactingStateTag);
	const bool bIsDead = DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);
	const bool bIsStunned = StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag);
	if (bIsAttacking || bIsDodging || bIsHitReacting || bIsSmallHitReacting || bIsDead || bIsStunned)
	{
		return false;
	}

	const bool bIsParrying = ParryingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ParryingStateTag);
	if (!MovementComponent->IsMovingOnGround() && !(bIsParrying && MovementComponent->MovementMode == MOVE_None))
	{
		return false;
	}

	FVector LockedDirection = FVector::ZeroVector;
	return TryGetLockedTargetDirectionUnchecked(LockedDirection);
}

void APlayerCharacter::UpdateLockedLocomotionFacing(const float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f || !CanApplyLockedLocomotionFacing())
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	FVector LockedDirection = FVector::ZeroVector;
	if (!MovementComponent || !TryGetLockedTargetDirectionUnchecked(LockedDirection))
	{
		return;
	}

	const float RotationRateDegreesPerSecond = MovementComponent->RotationRate.Yaw;
	if (!FMath::IsFinite(RotationRateDegreesPerSecond) || RotationRateDegreesPerSecond <= 0.0f)
	{
		return;
	}

	const float MaxYawDelta = RotationRateDegreesPerSecond * DeltaSeconds;
	const float TargetYaw = LockedDirection.Rotation().Yaw;
	if (!FMath::IsFinite(MaxYawDelta) || !FMath::IsFinite(TargetYaw))
	{
		return;
	}

	const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, MaxYawDelta);
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
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
		&& !(GuardingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(GuardingStateTag))
		&& !(ParryingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ParryingStateTag))
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
	if (GuardingStateTag.IsValid())
	{
		GuardingStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(GuardingStateTag)
			.AddUObject(this, &APlayerCharacter::OnSprintRelevantTagChanged);
	}
	if (ParryingStateTag.IsValid())
	{
		ParryingStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(ParryingStateTag)
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
		if (GuardingStateTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(GuardingStateTagChangedHandle, GuardingStateTag);
		}
		if (ParryingStateTagChangedHandle.IsValid())
		{
			CharacterASC->UnregisterGameplayTagEvent(ParryingStateTagChangedHandle, ParryingStateTag);
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
	GuardingStateTagChangedHandle.Reset();
	ParryingStateTagChangedHandle.Reset();
	DeadStateTagChangedHandle.Reset();
	StunnedStateTagChangedHandle.Reset();
	SprintStateBoundAbilitySystemComponent.Reset();
}

void APlayerCharacter::BindHealthEvents()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || HealthBoundAbilitySystemComponent.Get() == CharacterASC)
	{
		return;
	}

	UnbindHealthEvents();
	HealthBoundAbilitySystemComponent = CharacterASC;
	HealthAttributeChangedHandle = CharacterASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
		.AddUObject(this, &APlayerCharacter::OnHealthAttributeChanged);
}

void APlayerCharacter::UnbindHealthEvents()
{
	UAbilitySystemComponent* BoundASC = HealthBoundAbilitySystemComponent.Get();
	if (BoundASC)
	{
		if (HealthAttributeChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
				.Remove(HealthAttributeChangedHandle);
		}
	}

	HealthAttributeChangedHandle.Reset();
	HealthBoundAbilitySystemComponent.Reset();
}

void APlayerCharacter::BindExhaustionStateEvents()
{
	if (!HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || ExhaustionBoundAbilitySystemComponent.Get() == CharacterASC)
	{
		return;
	}

	ClearExhaustionState();
	UnbindExhaustionStateEvents();
	ExhaustionBoundAbilitySystemComponent = CharacterASC;
	StaminaAttributeChangedHandle = CharacterASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &APlayerCharacter::OnStaminaAttributeChanged);

	if (DeadStateTag.IsValid())
	{
		ExhaustionDeadStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(DeadStateTag)
			.AddUObject(this, &APlayerCharacter::OnExhaustionDeadStateTagChanged);
	}

	if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) <= 0.0f)
	{
		BeginExhaustion();
	}
}

void APlayerCharacter::UnbindExhaustionStateEvents()
{
	UAbilitySystemComponent* BoundASC = ExhaustionBoundAbilitySystemComponent.Get();
	if (BoundASC)
	{
		if (StaminaAttributeChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute())
				.Remove(StaminaAttributeChangedHandle);
		}
		if (ExhaustionDeadStateTagChangedHandle.IsValid() && DeadStateTag.IsValid())
		{
			BoundASC->UnregisterGameplayTagEvent(ExhaustionDeadStateTagChangedHandle, DeadStateTag);
		}
	}

	StaminaAttributeChangedHandle.Reset();
	ExhaustionDeadStateTagChangedHandle.Reset();
	ExhaustionBoundAbilitySystemComponent.Reset();
}

void APlayerCharacter::OnStaminaAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}

	if (ChangeData.NewValue <= 0.0f)
	{
		BeginExhaustion();
		return;
	}

	TryClearExhaustionAfterRecovery();
}

void APlayerCharacter::OnExhaustionDeadStateTagChanged(const FGameplayTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ClearExhaustionState();
	}
}

void APlayerCharacter::BeginExhaustion()
{
	if (bExhaustionActive || !HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = ExhaustionBoundAbilitySystemComponent.Get();
	UWorld* World = GetWorld();
	if (!CharacterASC || !World || !ExhaustedStateTag.IsValid()
		|| (DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag)))
	{
		return;
	}

	bExhaustionActive = true;
	bExhaustionMinimumDurationElapsed = false;
	CharacterASC->AddLooseGameplayTag(ExhaustedStateTag);
	ApplyExhaustionMoveSpeedEffect();
	World->GetTimerManager().SetTimer(
		ExhaustionRecoveryTimerHandle,
		this,
		&APlayerCharacter::OnExhaustionMinimumDurationElapsed,
		ExhaustionMinimumDurationSeconds,
		false);
}

void APlayerCharacter::OnExhaustionMinimumDurationElapsed()
{
	ExhaustionRecoveryTimerHandle.Invalidate();
	if (!bExhaustionActive || !HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}

	bExhaustionMinimumDurationElapsed = true;
	TryClearExhaustionAfterRecovery();
}

void APlayerCharacter::TryClearExhaustionAfterRecovery()
{
	if (!bExhaustionActive || !bExhaustionMinimumDurationElapsed)
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = ExhaustionBoundAbilitySystemComponent.Get();
	if (!CharacterASC
		|| (DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag))
		|| CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()) > 0.0f)
	{
		ClearExhaustionState();
	}
}

void APlayerCharacter::ClearExhaustionState()
{
	const bool bRemoveOwnedExhaustionTag = bExhaustionActive;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExhaustionRecoveryTimerHandle);
	}
	ExhaustionRecoveryTimerHandle.Invalidate();

	UAbilitySystemComponent* BoundASC = ExhaustionBoundAbilitySystemComponent.Get();
	if (BoundASC && ExhaustionMoveSpeedEffectHandle.IsValid())
	{
		BoundASC->RemoveActiveGameplayEffect(ExhaustionMoveSpeedEffectHandle);
	}
	if (BoundASC && bRemoveOwnedExhaustionTag && ExhaustedStateTag.IsValid())
	{
		BoundASC->RemoveLooseGameplayTag(ExhaustedStateTag);
	}

	ExhaustionMoveSpeedEffectHandle.Invalidate();
	bExhaustionActive = false;
	bExhaustionMinimumDurationElapsed = false;
}

void APlayerCharacter::ApplyExhaustionMoveSpeedEffect()
{
	UAbilitySystemComponent* CharacterASC = ExhaustionBoundAbilitySystemComponent.Get();
	const UGameplayEffect* ExhaustionMoveSpeedEffect = ExhaustionMoveSpeedGameplayEffectClass
		? ExhaustionMoveSpeedGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!CharacterASC || !ExhaustionMoveSpeedEffect)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot apply Exhaustion move speed without an ASC and configured GameplayEffect."), *GetNameSafe(this));
		return;
	}

	if (ExhaustionMoveSpeedEffectHandle.IsValid())
	{
		CharacterASC->RemoveActiveGameplayEffect(ExhaustionMoveSpeedEffectHandle);
		ExhaustionMoveSpeedEffectHandle.Invalidate();
	}

	ExhaustionMoveSpeedEffectHandle = CharacterASC->ApplyGameplayEffectToSelf(
		ExhaustionMoveSpeedEffect,
		1.0f,
		CharacterASC->MakeEffectContext());
	if (!ExhaustionMoveSpeedEffectHandle.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("'%s' failed to apply its configured Exhaustion move-speed GameplayEffect."), *GetNameSafe(this));
	}
}

void APlayerCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}

	// Lethal check: Player has no death implementation in C3E; simply send no reaction.
	if (ChangeData.NewValue <= 0.0f)
	{
		return;
	}

	if (ChangeData.NewValue >= ChangeData.OldValue || !ChangeData.GEModData)
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return;
	}

	const bool bIsDead = DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag);
	if (bIsDead)
	{
		return;
	}

	TriggerHitFeedbackOverlay();

	FGameplayTagContainer AssetTags;
	ChangeData.GEModData->EffectSpec.GetAllAssetTags(AssetTags);

	const EHitReactionTier ReactionTier = FHitReactionClassifier::ClassifyReactionTier(AssetTags);
	TriggerHitFeedbackCameraShake(ReactionTier);

	const float PunchDegrees = (ReactionTier == EHitReactionTier::Big || ReactionTier == EHitReactionTier::Launch) ? 1.5f : 0.0f;
	TriggerCameraFovPunch(PunchDegrees);

	const bool bIsFirstHealthModifierInSpec = (ChangeData.GEModData->EffectSpec.GetModifiedAttribute(UCharacterAttributeSet::GetHealthAttribute()) == nullptr);
	if (bIsFirstHealthModifierInSpec)
	{
		TriggerReceivedHitSound(ChangeData.GEModData->EffectSpec);
	}

	const bool bIsStunned = StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag);
	if (bIsStunned)
	{
		return;
	}

	if (ReactionTier == EHitReactionTier::Invalid)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player '%s' received invalid multi-tier hit reaction tags from effect '%s'; skipping reaction event."),
			*GetNameSafe(this), *GetNameSafe(ChangeData.GEModData->EffectSpec.Def));
		return;
	}

	if (ReactionTier == EHitReactionTier::Small)
	{
		if (SmallHitReactionEventTag.IsValid())
		{
			FGameplayEventData ReactionEventData;
			ReactionEventData.EventTag = SmallHitReactionEventTag;
			ReactionEventData.Instigator = ChangeData.GEModData->EffectSpec.GetContext().GetInstigator();
			ReactionEventData.Target = this;
			ReactionEventData.EventMagnitude = ChangeData.OldValue - ChangeData.NewValue;
			ReactionEventData.ContextHandle = ChangeData.GEModData->EffectSpec.GetContext();
			CharacterASC->HandleGameplayEvent(SmallHitReactionEventTag, &ReactionEventData);
		}
	}
	else if (ReactionTier == EHitReactionTier::Big)
	{
		if (BigHitReactionEventTag.IsValid())
		{
			FGameplayEventData ReactionEventData;
			ReactionEventData.EventTag = BigHitReactionEventTag;
			ReactionEventData.Instigator = ChangeData.GEModData->EffectSpec.GetContext().GetInstigator();
			ReactionEventData.Target = this;
			ReactionEventData.EventMagnitude = ChangeData.OldValue - ChangeData.NewValue;
			ReactionEventData.ContextHandle = ChangeData.GEModData->EffectSpec.GetContext();
			CharacterASC->HandleGameplayEvent(BigHitReactionEventTag, &ReactionEventData);
		}
	}
	else if (ReactionTier == EHitReactionTier::Launch)
	{
		if (LaunchReactionEventTag.IsValid())
		{
			FGameplayEventData ReactionEventData;
			ReactionEventData.EventTag = LaunchReactionEventTag;
			ReactionEventData.Instigator = ChangeData.GEModData->EffectSpec.GetContext().GetInstigator();
			ReactionEventData.Target = this;
			ReactionEventData.EventMagnitude = ChangeData.OldValue - ChangeData.NewValue;
			ReactionEventData.ContextHandle = ChangeData.GEModData->EffectSpec.GetContext();
			CharacterASC->HandleGameplayEvent(LaunchReactionEventTag, &ReactionEventData);
		}
	}
	// None is a legal no-op for Player.
}

UPlayerCombatFeedbackDataAsset* APlayerCharacter::GetPlayerCombatFeedbackData() const
{
	if (!CombatFeedbackData)
	{
		if (!bHasLoggedMissingCombatFeedbackData)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player '%s' is missing a CombatFeedbackData profile."), *GetNameSafe(this));
			const_cast<APlayerCharacter*>(this)->bHasLoggedMissingCombatFeedbackData = true;
		}
		return nullptr;
	}

	UPlayerCombatFeedbackDataAsset* PlayerProfile = Cast<UPlayerCombatFeedbackDataAsset>(CombatFeedbackData.Get());
	if (!PlayerProfile)
	{
		if (!bHasLoggedMissingCombatFeedbackData)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Player '%s' has an invalid CombatFeedbackData profile (expected UPlayerCombatFeedbackDataAsset, got '%s')."), *GetNameSafe(this), *GetNameSafe(CombatFeedbackData.Get()));
			const_cast<APlayerCharacter*>(this)->bHasLoggedMissingCombatFeedbackData = true;
		}
		return nullptr;
	}

	return PlayerProfile;
}

TSubclassOf<UCameraShakeBase> APlayerCharacter::ResolveHitFeedbackCameraShakeClass(const EHitReactionTier ReactionTier)
{
	if (ReactionTier == EHitReactionTier::None || ReactionTier == EHitReactionTier::Invalid)
	{
		return nullptr;
	}

	const UPlayerCombatFeedbackDataAsset* FeedbackData = GetPlayerCombatFeedbackData();
	if (!FeedbackData)
	{
		return nullptr;
	}

	const FPlayerCombatFeedbackTierSettings* TierSettings = FeedbackData->GetTierSettings(ReactionTier);
	return TierSettings ? TierSettings->ReceivedHitCameraShakeClass : nullptr;
}

TSubclassOf<UCameraShakeBase> APlayerCharacter::ResolveAttackerImpactCameraShakeClass(const EHitReactionTier ReactionTier)
{
	if (ReactionTier == EHitReactionTier::None || ReactionTier == EHitReactionTier::Invalid)
	{
		return nullptr;
	}

	const UPlayerCombatFeedbackDataAsset* FeedbackData = GetPlayerCombatFeedbackData();
	if (!FeedbackData)
	{
		return nullptr;
	}

	const FPlayerCombatFeedbackTierSettings* TierSettings = FeedbackData->GetTierSettings(ReactionTier);
	return TierSettings ? TierSettings->AttackerImpactCameraShakeClass : nullptr;
}

TSubclassOf<UCameraShakeBase> APlayerCharacter::ResolveExecutionImpactCameraShakeClass()
{
	const UPlayerCombatFeedbackDataAsset* FeedbackData = GetPlayerCombatFeedbackData();
	if (!FeedbackData)
	{
		return nullptr;
	}

	return FeedbackData->Execution.AttackerImpactCameraShakeClass;
}

void APlayerCharacter::StartHitFeedbackCameraShakeInstance(const TSubclassOf<UCameraShakeBase> ResolvedClass)
{
	if (!ResolvedClass)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;

	if (ActiveHitFeedbackCameraManager.IsValid() && ActiveHitFeedbackCameraShake.IsValid())
	{
		if (ActiveHitFeedbackCameraManager.Get() != CameraManager || ActiveHitFeedbackCameraShakeClass != ResolvedClass)
		{
			ActiveHitFeedbackCameraManager->StopCameraShake(ActiveHitFeedbackCameraShake.Get(), true);
			ActiveHitFeedbackCameraManager = nullptr;
			ActiveHitFeedbackCameraShake = nullptr;
			ActiveHitFeedbackCameraShakeClass = nullptr;
		}
	}
	else
	{
		ActiveHitFeedbackCameraManager = nullptr;
		ActiveHitFeedbackCameraShake = nullptr;
		ActiveHitFeedbackCameraShakeClass = nullptr;
	}

	UCameraShakeBase* StartedShake = CameraManager->StartCameraShake(ResolvedClass, 1.0f);
	if (StartedShake)
	{
		ActiveHitFeedbackCameraManager = CameraManager;
		ActiveHitFeedbackCameraShake = StartedShake;
		ActiveHitFeedbackCameraShakeClass = ResolvedClass;
#if WITH_DEV_AUTOMATION_TESTS
		++TestHitFeedbackCameraShakeStartCount;
		TestLastHitFeedbackCameraShake = StartedShake;
#endif
	}
}

void APlayerCharacter::TriggerHitFeedbackCameraShake(const EHitReactionTier ReactionTier)
{
	const TSubclassOf<UCameraShakeBase> ResolvedClass = ResolveHitFeedbackCameraShakeClass(ReactionTier);
	StartHitFeedbackCameraShakeInstance(ResolvedClass);
}

void APlayerCharacter::ClearActiveHitFeedbackCameraShake()
{
	if (ActiveHitFeedbackCameraManager.IsValid() && ActiveHitFeedbackCameraShake.IsValid())
	{
		ActiveHitFeedbackCameraManager->StopCameraShake(ActiveHitFeedbackCameraShake.Get(), true);
	}

	ActiveHitFeedbackCameraManager = nullptr;
	ActiveHitFeedbackCameraShake = nullptr;
	ActiveHitFeedbackCameraShakeClass = nullptr;
}

void APlayerCharacter::TriggerCameraFovPunch(const float PunchDegrees)
{
	if (!IsLocallyControlled() || PunchDegrees <= 0.0f)
	{
		return;
	}

	if (!FovPunchModifier.IsValid())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				UCameraModifier* ExistingMod = PC->PlayerCameraManager->FindCameraModifierByClass(UCameraModifier_FovPunch::StaticClass());
				if (!ExistingMod)
				{
					ExistingMod = PC->PlayerCameraManager->AddNewCameraModifier(UCameraModifier_FovPunch::StaticClass());
				}
				FovPunchModifier = Cast<UCameraModifier_FovPunch>(ExistingMod);
			}
		}
	}

	if (FovPunchModifier.IsValid())
	{
		FovPunchModifier->TriggerPunch(PunchDegrees);
	}
}

void APlayerCharacter::TriggerReceivedHitSound(const FGameplayEffectSpec& EffectSpec)
{
	const UPlayerCombatFeedbackDataAsset* FeedbackData = GetPlayerCombatFeedbackData();
	USoundBase* SoundToPlay = FeedbackData ? FeedbackData->ReceivedHitSound.Get() : nullptr;
	if (!SoundToPlay)
	{
		return;
	}

	const FGameplayEffectContextHandle ContextHandle = EffectSpec.GetContext();
	AActor* InstigatorActor = ContextHandle.GetInstigator();
	if (!InstigatorActor || !InstigatorActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass()))
	{
		return;
	}

	const FGameplayTag InstigatorTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(InstigatorActor);
	static const FGameplayTag EnemyTeamTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	if (!InstigatorTeamTag.IsValid() || !EnemyTeamTag.IsValid() || !InstigatorTeamTag.MatchesTagExact(EnemyTeamTag))
	{
		return;
	}

	FVector SoundLocation = GetActorLocation();
	const FHitResult* ContextHitResult = ContextHandle.GetHitResult();
	if (ContextHitResult
		&& ContextHitResult->GetActor() == this
		&& !ContextHitResult->ImpactPoint.ContainsNaN()
		&& FMath::IsFinite(ContextHitResult->ImpactPoint.X) && FMath::IsFinite(ContextHitResult->ImpactPoint.Y) && FMath::IsFinite(ContextHitResult->ImpactPoint.Z)
		&& !ContextHitResult->ImpactPoint.IsNearlyZero())
	{
		SoundLocation = ContextHitResult->ImpactPoint;
	}

	if (SoundLocation.ContainsNaN()
		|| !FMath::IsFinite(SoundLocation.X) || !FMath::IsFinite(SoundLocation.Y) || !FMath::IsFinite(SoundLocation.Z))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	++TestReceivedHitSoundDispatchCount;
	TestLastReceivedHitSoundLocation = SoundLocation;
	if (bTestBypassReceivedHitAudioPlayback)
	{
		return;
	}
#endif

	UGameplayStatics::PlaySoundAtLocation(World, SoundToPlay, SoundLocation);
}

void APlayerCharacter::OnSprintRelevantTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UpdateActionFacingRotationMode();

	if (NewCount > 0)
	{
		if (Tag == DeadStateTag)
		{
			ClearGuardResumeEligibility();
			ClearWorldPickupInteractionState();
		}
		else if (Tag == StunnedStateTag)
		{
			ClearGuardResumeEligibility();
		}

		// Guard and Parry own their Sprint cancellation after their own Montages have actually started.
		if (Tag == GuardingStateTag || Tag == ParryingStateTag)
		{
			return;
		}

		CancelSprintAbility();
		return;
	}

	if (Tag == DeadStateTag)
	{
		SeedWorldPickupCandidates();
	}

	if ((Tag == AttackingStateTag || Tag == ParryingStateTag) && bGuardResumeEligibleAfterAttack)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &APlayerCharacter::ResumeGuardAfterAttack);
		}
		else
		{
			ClearGuardResumeEligibility();
		}
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

#if WITH_DEV_AUTOMATION_TESTS
void APlayerCharacter::TriggerTestTargetCycle(const float InAxisValue)
{
	HandleTargetCycleTriggered(FInputActionValue(InAxisValue));
}

bool APlayerCharacter::HasTestMeleeMotionWarpTarget(FName WarpTargetName, FTransform* OutTransform) const
{
	if (!MotionWarpingComponent || WarpTargetName.IsNone())
	{
		return false;
	}

	if (const FMotionWarpingTarget* Target = MotionWarpingComponent->FindWarpTarget(WarpTargetName))
	{
		if (OutTransform)
		{
			*OutTransform = FTransform(Target->Rotation, Target->Location);
		}
		return true;
	}

	return false;
}

int32 APlayerCharacter::GetTestMeleeMotionWarpTargetCount() const
{
	return MotionWarpingComponent ? MotionWarpingComponent->GetWarpTargets().Num() : 0;
}
#endif

void APlayerCharacter::RegisterWorldPickupCandidate(AWorldWeaponPickup* Pickup)
{
	if (Pickup)
	{
		WorldPickupCandidates.Add(Pickup);
		RefreshWorldPickupInteractionPrompt();
	}
}

void APlayerCharacter::UnregisterWorldPickupCandidate(AWorldWeaponPickup* Pickup)
{
	if (Pickup)
	{
		WorldPickupCandidates.Remove(Pickup);
		RefreshWorldPickupInteractionPrompt();
	}
}

void APlayerCharacter::ClearWorldPickupInteractionState()
{
	WorldPickupCandidates.Empty();
	CurrentWorldPickupCandidate.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FormerOwnerInteractionRefreshTimerHandle);
	}
	if (APolyQuestPlayerController* PC = Cast<APolyQuestPlayerController>(GetController()))
	{
		PC->HideInteractionPrompt();
	}
}

void APlayerCharacter::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (WorldPickupCandidates.IsEmpty())
	{
		return;
	}

	if (!GetActorLocation().Equals(OldLocation, 0.1f))
	{
		RefreshWorldPickupInteractionPrompt();
	}
}

void APlayerCharacter::SeedWorldPickupCandidates()
{
	WorldPickupCandidates.Empty();
	TArray<AActor*> OverlappingPickups;
	GetOverlappingActors(OverlappingPickups, AWorldWeaponPickup::StaticClass());
	for (AActor* Actor : OverlappingPickups)
	{
		if (AWorldWeaponPickup* Pickup = Cast<AWorldWeaponPickup>(Actor))
		{
			WorldPickupCandidates.Add(Pickup);
		}
	}
	RefreshWorldPickupInteractionPrompt();
}

void APlayerCharacter::RefreshWorldPickupInteractionPrompt()
{
	UWorld* World = GetWorld();
	if (!World || IsActorBeingDestroyed())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(FormerOwnerInteractionRefreshTimerHandle);

	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (CharacterASC && DeadStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DeadStateTag))
	{
		CurrentWorldPickupCandidate.Reset();
		if (APolyQuestPlayerController* PC = Cast<APolyQuestPlayerController>(GetController()))
		{
			PC->HideInteractionPrompt();
		}
		return;
	}

	for (auto It = WorldPickupCandidates.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || (*It)->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
		}
	}

	AWorldWeaponPickup* BestCandidate = nullptr;
	float BestDistanceSq = MAX_flt;
	float EarliestRemainingCooldown = MAX_flt;
	const FVector PlayerLocation = GetActorLocation();

	for (const TWeakObjectPtr<AWorldWeaponPickup>& WeakPickup : WorldPickupCandidates)
	{
		AWorldWeaponPickup* Pickup = WeakPickup.Get();
		if (!Pickup)
		{
			continue;
		}

		if (Pickup->CanInteract(this))
		{
			const float DistSq = FVector::DistSquared(PlayerLocation, Pickup->GetActorLocation());
			if (DistSq < BestDistanceSq)
			{
				BestDistanceSq = DistSq;
				BestCandidate = Pickup;
			}
			else if (FMath::IsNearlyEqual(DistSq, BestDistanceSq, KINDA_SMALL_NUMBER) && BestCandidate)
			{
				if (Pickup->GetName() < BestCandidate->GetName())
				{
					BestCandidate = Pickup;
				}
			}
		}
		else
		{
			const float RemainingCooldown = Pickup->GetFormerOwnerRemainingTime(this);
			if (RemainingCooldown > 0.0f && RemainingCooldown < EarliestRemainingCooldown)
			{
				EarliestRemainingCooldown = RemainingCooldown;
			}
		}
	}

	CurrentWorldPickupCandidate = BestCandidate;

	APolyQuestPlayerController* PC = Cast<APolyQuestPlayerController>(GetController());
	if (BestCandidate)
	{
		if (PC)
		{
			const UWeaponDefinition* WeaponDef = BestCandidate->GetWeaponDefinition();
			FText PromptText;
			if (WeaponDef && !WeaponDef->InteractionDisplayName.IsEmpty())
			{
				PromptText = FText::Format(
					NSLOCTEXT("PolyQuest", "PromptWithWeapon", "拾取 {0}"),
					WeaponDef->InteractionDisplayName);
			}
			else
			{
				PromptText = NSLOCTEXT("PolyQuest", "PromptFallback", "拾取");
			}

			PC->ShowInteractionPrompt(PromptText);
		}
	}
	else
	{
		if (PC)
		{
			PC->HideInteractionPrompt();
		}
	}

	if (EarliestRemainingCooldown < MAX_flt && EarliestRemainingCooldown > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			FormerOwnerInteractionRefreshTimerHandle,
			this,
			&APlayerCharacter::RefreshWorldPickupInteractionPrompt,
			EarliestRemainingCooldown,
			false);
	}
}
