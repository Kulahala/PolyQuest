// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerVitalHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;
class UImage;

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

	/** Triggers a micro-shake jolt on the health bar (e.g. on hit impact). */
	UFUNCTION(BlueprintCallable, Category = "Vital|Shake")
	void PlayHealthShake();

	/** Triggers a micro-shake rejection nudge on the stamina bar (e.g. when stamina is depleted or action rejected). */
	UFUNCTION(BlueprintCallable, Category = "Vital|Shake")
	void PlayStaminaRejectionShake();

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestHealthBufferProgressBar(UProgressBar* InBar);
	void SetTestHealthBarOverlay(UWidget* InWidget);
	void SetTestStaminaWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestStaminaExhaustedOverlay(UWidget* InWidget);
	void SetTestStaminaBarOverlay(UWidget* InWidget);
	void SetTestLowHealthVignetteImage(UImage* InImage);
	UProgressBar* GetTestHealthProgressBar() const;
	UProgressBar* GetTestHealthBufferProgressBar() const;
	UWidget* GetTestHealthBarOverlay() const;
	UTextBlock* GetTestHealthCurrentText() const;
	UTextBlock* GetTestHealthMaxText() const;
	UProgressBar* GetTestStaminaProgressBar() const;
	UTextBlock* GetTestStaminaCurrentText() const;
	UTextBlock* GetTestStaminaMaxText() const;
	UWidget* GetTestStaminaExhaustedOverlay() const;
	UWidget* GetTestStaminaBarOverlay() const;
	UImage* GetTestLowHealthVignetteImage() const;
	bool IsTestExhaustedOverlayVisible() const;
	bool IsTestLowHealthVignetteVisible() const;
	float GetTestBufferDelayTimer() const;
	float GetTestCurrentBufferPercent() const;
	float GetTestCurrentVignetteAlpha() const;
	float GetTestLowHealthPulseWeight() const;
	float GetTestDamageFlashTimer() const;
	float GetTestHealthShakeTimer() const;
	float GetTestStaminaShakeTimer() const;
	float GetTestHealthTranslationY() const;
	float GetTestStaminaTranslationY() const;
	void SimulateTickForTesting(float InDeltaTime);
#endif

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateBufferHealth(float InDeltaTime);
	void UpdateVignette(float InDeltaTime);
	void UpdateShake(float InDeltaTime);
	float CalculateShakeOffset(float RemainingTimer) const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HealthBarOverlay;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StaminaBarOverlay;

	/** Max vertical displacement in pixels for hit and rejection micro-shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "0.0"))
	float ShakeMaxDisplacement = 4.0f;

	/** Duration in seconds for micro-shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "0.01"))
	float ShakeDuration = 0.16f;

	/** Frequency of oscillation for micro-shake in Hz. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "1.0"))
	float ShakeFrequency = 25.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LowHealthVignetteImage;

	/** Health percentage threshold to trigger low health vignette pulse (0.0 - 1.0). */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthThreshold = 0.25f;

	/** Oscillation period in seconds for low health pulse. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.1"))
	float LowHealthPulsePeriod = 1.1f;

	/** Minimum opacity of vignette during pulse oscillation. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthPulseMinAlpha = 0.08f;

	/** Maximum opacity of vignette during pulse oscillation. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthPulseMaxAlpha = 0.40f;

	/** Duration in seconds to fade out low health pulse when health recovers above threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.05"))
	float LowHealthFadeOutDuration = 0.5f;

	/** Duration in seconds of damage hit flash vignette. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.01"))
	float DamageFlashDuration = 0.14f;

	/** Peak opacity of damage hit flash vignette. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageFlashMaxAlpha = 0.40f;

	/** Global clamp ceiling for combined vignette alpha to ensure center vision stays clear. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxAllowedVignetteAlpha = 0.45f;

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
	float LowHealthPulseWeight = 0.0f;
	float LowHealthPulseTimer = 0.0f;
	float DamageFlashTimer = 0.0f;
	float CurrentVignetteAlpha = 0.0f;
	float HealthShakeTimer = 0.0f;
	float StaminaShakeTimer = 0.0f;
	bool bIsHealthInitialized = false;
	bool bWasExhausted = false;
};
