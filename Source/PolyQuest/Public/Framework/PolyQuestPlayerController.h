// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PolyQuestPlayerController.generated.h"

class UAbilitySystemComponent;
class UInputMappingContext;
class UPlayerVitalHUDWidget;
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

#if WITH_DEV_AUTOMATION_TESTS
	UPlayerVitalHUDWidget* GetTestPlayerVitalHUDInstance() const { return PlayerVitalHUDInstance; }
	void SetTestPlayerVitalHUDInstance(UPlayerVitalHUDWidget* InInstance) { PlayerVitalHUDInstance = InInstance; }
	void SetTestPlayerVitalHUDClass(TSubclassOf<UPlayerVitalHUDWidget> InClass) { PlayerVitalHUDClass = InClass; }
	UAbilitySystemComponent* GetTestBoundAbilitySystemComponent() const;
	bool HasBoundAttributeDelegates() const
	{
		return HealthChangedHandle.IsValid()
			&& MaxHealthChangedHandle.IsValid()
			&& StaminaChangedHandle.IsValid()
			&& MaxStaminaChangedHandle.IsValid();
	}
	void TriggerTestEnsureHUDCreated() { EnsureHUDCreated(); }
	void TriggerTestBindToPawn(APawn* InPawn) { BindToPawn(InPawn); }
	void TriggerTestUnbindCurrentPawn() { UnbindCurrentPawn(); }
	void TriggerTestRefreshVitalHUD() { RefreshVitalHUD(); }
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

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

private:
	void EnsureHUDCreated();
	void BindToPawn(APawn* InPawn);
	void UnbindCurrentPawn();
	void RefreshVitalHUD();
	void OnAttributeChanged(const FOnAttributeChangeData& ChangeData);

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;

	TWeakObjectPtr<APlayerCharacter> BoundPlayerCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	bool bHasLoggedMissingHUDClass = false;
};
