// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerVitalHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;

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

	/** Updates displayed stamina exhaustion overlay visibility. */
	void SetExhausted(bool bIsExhausted);

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestHealthBufferProgressBar(UProgressBar* InBar);
	void SetTestStaminaWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestStaminaExhaustedOverlay(UWidget* InWidget);
	UProgressBar* GetTestHealthProgressBar() const;
	UProgressBar* GetTestHealthBufferProgressBar() const;
	UTextBlock* GetTestHealthCurrentText() const;
	UTextBlock* GetTestHealthMaxText() const;
	UProgressBar* GetTestStaminaProgressBar() const;
	UTextBlock* GetTestStaminaCurrentText() const;
	UTextBlock* GetTestStaminaMaxText() const;
	UWidget* GetTestStaminaExhaustedOverlay() const;
	bool IsTestExhaustedOverlayVisible() const;
	float GetTestBufferDelayTimer() const;
	float GetTestCurrentBufferPercent() const;
	void SimulateTickForTesting(float InDeltaTime);
#endif

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateBufferHealth(float InDeltaTime);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBufferProgressBar;

	/** Delay in seconds before buffer health starts catching up after damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer", meta = (ClampMin = "0.0"))
	float BufferCatchUpDelay = 0.5f;

	/** Speed of buffer catch up interpolation. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer", meta = (ClampMin = "0.1"))
	float BufferCatchUpSpeed = 4.0f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthCurrentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthMaxText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StaminaExhaustedOverlay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StaminaCurrentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StaminaMaxText;

private:
	float TargetHealthPercent = 0.0f;
	float CurrentBufferPercent = 0.0f;
	float BufferDelayTimer = 0.0f;
	bool bIsHealthInitialized = false;
};
