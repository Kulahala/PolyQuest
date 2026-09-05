#include "Framework/PolyQuestPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Character/Player/PlayerCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PolyQuest.h"
#include "UI/PlayerVitalHUDWidget.h"
#include "UI/WorldInteractionPromptWidget.h"

APolyQuestPlayerController::APolyQuestPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
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
		EnsureInteractionPromptCreated();
		BindToPawn(GetPawn());
	}
}

void APolyQuestPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCombatImpactHitStop();
}

void APolyQuestPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsLocalPlayerController())
	{
		EnsureHUDCreated();
		EnsureInteractionPromptCreated();
		BindToPawn(InPawn);
	}
}

void APolyQuestPlayerController::OnUnPossess()
{
	HideInteractionPrompt();
	UnbindCurrentPawn();

	Super::OnUnPossess();
}

void APolyQuestPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreCombatImpactHitStop();
	UnbindCurrentPawn();

	if (PlayerVitalHUDInstance)
	{
		PlayerVitalHUDInstance->RemoveFromParent();
		PlayerVitalHUDInstance = nullptr;
	}

	if (InteractionPromptInstance)
	{
		InteractionPromptInstance->RemoveFromParent();
		InteractionPromptInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void APolyQuestPlayerController::Destroyed()
{
	RestoreCombatImpactHitStop();
	Super::Destroyed();
}

void APolyQuestPlayerController::BeginDestroy()
{
	RestoreCombatImpactHitStop();
	Super::BeginDestroy();
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

void APolyQuestPlayerController::EnsureInteractionPromptCreated()
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

	if (InteractionPromptInstance)
	{
		return;
	}

	if (!InteractionPromptClass)
	{
		if (!bHasLoggedMissingInteractionPromptClass)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("APolyQuestPlayerController: InteractionPromptClass is not configured on '%s'."), *GetNameSafe(this));
			bHasLoggedMissingInteractionPromptClass = true;
		}
		return;
	}

	if (IsLocalPlayerController())
	{
		InteractionPromptInstance = CreateWidget<UWorldInteractionPromptWidget>(this, InteractionPromptClass);
	}
#if WITH_DEV_AUTOMATION_TESTS
	else if (GetWorld())
	{
		InteractionPromptInstance = CreateWidget<UWorldInteractionPromptWidget>(GetWorld(), InteractionPromptClass);
	}
#endif

	if (InteractionPromptInstance)
	{
		InteractionPromptInstance->SetVisibility(ESlateVisibility::Collapsed);
		if (IsLocalPlayerController())
		{
			InteractionPromptInstance->AddToViewport(0);
		}
	}
	else if (!bHasLoggedMissingInteractionPromptClass)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("APolyQuestPlayerController: Failed to create InteractionPromptInstance from class '%s'."), *GetNameSafe(InteractionPromptClass));
		bHasLoggedMissingInteractionPromptClass = true;
	}
}

