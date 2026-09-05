// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "PolyQuestPlayerController.generated.h"

class UAbilitySystemComponent;
class UInputMappingContext;
class UPlayerVitalHUDWidget;
class UWorldInteractionPromptWidget;
class APlayerCharacter;
struct FOnAttributeChangeData;

/**
 * Product PlayerController root that installs desktop Enhanced Input mappings
 * and owns local player vital HUD creation and ASC attribute delegate lifecycle.
 */
UCLASS()
class POLYQUEST_API APolyQuestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APolyQuestPlayerController();

	/**
	 * Requests a controller-owned combat impact hit-stop with monotonic overlap arbitration.
	 * Time dilation restoration is scheduled against unscaled real-time seconds.
	 */
	void RequestCombatImpactHitStop(float DurationSeconds, float TimeDilation);

	/** Shows the interaction prompt with the given text; creates the prompt widget idempotently. */
	void ShowInteractionPrompt(const FText& InText);

	/** Hides the interaction prompt without destroying the widget. */
	void HideInteractionPrompt();

#if WITH_DEV_AUTOMATION_TESTS
	UPlayerVitalHUDWidget* GetTestPlayerVitalHUDInstance() const { return PlayerVitalHUDInstance; }
	void SetTestPlayerVitalHUDInstance(UPlayerVitalHUDWidget* InInstance) { PlayerVitalHUDInstance = InInstance; }
	void SetTestPlayerVitalHUDClass(TSubclassOf<UPlayerVitalHUDWidget> InClass) { PlayerVitalHUDClass = InClass; }
	void SetTestInteractionPromptClass(TSubclassOf<UWorldInteractionPromptWidget> InClass) { InteractionPromptClass = InClass; }
	UWorldInteractionPromptWidget* GetTestInteractionPromptInstance() const { return InteractionPromptInstance.Get(); }
	void SetTestInteractionPromptInstance(UWorldInteractionPromptWidget* InInstance) { InteractionPromptInstance = InInstance; }
	void TriggerTestEnsureInteractionPromptCreated() { EnsureInteractionPromptCreated(); }
	void TriggerTestShowInteractionPrompt(const FText& InText) { ShowInteractionPrompt(InText); }
	void TriggerTestHideInteractionPrompt() { HideInteractionPrompt(); }
	UAbilitySystemComponent* GetTestBoundAbilitySystemComponent() const;
	bool HasBoundAttributeDelegates() const
	{
		return HealthChangedHandle.IsValid()
			&& MaxHealthChangedHandle.IsValid()
			&& StaminaChangedHandle.IsValid()
			&& MaxStaminaChangedHandle.IsValid();
	}
	bool HasBoundExhaustedTagDelegate() const { return ExhaustedTagChangedHandle.IsValid(); }
	void TriggerTestEnsureHUDCreated() { EnsureHUDCreated(); }
	void TriggerTestBindToPawn(APawn* InPawn) { BindToPawn(InPawn); }
	void TriggerTestUnbindCurrentPawn() { UnbindCurrentPawn(); }
	void TriggerTestRefreshVitalHUD() { RefreshVitalHUD(); }
	bool IsTestHitStopActive() const { return bHitStopActive; }
	float GetTestPreHitStopGlobalTimeDilation() const { return PreHitStopGlobalTimeDilation; }
	float GetTestCurrentAppliedTimeDilation() const { return CurrentAppliedTimeDilation; }
	double GetTestHitStopExpireRealTimeSeconds() const { return HitStopExpireRealTimeSeconds; }
	void TriggerTestRestoreCombatImpactHitStop() { RestoreCombatImpactHitStop(); }
#endif

protected:
	/** Mapping contexts installed for local desktop players. */
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Configured Player Vital HUD Widget class for local player display. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPlayerVitalHUDWidget> PlayerVitalHUDClass;

	/** Created player vital HUD instance in the viewport. */
	UPROPERTY(Transient)
	TObjectPtr<UPlayerVitalHUDWidget> PlayerVitalHUDInstance;

	/** Configured Interaction Prompt Widget class for local player display. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UWorldInteractionPromptWidget> InteractionPromptClass;

	/** Created interaction prompt instance in the viewport. */
	UPROPERTY(Transient)
	TObjectPtr<UWorldInteractionPromptWidget> InteractionPromptInstance;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void BeginDestroy() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

private:
	void EnsureHUDCreated();
	void EnsureInteractionPromptCreated();
	void BindToPawn(APawn* InPawn);
	void UnbindCurrentPawn();
	void RefreshVitalHUD();
	void OnAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void OnExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount);
	void UpdateCombatImpactHitStop();
	void RestoreCombatImpactHitStop();

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
	FDelegateHandle ExhaustedTagChangedHandle;

	TWeakObjectPtr<APlayerCharacter> BoundPlayerCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	bool bHasLoggedMissingHUDClass = false;
	bool bHasLoggedMissingInteractionPromptClass = false;

	bool bHitStopActive = false;
	float PreHitStopGlobalTimeDilation = 1.0f;
	float CurrentAppliedTimeDilation = 1.0f;
	double HitStopExpireRealTimeSeconds = 0.0;
	TWeakObjectPtr<UWorld> CachedCombatImpactWorld;
};
