// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerVitalHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * Native C++ base widget for the player vital HUD (Health and Stamina).
 * It provides pure display setters without any GAS, gameplay, tick, or input logic.
 */
UCLASS(Blueprintable)
class POLYQUEST_API UPlayerVitalHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates displayed health progress bar and numeric text. */
	void SetHealth(float Current, float Max);

	/** Updates displayed stamina progress bar and numeric text. */
	void SetStamina(float Current, float Max);

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestStaminaWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	UProgressBar* GetTestHealthProgressBar() const;
	UTextBlock* GetTestHealthCurrentText() const;
	UTextBlock* GetTestHealthMaxText() const;
	UProgressBar* GetTestStaminaProgressBar() const;
	UTextBlock* GetTestStaminaCurrentText() const;
	UTextBlock* GetTestStaminaMaxText() const;
#endif

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthCurrentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthMaxText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StaminaCurrentText;

	UPROPERTY(meta = (BindWidget))
TObjectPtr<UTextBlock> StaminaMaxText;
};
