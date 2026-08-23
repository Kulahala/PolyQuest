// Copyright Epic Games, Inc. All Rights Reserved.


#include "Framework/PolyQuestPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Character/Player/PlayerCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "PolyQuest.h"
#include "UI/PlayerVitalHUDWidget.h"

APolyQuestPlayerController::APolyQuestPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void APolyQuestPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);

		EnsureHUDCreated();
		BindToPawn(GetPawn());
	}
}

void APolyQuestPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsLocalPlayerController())
	{
		EnsureHUDCreated();
		BindToPawn(InPawn);
	}
}

void APolyQuestPlayerController::OnUnPossess()
{
	UnbindCurrentPawn();

	Super::OnUnPossess();
}

void APolyQuestPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCurrentPawn();

	if (PlayerVitalHUDInstance)
	{
		PlayerVitalHUDInstance->RemoveFromParent();
		PlayerVitalHUDInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void APolyQuestPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}
	}
}

void APolyQuestPlayerController::EnsureHUDCreated()
{
#if !WITH_DEV_AUTOMATION_TESTS
	if (!IsLocalPlayerController())
	{
		return;
	}
#endif

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (PlayerVitalHUDInstance)
	{
		return;
	}

	if (!PlayerVitalHUDClass)
	{
		if (!bHasLoggedMissingHUDClass)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("APolyQuestPlayerController: PlayerVitalHUDClass is not configured on '%s'."), *GetNameSafe(this));
			bHasLoggedMissingHUDClass = true;
		}
		return;
	}

	if (IsLocalPlayerController())
	{
		PlayerVitalHUDInstance = CreateWidget<UPlayerVitalHUDWidget>(this, PlayerVitalHUDClass);
	}
#if WITH_DEV_AUTOMATION_TESTS
	else if (GetWorld())
	{
		PlayerVitalHUDInstance = CreateWidget<UPlayerVitalHUDWidget>(GetWorld(), PlayerVitalHUDClass);
	}
#endif

	if (PlayerVitalHUDInstance)
	{
		if (IsLocalPlayerController())
		{
			PlayerVitalHUDInstance->AddToViewport();
		}
	}
	else if (!bHasLoggedMissingHUDClass)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("APolyQuestPlayerController: Failed to create PlayerVitalHUDInstance from class '%s'."), *GetNameSafe(PlayerVitalHUDClass));
		bHasLoggedMissingHUDClass = true;
	}
}

void APolyQuestPlayerController::BindToPawn(APawn* InPawn)
{
	UnbindCurrentPawn();

	if (!InPawn)
	{
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(InPawn);
	if (!PlayerChar)
	{
		return;
	}

	UAbilitySystemComponent* ASC = PlayerChar->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	BoundPlayerCharacter = PlayerChar;
	BoundAbilitySystemComponent = ASC;

	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
		.AddUObject(this, &APolyQuestPlayerController::OnAttributeChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &APolyQuestPlayerController::OnAttributeChanged);
	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &APolyQuestPlayerController::OnAttributeChanged);
	MaxStaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxStaminaAttribute())
		.AddUObject(this, &APolyQuestPlayerController::OnAttributeChanged);

	RefreshVitalHUD();
}

void APolyQuestPlayerController::UnbindCurrentPawn()
{
	if (UAbilitySystemComponent* BoundASC = BoundAbilitySystemComponent.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		}
		if (StaminaChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		}
		if (MaxStaminaChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();

	BoundPlayerCharacter.Reset();
	BoundAbilitySystemComponent.Reset();
}

void APolyQuestPlayerController::RefreshVitalHUD()
{
	if (!PlayerVitalHUDInstance)
	{
		return;
	}

	UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	bool bFoundHealth = false;
	const float CurrentHealth = ASC->GetGameplayAttributeValue(UCharacterAttributeSet::GetHealthAttribute(), bFoundHealth);

	bool bFoundMaxHealth = false;
	const float MaxHealth = ASC->GetGameplayAttributeValue(UCharacterAttributeSet::GetMaxHealthAttribute(), bFoundMaxHealth);

	bool bFoundStamina = false;
	const float CurrentStamina = ASC->GetGameplayAttributeValue(UCharacterAttributeSet::GetStaminaAttribute(), bFoundStamina);

	bool bFoundMaxStamina = false;
	const float MaxStamina = ASC->GetGameplayAttributeValue(UCharacterAttributeSet::GetMaxStaminaAttribute(), bFoundMaxStamina);

	PlayerVitalHUDInstance->SetHealth(bFoundHealth ? CurrentHealth : 0.0f, bFoundMaxHealth ? MaxHealth : 0.0f);
	PlayerVitalHUDInstance->SetStamina(bFoundStamina ? CurrentStamina : 0.0f, bFoundMaxStamina ? MaxStamina : 0.0f);
}

void APolyQuestPlayerController::OnAttributeChanged(const FOnAttributeChangeData&)
{
	RefreshVitalHUD();
}

#if WITH_DEV_AUTOMATION_TESTS
UAbilitySystemComponent* APolyQuestPlayerController::GetTestBoundAbilitySystemComponent() const
{
	return BoundAbilitySystemComponent.Get();
}
#endif