void APolyQuestPlayerController::ShowInteractionPrompt(const FText& InText)
{
	EnsureInteractionPromptCreated();
	if (InteractionPromptInstance)
	{
		InteractionPromptInstance->SetPromptText(InText);
		InteractionPromptInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void APolyQuestPlayerController::HideInteractionPrompt()
{
	if (InteractionPromptInstance)
	{
		InteractionPromptInstance->SetVisibility(ESlateVisibility::Collapsed);
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

	const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	if (ExhaustedTag.IsValid())
	{
		ExhaustedTagChangedHandle = ASC->RegisterGameplayTagEvent(ExhaustedTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &APolyQuestPlayerController::OnExhaustedTagChanged);
	}

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
		const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
		if (ExhaustedTagChangedHandle.IsValid() && ExhaustedTag.IsValid())
		{
			BoundASC->RegisterGameplayTagEvent(ExhaustedTag, EGameplayTagEventType::NewOrRemoved).Remove(ExhaustedTagChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
	ExhaustedTagChangedHandle.Reset();

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

	const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const bool bIsExhausted = ExhaustedTag.IsValid() && ASC->HasMatchingGameplayTag(ExhaustedTag);

	PlayerVitalHUDInstance->SetHealth(bFoundHealth ? CurrentHealth : 0.0f, bFoundMaxHealth ? MaxHealth : 0.0f);
	PlayerVitalHUDInstance->SetStamina(bFoundStamina ? CurrentStamina : 0.0f, bFoundMaxStamina ? MaxStamina : 0.0f);
	PlayerVitalHUDInstance->SetExhausted(bIsExhausted);
}

void APolyQuestPlayerController::OnAttributeChanged(const FOnAttributeChangeData&)
{
	RefreshVitalHUD();
}

void APolyQuestPlayerController::OnExhaustedTagChanged(const FGameplayTag, const int32 NewCount)
{
	if (PlayerVitalHUDInstance)
	{
		PlayerVitalHUDInstance->SetExhausted(NewCount > 0);
	}
}

void APolyQuestPlayerController::RequestCombatImpactHitStop(float DurationSeconds, float TimeDilation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CachedCombatImpactWorld = World;

	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f
		|| !FMath::IsFinite(TimeDilation) || TimeDilation <= 0.0f || TimeDilation > 1.0f)
	{
		return;
	}

	const double CurrentRealTime = World->GetRealTimeSeconds();
	const float LiveGlobalDilation = UGameplayStatics::GetGlobalTimeDilation(World);

	if (!bHitStopActive)
	{
		PreHitStopGlobalTimeDilation = LiveGlobalDilation;
		CurrentAppliedTimeDilation = TimeDilation;
		HitStopExpireRealTimeSeconds = CurrentRealTime + static_cast<double>(DurationSeconds);
		bHitStopActive = true;

		UGameplayStatics::SetGlobalTimeDilation(World, TimeDilation);
		CurrentAppliedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		return;
	}

	// If another system modified global dilation away from our recorded applied value,
	// relinquish the old request without writing a restore and capture the new baseline.
	if (!FMath::IsNearlyEqual(LiveGlobalDilation, CurrentAppliedTimeDilation, KINDA_SMALL_NUMBER))
	{
		PreHitStopGlobalTimeDilation = LiveGlobalDilation;
		CurrentAppliedTimeDilation = TimeDilation;
		HitStopExpireRealTimeSeconds = CurrentRealTime + static_cast<double>(DurationSeconds);
		bHitStopActive = true;

		UGameplayStatics::SetGlobalTimeDilation(World, TimeDilation);
		CurrentAppliedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		return;
	}

	// Monotonic arbitration while active: lower dilation wins, later real-time expiry wins.
	const float TargetDilation = FMath::Min(CurrentAppliedTimeDilation, TimeDilation);
	const double TargetExpiry = FMath::Max(HitStopExpireRealTimeSeconds, CurrentRealTime + static_cast<double>(DurationSeconds));

	CurrentAppliedTimeDilation = TargetDilation;
	HitStopExpireRealTimeSeconds = TargetExpiry;

	UGameplayStatics::SetGlobalTimeDilation(World, TargetDilation);
	CurrentAppliedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
}

void APolyQuestPlayerController::UpdateCombatImpactHitStop()
{
	if (!bHitStopActive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		bHitStopActive = false;
		return;
	}

	const float LiveGlobalDilation = UGameplayStatics::GetGlobalTimeDilation(World);
	if (!FMath::IsNearlyEqual(LiveGlobalDilation, CurrentAppliedTimeDilation, KINDA_SMALL_NUMBER))
	{
		bHitStopActive = false;
		PreHitStopGlobalTimeDilation = 1.0f;
		CurrentAppliedTimeDilation = 1.0f;
		HitStopExpireRealTimeSeconds = 0.0;
		return;
	}

	const double CurrentRealTime = World->GetRealTimeSeconds();
	if (CurrentRealTime >= HitStopExpireRealTimeSeconds)
	{
		RestoreCombatImpactHitStop();
	}
}

void APolyQuestPlayerController::RestoreCombatImpactHitStop()
{
	if (!bHitStopActive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		World = CachedCombatImpactWorld.Get();
	}

	if (World)
	{
		const float LiveGlobalDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		if (FMath::IsNearlyEqual(LiveGlobalDilation, CurrentAppliedTimeDilation, KINDA_SMALL_NUMBER))
		{
			UGameplayStatics::SetGlobalTimeDilation(World, PreHitStopGlobalTimeDilation);
		}
	}

	bHitStopActive = false;
	PreHitStopGlobalTimeDilation = 1.0f;
	CurrentAppliedTimeDilation = 1.0f;
	HitStopExpireRealTimeSeconds = 0.0;
	CachedCombatImpactWorld.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
UAbilitySystemComponent* APolyQuestPlayerController::GetTestBoundAbilitySystemComponent() const
{
	return BoundAbilitySystemComponent.Get();
}
#endif
